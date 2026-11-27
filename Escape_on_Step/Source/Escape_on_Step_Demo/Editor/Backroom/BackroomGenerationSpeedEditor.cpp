#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

// Editor authoring only. Runtime generation continues to execute Blueprint nodes.
namespace BackroomGenerationSpeed
{
const TCHAR* GenPath=TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Dungeon_Generator_BackRoom.BP_Dungeon_Generator_BackRoom");
const TCHAR* RenderPath=TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer");
UEdGraph* Graph(UBlueprint* BP,const TCHAR* Name)
{for(UEdGraph* G:BP->FunctionGraphs)if(G->GetFName()==Name)return G;return nullptr;}
UK2Node_FunctionEntry* Entry(UEdGraph* G)
{check(G);for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N))return E;return nullptr;}
UEdGraphPin* Pin(UEdGraphNode* N,const TCHAR* Name){check(N);return N->FindPinChecked(Name);}
void Link(UEdGraphPin* A,UEdGraphPin* B){check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(A,B));}
template<class T>T* New(UEdGraph* G,int32 X,int32 Y)
{auto* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;}
UK2Node_CallFunction* Call(UEdGraph* G,UClass* Owner,const TCHAR* Name,int32 X,int32 Y)
{auto* N=New<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(Owner->FindFunctionByName(Name));N->AllocateDefaultPins();return N;}
template<class T>T* Var(UEdGraph* G,const TCHAR* Name,int32 X,int32 Y)
{
 auto* N=New<T>(G,X,Y);
 if(auto* L=Entry(G)->LocalVariables.FindByPredicate([&](const auto& V){return V.VarName==Name;}))N->VariableReference.SetLocalMember(Name,G->GetName(),L->VarGuid);
 else N->VariableReference.SetSelfMember(Name);
 N->AllocateDefaultPins();return N;
}
void Compile(UBlueprint* BP)
{FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);}
UK2Node_CallFunction* Print(UEdGraph* G,const FString& Text,int32 X,int32 Y)
{
 auto* N=Call(G,UKismetSystemLibrary::StaticClass(),TEXT("PrintString"),X,Y);
 const auto* S=GetDefault<UEdGraphSchema_K2>();
 S->TrySetDefaultValue(*Pin(N,TEXT("InString")),Text);
 S->TrySetDefaultValue(*Pin(N,TEXT("bPrintToScreen")),TEXT("false"));
 S->TrySetDefaultValue(*Pin(N,TEXT("bPrintToLog")),TEXT("true"));return N;
}
void ProfileTransient()
{
 const TSet<FName> Targets={TEXT("BuildAttempt"),TEXT("AddExtraDoors"),TEXT("AddReturnCorridors"),TEXT("AddCorridorBranches"),TEXT("AddCorridorRooms"),TEXT("BuildWalls"),TEXT("RemoveIsolatedWalls"),TEXT("EvaluateLayout"),TEXT("ChooseGoalLocation2"),TEXT("BuildGeometry"),TEXT("SelectRoomPillars"),TEXT("RenderBlueprintLayout"),TEXT("Init_Grid"),TEXT("Generate_Floors"),TEXT("Spawn_room_enemies"),TEXT("SpawnRoomPillars"),TEXT("Generate_Corridor_Lights"),TEXT("PlanRegionLightPositions"),TEXT("Spawn_goal_on_reserved_route")};
 for(const TCHAR* Path:{GenPath,RenderPath})
 {
  auto* BP=LoadObject<UBlueprint>(nullptr,Path);check(BP);
  for(UEdGraph* G:BP->FunctionGraphs)
  {
   auto Old=G->Nodes;
   if(G->GetName().StartsWith(TEXT("Stage"))||G->GetFName()==TEXT("StartLayoutAttempt"))
   {
    auto* E=Entry(G);auto Next=Pin(E,TEXT("then"))->LinkedTo;
    auto* P=Print(G,TEXT("GEN_SPEED STAGE ")+G->GetName(),E->NodePosX+150,E->NodePosY-200);
    Pin(E,TEXT("then"))->BreakAllPinLinks();Link(Pin(E,TEXT("then")),Pin(P,TEXT("execute")));
    for(auto* N:Next)Link(Pin(P,TEXT("then")),N);
   }
   for(UEdGraphNode* N:Old)
   {
    auto* C=Cast<UK2Node_CallFunction>(N);if(!C||!Targets.Contains(C->FunctionReference.GetMemberName()))continue;
    auto* In=C->FindPin(TEXT("execute"));auto* Out=C->FindPin(TEXT("then"));if(!In||!Out||In->LinkedTo.IsEmpty())continue;
    auto Prev=In->LinkedTo;auto Next=Out->LinkedTo;
    const FString Name=C->FunctionReference.GetMemberName().ToString();
    auto* Before=Print(G,TEXT("GEN_SPEED BEGIN ")+Name,C->NodePosX-100,C->NodePosY-300);
    auto* After=Print(G,TEXT("GEN_SPEED END ")+Name,C->NodePosX+200,C->NodePosY-300);
    In->BreakAllPinLinks();Out->BreakAllPinLinks();
    for(auto* P:Prev)Link(P,Pin(Before,TEXT("execute")));
    Link(Pin(Before,TEXT("then")),In);Link(Out,Pin(After,TEXT("execute")));
    for(auto* P:Next)Link(Pin(After,TEXT("then")),P);
   }
  }
  Compile(BP);
 }
 UE_LOG(LogTemp,Display,TEXT("GEN_SPEED transient instrumentation ready; no assets saved"));
}
void OptimizeReturns()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,GenPath);check(BP);
 auto* Returns=Graph(BP,TEXT("StageReturnCorridor"));auto* Start=Graph(BP,TEXT("StartLayoutAttempt"));check(Returns&&Start);
 check(!BP->NewVariables.ContainsByPredicate([](const auto& V){return V.VarName==TEXT("ReturnRoomsPerFrame");}));
 FEdGraphPinType Int;Int.PinCategory=UEdGraphSchema_K2::PC_Int;
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("ReturnRoomsPerFrame"),Int,TEXT("4"));
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("ReturnBatchCount"),Int,TEXT("0"));
 FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,TEXT("ReturnRoomsPerFrame"),false);
 FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,TEXT("ReturnRoomsPerFrame"),nullptr,FText::FromString(TEXT("Generation|Performance")));
 FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("ReturnRoomsPerFrame"),nullptr,TEXT("ClampMin"),TEXT("1"));
 FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("ReturnRoomsPerFrame"),nullptr,TEXT("ClampMax"),TEXT("4"));
 auto* Remote=Graph(BP,TEXT("StageRemoteEndRepair"));check(Remote);
 Entry(Returns)->AddExtraFlags(FUNC_BlueprintCallable);Entry(Remote)->AddExtraFlags(FUNC_BlueprintCallable);Compile(BP);
 auto* ResetStart=Var<UK2Node_VariableSet>(Start,TEXT("ReturnBatchCount"),Entry(Start)->NodePosX+150,Entry(Start)->NodePosY-200);
 auto First=Pin(Entry(Start),TEXT("then"))->LinkedTo;Pin(Entry(Start),TEXT("then"))->BreakAllPinLinks();
 Link(Pin(Entry(Start),TEXT("then")),Pin(ResetStart,TEXT("execute")));
 for(auto* P:First)Link(Pin(ResetStart,TEXT("then")),P);
 // Reset the shared batch counter before the remote-repair phase as well.
 auto* WallsStage=Graph(BP,TEXT("StageWalls"));check(WallsStage);
 auto* ResetWalls=Var<UK2Node_VariableSet>(WallsStage,TEXT("ReturnBatchCount"),Entry(WallsStage)->NodePosX+150,Entry(WallsStage)->NodePosY-200);
 auto WallsNext=Pin(Entry(WallsStage),TEXT("then"))->LinkedTo;Pin(Entry(WallsStage),TEXT("then"))->BreakAllPinLinks();
 Link(Pin(Entry(WallsStage),TEXT("then")),Pin(ResetWalls,TEXT("execute")));for(auto* P:WallsNext)Link(Pin(ResetWalls,TEXT("then")),P);
 // Every successful path in AddReturnCorridors increments Corridors before changing Cells/Doors.
 // Without that increment, the wall inputs are unchanged and rebuilding gives the same result.
 UK2Node_CallFunction* Build=nullptr;UK2Node_CallFunction* Clean=nullptr;
 for(UEdGraphNode* N:Remote->Nodes)if(auto* C=Cast<UK2Node_CallFunction>(N))
 {if(C->FunctionReference.GetMemberName()==TEXT("BuildWalls"))Build=C;if(C->FunctionReference.GetMemberName()==TEXT("RemoveIsolatedWalls"))Clean=C;}
 check(Build&&Clean);
 auto* Corridors=Var<UK2Node_VariableGet>(Remote,TEXT("Corridors"),Build->NodePosX-700,Build->NodePosY+250);
 auto* Before=Var<UK2Node_VariableGet>(Remote,TEXT("BeforeCorridors"),Build->NodePosX-700,Build->NodePosY+350);
 auto* Changed=Call(Remote,UKismetMathLibrary::StaticClass(),TEXT("NotEqual_IntInt"),Build->NodePosX-450,Build->NodePosY+250);
 Link(Pin(Corridors,TEXT("Corridors")),Pin(Changed,TEXT("A")));Link(Pin(Before,TEXT("BeforeCorridors")),Pin(Changed,TEXT("B")));
 auto* Dirty=New<UK2Node_IfThenElse>(Remote,Build->NodePosX-250,Build->NodePosY);Dirty->AllocateDefaultPins();
 auto WallPrev=Pin(Build,TEXT("execute"))->LinkedTo;auto WallAfter=Pin(Clean,TEXT("then"))->LinkedTo;Pin(Build,TEXT("execute"))->BreakAllPinLinks();
 for(auto* P:WallPrev)Link(P,Pin(Dirty,TEXT("execute")));Link(Pin(Changed,TEXT("ReturnValue")),Pin(Dirty,TEXT("Condition")));
 Link(Pin(Dirty,TEXT("then")),Pin(Build,TEXT("execute")));for(auto* P:WallAfter)Link(Pin(Dirty,TEXT("else")),P);
 Dirty->NodeComment=TEXT("通路が追加された場合だけ壁を再計算。未変更なら既存の壁を使う。");Dirty->bCommentBubbleVisible=true;
 int32 Patched=0;
 for(UEdGraph* BatchGraph:{Returns,Remote})
 {
 UEdGraph* G=BatchGraph;const FString FunctionName=G->GetName();const bool IsRemote=G==Remote;
 auto Old=G->Nodes;
 for(UEdGraphNode* N:Old)
 {
  auto* Timer=Cast<UK2Node_CallFunction>(N);
  if(!Timer||Timer->FunctionReference.GetMemberName()!=TEXT("K2_SetTimer")||Pin(Timer,TEXT("FunctionName"))->DefaultValue!=FunctionName)continue;
  auto* Count=Var<UK2Node_VariableGet>(G,TEXT("ReturnBatchCount"),Timer->NodePosX-700,Timer->NodePosY+200);
  auto* Add=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Add_IntInt"),Timer->NodePosX-450,Timer->NodePosY+200);
  Link(Pin(Count,TEXT("ReturnBatchCount")),Pin(Add,TEXT("A")));GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Add,TEXT("B")),TEXT("1"));
  auto* SetCount=Var<UK2Node_VariableSet>(G,TEXT("ReturnBatchCount"),Timer->NodePosX-450,Timer->NodePosY);
  Link(Pin(Add,TEXT("ReturnValue")),Pin(SetCount,TEXT("ReturnBatchCount")));
  auto Prev=Pin(Timer,TEXT("execute"))->LinkedTo;Pin(Timer,TEXT("execute"))->BreakAllPinLinks();
  for(auto* P:Prev)Link(P,Pin(SetCount,TEXT("execute")));
  auto* Limit=Var<UK2Node_VariableGet>(G,TEXT("ReturnRoomsPerFrame"),Timer->NodePosX-700,Timer->NodePosY+400);
  auto* Clamp=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Clamp"),Timer->NodePosX-450,Timer->NodePosY+400);
  Link(Pin(Limit,TEXT("ReturnRoomsPerFrame")),Pin(Clamp,TEXT("Value")));
  GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Clamp,TEXT("Min")),TEXT("1"));GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(Clamp,TEXT("Max")),IsRemote?TEXT("2"):TEXT("4"));
  auto* Less=Call(G,UKismetMathLibrary::StaticClass(),TEXT("Less_IntInt"),Timer->NodePosX-200,Timer->NodePosY+200);
  Link(Pin(Count,TEXT("ReturnBatchCount")),Pin(Less,TEXT("A")));Link(Pin(Clamp,TEXT("ReturnValue")),Pin(Less,TEXT("B")));
  auto* B=New<UK2Node_IfThenElse>(G,Timer->NodePosX-150,Timer->NodePosY);B->AllocateDefaultPins();
  Link(Pin(SetCount,TEXT("then")),Pin(B,TEXT("execute")));Link(Pin(Less,TEXT("ReturnValue")),Pin(B,TEXT("Condition")));
  auto* Continue=Call(G,BP->SkeletonGeneratedClass,*FunctionName,Timer->NodePosX+150,Timer->NodePosY-200);
  Continue->FunctionReference.SetSelfMember(*FunctionName);
  Link(Pin(B,TEXT("then")),Pin(Continue,TEXT("execute")));
  auto* Reset=Var<UK2Node_VariableSet>(G,TEXT("ReturnBatchCount"),Timer->NodePosX+150,Timer->NodePosY+150);
  Link(Pin(B,TEXT("else")),Pin(Reset,TEXT("execute")));Link(Pin(Reset,TEXT("then")),Pin(Timer,TEXT("execute")));
  B->NodeComment=IsRemote?TEXT("修復処理は1フレーム最大2部屋で次フレームへ譲る。"):TEXT("1フレーム最大4部屋。上限で次フレームへ譲り、無制限ループを防ぐ。");B->bCommentBubbleVisible=true;++Patched;
 }
 }
 check(Patched==4);Compile(BP);
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
 FString Report=FString::Printf(TEXT("patched=%d errors=%d warnings=%d\n"),Patched,Log.NumErrors,Log.NumWarnings);
 for(auto& M:Log.Messages)Report+=M->ToText().ToString()+TEXT("\n");
 if(!Log.NumErrors)
 {
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  Report+=FString::Printf(TEXT("saved=%d\n"),UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 }
 FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("BackroomGenerationSpeed/apply_result.txt")));
 UE_LOG(LogTemp,Display,TEXT("GEN_SPEED %s"),*Report);
}
}
static FAutoConsoleCommand ProfileGenerationSpeed(TEXT("BackroomSpeed.ProfileTransient"),TEXT("Add unsaved profiling markers for a test session."),FConsoleCommandDelegate::CreateStatic(&BackroomGenerationSpeed::ProfileTransient));
static FAutoConsoleCommand OptimizeGenerationSpeed(TEXT("BackroomSpeed.OptimizeReturns"),TEXT("Batch up to four return-corridor rooms per frame while retaining a bounded yield."),FConsoleCommandDelegate::CreateStatic(&BackroomGenerationSpeed::OptimizeReturns));
#endif
