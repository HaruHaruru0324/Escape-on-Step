#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_IfThenElse.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
static FAutoConsoleCommand GuardRouteDestroy(TEXT("DungeonWalls.GuardRouteDestroy"),TEXT("Validate traced actors before destroying them."),FConsoleCommandDelegate::CreateLambda([]()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/AutoMapData/Blueprint/BP_Dugeon_Generator.BP_Dugeon_Generator"));if(!BP)return;
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();bool OK=true;int32 Count=0;
 auto Link=[&](UEdGraphPin*A,UEdGraphPin*B){if(!A||!B||!Schema->TryCreateConnection(A,B))OK=false;};
 BP->Modify();
 for(UEdGraph* G:BP->FunctionGraphs)
 {
  if(G->GetName()!=TEXT("Put_player_on_reserved_route")&&G->GetName()!=TEXT("Clear_reserved_route_segment")&&G->GetName()!=TEXT("Spawn_goal_on_reserved_route"))continue;
  bool Already=false;for(UEdGraphNode*N:G->Nodes)if(N->GetName().StartsWith(TEXT("GuardDestroyedWall")))Already=true;if(Already)continue;
  G->Modify();auto Nodes=G->Nodes;
  for(UEdGraphNode* N:Nodes)
  {
   auto* Destroy=Cast<UK2Node_CallFunction>(N);if(!Destroy||Destroy->FunctionReference.GetMemberName()!=TEXT("K2_DestroyActor"))continue;
   auto* Target=Destroy->FindPinChecked(TEXT("self"));if(Target->LinkedTo.Num()!=1)continue;
   FGraphNodeCreator<UK2Node_CallFunction> C(*G);auto* Valid=C.CreateNode();
   Valid->SetFromFunction(UKismetSystemLibrary::StaticClass()->FindFunctionByName(TEXT("IsValid")));
   Valid->NodePosX=N->NodePosX-440;Valid->NodePosY=N->NodePosY+150;C.Finalize();
   FGraphNodeCreator<UK2Node_IfThenElse> B(*G);auto* Branch=B.CreateNode();
   Branch->Rename(*FString::Printf(TEXT("GuardDestroyedWall_%d"),Count),G,REN_DontCreateRedirectors);
   Branch->NodePosX=N->NodePosX-220;Branch->NodePosY=N->NodePosY;
   Branch->NodeComment=TEXT("Trace results may contain an actor already destroyed by an earlier hit");Branch->bCommentBubbleVisible=true;B.Finalize();
   Link(Target->LinkedTo[0],Valid->FindPin(TEXT("Object")));Link(Valid->FindPin(TEXT("ReturnValue")),Branch->GetConditionPin());
   auto Exec=Destroy->FindPinChecked(TEXT("execute"))->LinkedTo;Destroy->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
   for(auto* P:Exec)Link(P,Branch->GetExecPin());Link(Branch->GetThenPin(),Destroy->FindPinChecked(TEXT("execute")));
   // Preserve any continuation when the trace actor has already been removed.
   for(auto* P:Destroy->FindPinChecked(TEXT("then"))->LinkedTo)Link(Branch->GetElsePin(),P);
   ++Count;
  }
 }
 if(!OK){UE_LOG(LogTemp,Error,TEXT("ROUTE_GUARD failed; not saved"));return;}
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
 if(Log.NumErrors){UE_LOG(LogTemp,Error,TEXT("ROUTE_GUARD compile failed; not saved"));return;}
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 bool Saved=UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 UE_LOG(LogTemp,Display,TEXT("ROUTE_GUARD saved=%d guards=%d warnings=%d"),Saved,Count,Log.NumWarnings);
}));
#endif
