#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_CallFunction.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_BreakStruct.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Actor.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

// One-shot editor migration. Gameplay remains in the saved Blueprint graphs.
namespace BackroomGoalHandoff
{
UEdGraph* Graph(UBlueprint* BP,const TCHAR* Name)
{for(UEdGraph* G:BP->FunctionGraphs)if(G->GetFName()==Name)return G;return nullptr;}
UK2Node_FunctionEntry* Entry(UEdGraph* G)
{check(G);for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N))return E;return nullptr;}
UK2Node_CallFunction* Call(UEdGraph* G,const TCHAR* Name)
{for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N);C&&C->FunctionReference.GetMemberName()==Name)return C;return nullptr;}
UK2Node_VariableSet* Set(UEdGraph* G,const TCHAR* Name)
{for(UEdGraphNode* N:G->Nodes)if(auto* C=Cast<UK2Node_VariableSet>(N);C&&C->VariableReference.GetMemberName()==Name)return C;return nullptr;}
UEdGraphPin* Pin(UEdGraphNode* N,const TCHAR* Name){check(N);return N->FindPinChecked(Name);}
void Link(UEdGraphPin* A,UEdGraphPin* B)
{check(A&&B);check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
template<class T>T* New(UEdGraph* G,int32 X=0,int32 Y=0)
{auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
template<class T>T* Variable(UEdGraph* G,const TCHAR* Name,int32 X=0,int32 Y=0)
{
 auto* N=New<T>(G,X,Y);
 if(auto* L=Entry(G)->LocalVariables.FindByPredicate([&](const auto& V){return V.VarName==Name;}))
  N->VariableReference.SetLocalMember(Name,G->GetName(),L->VarGuid);
 else N->VariableReference.SetSelfMember(Name);
 N->AllocateDefaultPins();return N;
}
void Compile(UBlueprint* BP)
{
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
}
void MatchGridSize(UEdGraph* G)
{
 auto OldNodes=G->Nodes;
 for(UEdGraphNode* N:OldNodes)if(auto* V=Cast<UK2Node_VariableGet>(N);V&&V->VariableReference.GetMemberName()==TEXT("GridSize"))
 {
  auto Links=Pin(V,TEXT("GridSize"))->LinkedTo;Pin(V,TEXT("GridSize"))->BreakAllPinLinks();
  auto* Replacement=Variable<UK2Node_VariableGet>(G,TEXT("Grid_size"),V->NodePosX,V->NodePosY);
  for(auto* P:Links)Link(Pin(Replacement,TEXT("Grid_size")),P);
  V->BreakAllNodeLinks();G->RemoveNode(V);
 }
}
void Save(UBlueprint* BP,FString& Report)
{
 FCompilerResultsLog Log;
 FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
 Report+=FString::Printf(TEXT("%s errors=%d warnings=%d\n"),*BP->GetName(),Log.NumErrors,Log.NumWarnings);
 for(auto& M:Log.Messages)Report+=M->ToText().ToString()+TEXT("\n");
 if(Log.NumErrors)return;
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 const bool OK=UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 Report+=FString::Printf(TEXT("saved=%d\n"),OK);
}
void RepairInput()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer"));check(BP);
 auto* G=Graph(BP,TEXT("RenderBlueprintLayout"));check(G);
 for(UEdGraphNode* N:G->Nodes)
  if(auto* S=Cast<UK2Node_VariableSet>(N);S&&S->VariableReference.GetMemberName()==TEXT("PlannedGoalLocation"))
  {Pin(S,TEXT("PlannedGoalLocation"))->BreakAllPinLinks();Link(Pin(Entry(G),TEXT("GoalBaseLocation")),Pin(S,TEXT("PlannedGoalLocation")));}
 Compile(BP);FString Report;Save(BP,Report);
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("BackroomGoalHandoff/input_result.txt")));
}
void ProtectPillarClearance()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer"));check(BP);
 auto* G=Graph(BP,TEXT("SpawnRoomPillars"));check(G);
 UK2Node_MacroInstance* Loop=nullptr;
 for(UEdGraphNode* N:G->Nodes)if(auto* M=Cast<UK2Node_MacroInstance>(N);M&&M->GetMacroGraph()&&M->GetMacroGraph()->GetFName()==TEXT("ForEachLoop"))Loop=M;
 check(Loop);
 // This guard is inserted before any pillar Actor or collision is created.
 auto MakeCall=[&](UClass* Owner,const TCHAR* Name,int32 X,int32 Y)
 {auto* C=New<UK2Node_CallFunction>(G,X,Y);C->SetFromFunction(Owner->FindFunctionByName(Name));C->AllocateDefaultPins();return C;};
 auto* Math=UKismetMathLibrary::StaticClass();
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();
 auto* Cell=New<UK2Node_BreakStruct>(G,Loop->NodePosX,Loop->NodePosY+600);Cell->StructType=TBaseStructure<FIntPoint>::Get();Cell->AllocateDefaultPins();
 Link(Pin(Loop,TEXT("Array Element")),Pin(Cell,TEXT("IntPoint")));
 auto* Size=Variable<UK2Node_VariableGet>(G,TEXT("Grid_size"),Cell->NodePosX,Cell->NodePosY+240);
 auto Coordinate=[&](const TCHAR* Axis,int32 Y)
 {
  auto* Add=MakeCall(Math,TEXT("Add_DoubleDouble"),Cell->NodePosX+250,Y);
  Link(Pin(Cell,Axis),Pin(Add,TEXT("A")));Schema->TrySetDefaultValue(*Pin(Add,TEXT("B")),TEXT("0.5"));
  auto* Mul=MakeCall(Math,TEXT("Multiply_DoubleDouble"),Cell->NodePosX+500,Y);
  Link(Pin(Add,TEXT("ReturnValue")),Pin(Mul,TEXT("A")));Link(Pin(Size,TEXT("Grid_size")),Pin(Mul,TEXT("B")));return Mul;
 };
 auto* X=Coordinate(TEXT("X"),Cell->NodePosY);auto* Y=Coordinate(TEXT("Y"),Cell->NodePosY+150);
 auto* Position=MakeCall(Math,TEXT("MakeVector"),Cell->NodePosX+750,Cell->NodePosY);
 Link(Pin(X,TEXT("ReturnValue")),Pin(Position,TEXT("X")));Link(Pin(Y,TEXT("ReturnValue")),Pin(Position,TEXT("Y")));
 auto* Goal=Variable<UK2Node_VariableGet>(G,TEXT("PlannedGoalLocation"),Position->NodePosX,Position->NodePosY+200);
 auto* Player=MakeCall(UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"),Position->NodePosX,Position->NodePosY+400);
 auto* PlayerPosition=MakeCall(AActor::StaticClass(),TEXT("K2_GetActorLocation"),Position->NodePosX+250,Position->NodePosY+400);
 Link(Pin(Player,TEXT("ReturnValue")),Pin(PlayerPosition,TEXT("self")));
 auto* Clearance=MakeCall(Math,TEXT("Multiply_DoubleDouble"),Position->NodePosX+250,Position->NodePosY+600);
 Link(Pin(Size,TEXT("Grid_size")),Pin(Clearance,TEXT("A")));Schema->TrySetDefaultValue(*Pin(Clearance,TEXT("B")),TEXT("2"));
 auto TestDistance=[&](UEdGraphPin* Other,int32 Offset)
 {
  auto* D=MakeCall(Math,TEXT("Vector_Distance2D"),Position->NodePosX+500,Position->NodePosY+Offset);
  Link(Pin(Position,TEXT("ReturnValue")),Pin(D,TEXT("V1")));Link(Other,Pin(D,TEXT("V2")));
  auto* Check=MakeCall(Math,TEXT("GreaterEqual_DoubleDouble"),D->NodePosX+250,D->NodePosY);
  Link(Pin(D,TEXT("ReturnValue")),Pin(Check,TEXT("A")));Link(Pin(Clearance,TEXT("ReturnValue")),Pin(Check,TEXT("B")));return Check;
 };
 auto* FromStart=TestDistance(Pin(PlayerPosition,TEXT("ReturnValue")),0);
 auto* FromGoal=TestDistance(Pin(Goal,TEXT("PlannedGoalLocation")),200);
 auto* Both=MakeCall(Math,TEXT("BooleanAND"),FromStart->NodePosX+250,FromStart->NodePosY);
 Link(Pin(FromStart,TEXT("ReturnValue")),Pin(Both,TEXT("A")));Link(Pin(FromGoal,TEXT("ReturnValue")),Pin(Both,TEXT("B")));
 auto* Branch=New<UK2Node_IfThenElse>(G,Loop->NodePosX+350,Loop->NodePosY);Branch->AllocateDefaultPins();
 Branch->NodeComment=TEXT("開始位置とゴールの周囲2セルは柱を置かない");Branch->bCommentBubbleVisible=true;
 auto Next=Pin(Loop,TEXT("LoopBody"))->LinkedTo;Pin(Loop,TEXT("LoopBody"))->BreakAllPinLinks();
 Link(Pin(Loop,TEXT("LoopBody")),Pin(Branch,TEXT("execute")));Link(Pin(Both,TEXT("ReturnValue")),Pin(Branch,TEXT("Condition")));
 for(auto* P:Next)Link(Pin(Branch,TEXT("then")),P);
 Compile(BP);FString Report;Save(BP,Report);
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("BackroomGoalHandoff/pillar_result.txt")));
}
void SpawnMapGenerator()
{
 UWorld* World=GEditor->GetEditorWorldContext().World();check(World);
 AActor* Old=nullptr;
 for(TActorIterator<AActor> It(World);It;++It)if(It->GetClass()->GetName()==TEXT("BP_Dungeon_Generator_BackRoom_Save_C"))Old=*It;
 check(Old);
 UClass* Class=LoadClass<AActor>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Dungeon_Generator_BackRoom.BP_Dungeon_Generator_BackRoom_C"));check(Class);
 FActorSpawnParameters Params;Params.OverrideLevel=World->PersistentLevel;Params.bDeferConstruction=true;Params.ObjectFlags|=RF_Transactional;
 const FTransform Transform=Old->GetActorTransform();
 AActor* Actor=World->SpawnActor<AActor>(Class,Transform,Params);check(Actor);
 for(const TCHAR* Name:{TEXT("Seed"),TEXT("TargetRooms"),TEXT("GridMin"),TEXT("GridMax"),TEXT("PillarChancePercent"),TEXT("PillarMinRoomArea"),TEXT("bRepairRemoteEnds"),TEXT("MinAcceptableRooms"),TEXT("MaxAllowedExitDistance"),TEXT("ExtraLoopLimit")})
 {
  FProperty* From=FindFProperty<FProperty>(Old->GetClass(),Name);FProperty* To=FindFProperty<FProperty>(Class,Name);
  if(From&&To&&From->SameType(To))To->CopyCompleteValue(To->ContainerPtrToValuePtr<void>(Actor),From->ContainerPtrToValuePtr<void>(Old));
 }
 auto* Grid=FindFProperty<FNumericProperty>(Class,TEXT("Grid_size"));check(Grid);Grid->SetIntPropertyValue(Grid->ContainerPtrToValuePtr<void>(Actor),int64(250));
 auto* Legacy=FindFProperty<FNumericProperty>(Class,TEXT("GridSize"));check(Legacy);Legacy->SetFloatingPointPropertyValue(Legacy->ContainerPtrToValuePtr<void>(Actor),250.0);
 Actor->FinishSpawning(Transform);Actor->SetActorLabel(TEXT("BP_Dungeon_Generator_BackRoom"));Actor->MarkPackageDirty();
}
void Run()
{
 auto* Generator=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Dungeon_Generator_BackRoom.BP_Dungeon_Generator_BackRoom"));
 auto* Renderer=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer"));
 check(Generator&&Renderer);
 auto* Choose=Graph(Generator,TEXT("ChooseGoalLocation2"));
 auto* Find=Graph(Generator,TEXT("FindRoomCenter2"));
 auto* Evaluate=Graph(Generator,TEXT("StageEvaluate"));
 auto* Geometry=Graph(Generator,TEXT("BuildGeometry"));
 auto* Render=Graph(Renderer,TEXT("RenderBlueprintLayout"));
 auto* SpawnGoal=Graph(Renderer,TEXT("Spawn_goal_on_reserved_route"));
 check(Choose&&Find&&Evaluate&&Geometry&&Render&&SpawnGoal);
 check(Call(Evaluate,TEXT("ChooseGoalLocation2"))&&Call(Geometry,TEXT("RenderBlueprintLayout")));
 check(Set(Choose,TEXT("Current"))&&Set(SpawnGoal,TEXT("Selected_goal_location")));
 // Make the existing custom functions placeable without changing their bodies.
 for(auto* G:{Choose,Find,Graph(Generator,TEXT("FindRoomCenter")),Graph(Generator,TEXT("ChooseGoalLocation")),Render})
  if(G)Entry(G)->AddExtraFlags(FUNC_BlueprintCallable);
 FEdGraphPinType Vector;Vector.PinCategory=UEdGraphSchema_K2::PC_Struct;Vector.PinSubCategoryObject=TBaseStructure<FVector>::Get();
 for(auto* BP:{Generator,Renderer})
  if(!BP->NewVariables.ContainsByPredicate([](const auto& V){return V.VarName==TEXT("PlannedGoalLocation");}))
   FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("PlannedGoalLocation"),Vector);
 // Correct the copied map's value type. Its keys are floor cells and values are distances.
 auto* Dist=Entry(Choose)->LocalVariables.FindByPredicate([](const auto& V){return V.VarName==TEXT("RouteDistances");});check(Dist);
 Dist->VarType.PinCategory=UEdGraphSchema_K2::PC_Struct;
 Dist->VarType.PinSubCategoryObject=TBaseStructure<FIntPoint>::Get();
 Dist->VarType.ContainerType=EPinContainerType::Map;
 Dist->VarType.PinValueType.TerminalCategory=UEdGraphSchema_K2::PC_Int;
 Dist->VarType.PinValueType.TerminalSubCategory=NAME_None;
 Dist->VarType.PinValueType.TerminalSubCategoryObject=nullptr;
 if(!Entry(Render)->FindPin(TEXT("GoalBaseLocation")))Entry(Render)->CreateUserDefinedPin(TEXT("GoalBaseLocation"),Vector,EGPD_Output,false);
 Compile(Renderer);Compile(Generator);
 // Copied room-array nodes had lost their RoomId wire. Retain the reusable function input.
 for(UEdGraphNode* N:Find->Nodes)if(auto* A=Cast<UK2Node_GetArrayItem>(N))
 {
  auto* Array=Pin(A,TEXT("Array"));
  if(Array->LinkedTo.Num()==1)
   if(auto* V=Cast<UK2Node_VariableGet>(Array->LinkedTo[0]->GetOwningNode());V&&V->VariableReference.GetMemberName()==TEXT("Rooms"))
   {Pin(A,TEXT("Dimension 1"))->BreakAllPinLinks();Link(Pin(Entry(Find),TEXT("RoomId")),Pin(A,TEXT("Dimension 1")));}
 }
 // Current = Queue[Cursor], restoring the missing BFS data wire.
 auto* Current=Set(Choose,TEXT("Current"));
 auto* Queue=Variable<UK2Node_VariableGet>(Choose,TEXT("Queue"),Current->NodePosX-500,Current->NodePosY+120);
 auto* Cursor=Variable<UK2Node_VariableGet>(Choose,TEXT("Cursor"),Current->NodePosX-500,Current->NodePosY+220);
 auto* Item=New<UK2Node_GetArrayItem>(Choose,Current->NodePosX-240,Current->NodePosY+120);Item->AllocateDefaultPins();
 Link(Pin(Queue,TEXT("Queue")),Pin(Item,TEXT("Array")));
 Link(Pin(Cursor,TEXT("Cursor")),Pin(Item,TEXT("Dimension 1")));
 Pin(Current,TEXT("Current"))->BreakAllPinLinks();Link(Pin(Item,TEXT("Output")),Pin(Current,TEXT("Current")));
 // Match world conversion to the renderer's cell size, retaining all selection conditions.
 MatchGridSize(Choose);
 // Starting the player must use the same scale and centre as the data search.
 if(auto* Place=Graph(Generator,TEXT("PlacePlayerAtStart")))
 {
  MatchGridSize(Place);
  if(auto* C=Call(Place,TEXT("FindRoomCenter")))
  {C->FunctionReference.SetSelfMember(TEXT("FindRoomCenter2"));C->ReconstructNode();}
 }
 // Successful selection stores the planned position before StageGeometry is scheduled.
 auto* Choice=Call(Evaluate,TEXT("ChooseGoalLocation2"));
 auto* Decision=Cast<UK2Node_IfThenElse>(Pin(Choice,TEXT("then"))->LinkedTo[0]->GetOwningNode());check(Decision);
 auto* Store=Variable<UK2Node_VariableSet>(Evaluate,TEXT("PlannedGoalLocation"),Decision->NodePosX+250,Decision->NodePosY);
 auto Next=Pin(Decision,TEXT("then"))->LinkedTo;Pin(Decision,TEXT("then"))->BreakAllPinLinks();
 Link(Pin(Decision,TEXT("then")),Pin(Store,TEXT("execute")));
 Link(Pin(Choice,TEXT("GoalBaseLocation")),Pin(Store,TEXT("PlannedGoalLocation")));
 for(auto* P:Next)Link(Pin(Store,TEXT("then")),P);
 // Use the existing bounded retry decision; do not reload the level.
 UK2Node_IfThenElse* Retry=nullptr;
 for(UEdGraphNode* N:Evaluate->Nodes)if(auto* B=Cast<UK2Node_IfThenElse>(N))
  for(auto* P:Pin(B,TEXT("then"))->LinkedTo)
   if(auto* C=Cast<UK2Node_CallFunction>(P->GetOwningNode());C&&C->FunctionReference.GetMemberName()==TEXT("K2_SetTimer")&&Pin(C,TEXT("FunctionName"))->DefaultValue==TEXT("StartLayoutAttempt"))Retry=B;
 check(Retry);Pin(Decision,TEXT("else"))->BreakAllPinLinks();Link(Pin(Decision,TEXT("else")),Pin(Retry,TEXT("execute")));
 // Renderer receives the position as an explicit parameter.
 auto* RenderStore=Variable<UK2Node_VariableSet>(Render,TEXT("PlannedGoalLocation"),Entry(Render)->NodePosX+280,Entry(Render)->NodePosY);
 auto RenderNext=Pin(Entry(Render),TEXT("then"))->LinkedTo;Pin(Entry(Render),TEXT("then"))->BreakAllPinLinks();
 Link(Pin(Entry(Render),TEXT("then")),Pin(RenderStore,TEXT("execute")));
 Link(Pin(Entry(Render),TEXT("GoalBaseLocation")),Pin(RenderStore,TEXT("PlannedGoalLocation")));
 for(auto* P:RenderNext)Link(Pin(RenderStore,TEXT("then")),P);
 for(UEdGraphNode* N:Render->Nodes)
  if(auto* S=Cast<UK2Node_VariableSet>(N);S&&S->VariableReference.GetMemberName()==TEXT("PlannedGoalLocation"))
  {Pin(S,TEXT("PlannedGoalLocation"))->BreakAllPinLinks();Link(Pin(Entry(Render),TEXT("GoalBaseLocation")),Pin(S,TEXT("PlannedGoalLocation")));}
 // Reuse the existing goal/table rendering, bypassing the old physics-based selector.
 auto* GoalSet=Set(SpawnGoal,TEXT("Selected_goal_location"));
 // The old graph also has disconnected legacy setters; use the one directly fed by the native selector.
 if(auto* Native=Call(SpawnGoal,TEXT("SelectLargeRoomGoalLocation")))
  for(auto* P:Pin(Native,TEXT("GoalBaseLocation"))->LinkedTo)
   if(auto* S=Cast<UK2Node_VariableSet>(P->GetOwningNode()))GoalSet=S;
 Pin(Entry(SpawnGoal),TEXT("then"))->BreakAllPinLinks();Pin(GoalSet,TEXT("execute"))->BreakAllPinLinks();
 Link(Pin(Entry(SpawnGoal),TEXT("then")),Pin(GoalSet,TEXT("execute")));
 auto* Planned=Variable<UK2Node_VariableGet>(SpawnGoal,TEXT("PlannedGoalLocation"),GoalSet->NodePosX-200,GoalSet->NodePosY+180);
 Pin(GoalSet,TEXT("Selected_goal_location"))->BreakAllPinLinks();Link(Pin(Planned,TEXT("PlannedGoalLocation")),Pin(GoalSet,TEXT("Selected_goal_location")));
 Compile(Renderer);
 auto* RenderCall=Call(Geometry,TEXT("RenderBlueprintLayout"));RenderCall->ReconstructNode();
 auto* Position=Variable<UK2Node_VariableGet>(Geometry,TEXT("PlannedGoalLocation"),RenderCall->NodePosX-250,RenderCall->NodePosY+650);
 Link(Pin(Position,TEXT("PlannedGoalLocation")),Pin(RenderCall,TEXT("GoalBaseLocation")));
 Compile(Generator);
 FString Report;
 if(Renderer->Status==BS_Error||Generator->Status==BS_Error)Report=TEXT("ABORTED: Blueprint compilation failed; neither asset saved.\n");
 else {Save(Renderer,Report);Save(Generator,Report);}
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("BackroomGoalHandoff/result.txt")));
 UE_LOG(LogTemp,Display,TEXT("GOAL_HANDOFF %s"),*Report);
}
}
static FAutoConsoleCommand GoalHandoffCommand(TEXT("BackroomGoalHandoff.Apply"),TEXT("Repair copied goal selection and pass its result to the renderer."),FConsoleCommandDelegate::CreateStatic(&BackroomGoalHandoff::Run));
static FAutoConsoleCommand GoalHandoffInputCommand(TEXT("BackroomGoalHandoff.RepairInput"),TEXT("Reconnect all planned goal assignments to the render input."),FConsoleCommandDelegate::CreateStatic(&BackroomGoalHandoff::RepairInput));
static FAutoConsoleCommand GoalPillarCommand(TEXT("BackroomGoalHandoff.ProtectPillars"),TEXT("Reserve space around the start and planned goal before spawning pillars."),FConsoleCommandDelegate::CreateStatic(&BackroomGoalHandoff::ProtectPillarClearance));
static FAutoConsoleCommand GoalMapSpawnCommand(TEXT("BackroomGoalHandoff.SpawnMapGenerator"),TEXT("Spawn the repaired generator in the editor without using an actor factory."),FConsoleCommandDelegate::CreateStatic(&BackroomGoalHandoff::SpawnMapGenerator));
#endif


