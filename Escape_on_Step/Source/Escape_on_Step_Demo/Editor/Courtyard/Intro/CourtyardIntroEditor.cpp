// Editor-only authoring helper. The generated exit runs entirely as Blueprint.
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "Engine/TriggerBox.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "ActorFactories/ActorFactory.h"
#include "Builders/CubeBuilder.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavMesh/RecastNavMesh.h"
#include "NavigationSystem.h"
#include "Camera/PlayerCameraManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"

namespace CourtyardIntroEditor
{
template<class T> T* Node(UEdGraph* Graph,int32 X,int32 Y)
{
 T* N=NewObject<T>(Graph);Graph->AddNode(N,false,false);N->CreateNewGuid();N->NodePosX=X;N->NodePosY=Y;return N;
}
UEdGraphPin* Pin(UEdGraphNode* N,const TCHAR* Name){return N->FindPinChecked(FName(Name));}
void Link(UEdGraphNode* A,const TCHAR* AP,UEdGraphNode* B,const TCHAR* BP)
{
 checkf(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(A,AP),Pin(B,BP)),TEXT("Intro exit connection failed: %s -> %s"),AP,BP);
}
UK2Node_CallFunction* Call(UEdGraph* G,UClass* Class,const TCHAR* Name,int32 X,int32 Y)
{
 auto* N=Node<UK2Node_CallFunction>(G,X,Y);N->SetFromFunction(Class->FindFunctionByName(FName(Name)));N->AllocateDefaultPins();return N;
}
void CreateExit()
{
 const FString PackageName=TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Intro/Blueprints/BP_IntroExit");
 if(LoadObject<UBlueprint>(nullptr,*(PackageName+TEXT(".BP_IntroExit")))){UE_LOG(LogTemp,Display,TEXT("INTRO_EXIT_ALREADY_EXISTS"));return;}
 auto* Package=CreatePackage(*PackageName);
 auto* BP=FKismetEditorUtilities::CreateBlueprint(ATriggerBox::StaticClass(),Package,TEXT("BP_IntroExit"),BPTYPE_Normal,UBlueprint::StaticClass(),UBlueprintGeneratedClass::StaticClass());
 check(BP);
 FEdGraphPinType NameType;NameType.PinCategory=UEdGraphSchema_K2::PC_Name;
 FEdGraphPinType BoolType;BoolType.PinCategory=UEdGraphSchema_K2::PC_Boolean;
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("NextLevelName"),NameType,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Maps/LVL_BackAlley"));
 FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,TEXT("NextLevelName"),false);
 FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("bTransitionStarted"),BoolType,TEXT("false"));
 FKismetEditorUtilities::CompileBlueprint(BP);
 UEdGraph* G=BP->UbergraphPages.Num()?BP->UbergraphPages[0].Get():nullptr;
 if(!G){G=FBlueprintEditorUtils::CreateNewGraph(BP,TEXT("EventGraph"),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());FBlueprintEditorUtils::AddUbergraphPage(BP,G);}
 auto* Event=Node<UK2Node_Event>(G,0,0);Event->EventReference.SetExternalMember(TEXT("ReceiveActorBeginOverlap"),AActor::StaticClass());Event->bOverrideFunction=true;Event->AllocateDefaultPins();
 auto* Player=Call(G,UGameplayStatics::StaticClass(),TEXT("GetPlayerCharacter"),0,250);
 auto* Equal=Call(G,UKismetMathLibrary::StaticClass(),TEXT("EqualEqual_ObjectObject"),320,220);
 auto* IsPlayer=Node<UK2Node_IfThenElse>(G,600,0);IsPlayer->AllocateDefaultPins();
 auto* Started=Node<UK2Node_VariableGet>(G,600,280);Started->VariableReference.SetSelfMember(TEXT("bTransitionStarted"));Started->AllocateDefaultPins();
 auto* Once=Node<UK2Node_IfThenElse>(G,880,0);Once->AllocateDefaultPins();
 auto* SetStarted=Node<UK2Node_VariableSet>(G,1150,100);SetStarted->VariableReference.SetSelfMember(TEXT("bTransitionStarted"));SetStarted->AllocateDefaultPins();
 GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*Pin(SetStarted,TEXT("bTransitionStarted")),TEXT("true"));
 auto* Next=Node<UK2Node_VariableGet>(G,1150,340);Next->VariableReference.SetSelfMember(TEXT("NextLevelName"));Next->AllocateDefaultPins();
 auto* Open=Call(G,UGameplayStatics::StaticClass(),TEXT("OpenLevel"),1450,100);
 Link(Event,TEXT("then"),IsPlayer,TEXT("execute"));
 Link(Event,TEXT("OtherActor"),Equal,TEXT("A"));Link(Player,TEXT("ReturnValue"),Equal,TEXT("B"));Link(Equal,TEXT("ReturnValue"),IsPlayer,TEXT("Condition"));
 Link(IsPlayer,TEXT("then"),Once,TEXT("execute"));Link(Started,TEXT("bTransitionStarted"),Once,TEXT("Condition"));
 // Only the false branch may initiate travel; subsequent overlaps do nothing.
 Link(Once,TEXT("else"),SetStarted,TEXT("execute"));Link(SetStarted,TEXT("then"),Open,TEXT("execute"));Link(Next,TEXT("NextLevelName"),Open,TEXT("LevelName"));
 Event->NodeComment=TEXT("Only the local player can leave the introduction.");Event->bCommentBubbleVisible=true;
 Open->NodeComment=TEXT("Travel to the ISM maze. Its existing generator handles the player's start.");Open->bCommentBubbleVisible=true;
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
 if(Results.NumErrors){UE_LOG(LogTemp,Error,TEXT("INTRO_EXIT_COMPILE_FAILED %d"),Results.NumErrors);return;}
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 const auto Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
 check(UPackage::SavePackage(Package,BP,*Filename,Args));
 UE_LOG(LogTemp,Display,TEXT("INTRO_EXIT_CREATED"));
}
static FAutoConsoleCommand Command(TEXT("CourtyardIntro.CreateExit"),TEXT("Create a standalone Blueprint exit for the copied introduction map."),FConsoleCommandDelegate::CreateStatic(&CreateExit));
void AddNavigation()
{
 UWorld* World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
 if(!World||!World->GetOutermost()->GetName().EndsWith(TEXT("/LVL_Intro_Explanation")))return;
 ANavMeshBoundsVolume* Volume=nullptr;
 for(TActorIterator<ANavMeshBoundsVolume> It(World);It;++It){Volume=*It;break;}
 if(!Volume)
 {
  Volume=World->SpawnActor<ANavMeshBoundsVolume>(FVector(11000,0,250),FRotator::ZeroRotator);
  Volume->SetActorLabel(TEXT("Intro_InlineMaze_NavBounds"));
  auto* Builder=NewObject<UCubeBuilder>();Builder->X=24000;Builder->Y=20000;Builder->Z=1000;
  UActorFactory::CreateBrushForVolumeActor(Volume,Builder);
 }
 if(auto* Nav=FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
 {
  Nav->OnNavigationBoundsUpdated(Volume);
  auto* Data=Nav->GetDefaultNavDataInstance(FNavigationSystem::Create);
  if(Data)
   if(auto* Property=Data->GetClass()->FindPropertyByName(TEXT("RuntimeGeneration")))
    Property->ImportText_Direct(TEXT("Dynamic"),Property->ContainerPtrToValuePtr<void>(Data),Data,PPF_None);
 }
 World->MarkPackageDirty();
 UE_LOG(LogTemp,Display,TEXT("INTRO_NAVIGATION_CONFIGURED"));
}
static FAutoConsoleCommand NavigationCommand(TEXT("CourtyardIntro.AddNavigation"),TEXT("Add dynamic navigation to the owned introduction map."),FConsoleCommandDelegate::CreateStatic(&AddNavigation));
void RepairStartupFade()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_ISM.BP_Courtyard_Generator_ISM"));
 if(!BP)return;
 UEdGraph* G=BP->UbergraphPages[0];UK2Node_Event* Ready=nullptr;UK2Node_CallFunction* Stop=nullptr;UK2Node_CallFunction* Generate=nullptr;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(auto* E=Cast<UK2Node_Event>(N);E&&E->EventReference.GetMemberName()==TEXT("OnCourtyardReady"))Ready=E;
  if(auto* C=Cast<UK2Node_CallFunction>(N))
  {
   if(C->FunctionReference.GetMemberName()==TEXT("StopCameraFade")&&!Stop)Stop=C;
   if(C->FunctionReference.GetMemberName()==TEXT("GenerateCourtyardBP"))Generate=C;
  }
 }
 check(Ready&&Stop&&Generate);
 auto* ReadyThen=Pin(Ready,TEXT("then"));
 if(!ReadyThen->LinkedTo.Contains(Pin(Stop,TEXT("execute"))))
 {
  check(ReadyThen->LinkedTo.Num()==1);auto* Original=ReadyThen->LinkedTo[0];
  ReadyThen->BreakAllPinLinks();Pin(Stop,TEXT("execute"))->BreakAllPinLinks();Pin(Stop,TEXT("then"))->BreakAllPinLinks();
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(ReadyThen,Pin(Stop,TEXT("execute"))));
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Pin(Stop,TEXT("then")),Original));
  Stop->NodeComment=TEXT("Always release startup blackout before optional Timeline lookup.");Stop->bCommentBubbleVisible=true;
 }
 bool HasFailureBranch=false;
 for(UEdGraphNode* N:G->Nodes)if(N->NodeComment==TEXT("Release blackout if no layout could be generated."))HasFailureBranch=true;
 if(!HasFailureBranch)
 {
  UEdGraphPin* Result=nullptr;
  for(auto* P:Generate->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory==UEdGraphSchema_K2::PC_Boolean){Result=P;break;}
  check(Result);
  auto* Branch=Node<UK2Node_IfThenElse>(G,Generate->NodePosX+320,Generate->NodePosY);Branch->AllocateDefaultPins();
  auto* FailStop=Call(G,APlayerCameraManager::StaticClass(),TEXT("StopCameraFade"),Generate->NodePosX+640,Generate->NodePosY+160);
  FailStop->NodeComment=TEXT("Release blackout if no layout could be generated.");FailStop->bCommentBubbleVisible=true;
  auto* Camera=Call(G,UGameplayStatics::StaticClass(),TEXT("GetPlayerCameraManager"),Generate->NodePosX+320,Generate->NodePosY+320);
  Link(Generate,TEXT("then"),Branch,TEXT("execute"));
  check(GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(Result,Pin(Branch,TEXT("Condition"))));
  Link(Branch,TEXT("else"),FailStop,TEXT("execute"));Link(Camera,TEXT("ReturnValue"),FailStop,TEXT("self"));
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);
 check(Results.NumErrors==0);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 UE_LOG(LogTemp,Display,TEXT("INTRO_STARTUP_FADE_REPAIRED"));
}
static FAutoConsoleCommand FadeCommand(TEXT("CourtyardIntro.RepairStartupFade"),TEXT("Release startup fade independently of optional player timelines."),FConsoleCommandDelegate::CreateStatic(&RepairStartupFade));
void ProbeFade()
{
 UWorld* World=GEditor?GEditor->PlayWorld:nullptr;
 auto* Camera=World?UGameplayStatics::GetPlayerCameraManager(World,0):nullptr;
 if(!Camera)return;
 const FString Value=FString::Printf(TEXT("{\"enabled\":%s,\"amount\":%f}"),Camera->bEnableFading?TEXT("true"):TEXT("false"),Camera->FadeAmount);
 FFileHelper::SaveStringToFile(Value,*(FPaths::ProjectSavedDir()/TEXT("CourtyardIntroFadeProbe.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
static FAutoConsoleCommand ProbeCommand(TEXT("CourtyardIntro.ProbeFade"),TEXT("Inspect the startup camera fade during editor verification."),FConsoleCommandDelegate::CreateStatic(&ProbeFade));
}
#endif
