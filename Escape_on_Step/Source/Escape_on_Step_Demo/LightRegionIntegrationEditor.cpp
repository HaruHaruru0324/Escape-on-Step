#if WITH_EDITOR
#include "CoreMinimal.h"
#include "DungeonLightPlacementLibrary.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_GetArrayItem.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Components/LocalLightComponent.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

static FAutoConsoleCommand IntegrateLightRegions(TEXT("DungeonLights.IntegrateRegions"),TEXT("Install region planner using UE graph APIs."),
 FConsoleCommandDelegate::CreateLambda([]()
 {
	UBlueprint* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/AutoMapData/Blueprint/BP_Dugeon_Generator.BP_Dugeon_Generator"));
	check(BP);
	UEdGraph* G=nullptr;
	for(UEdGraph* Graph:BP->FunctionGraphs) if(Graph->GetName()==TEXT("Generate_Corridor_Lights")) G=Graph;
	check(G);
	UK2Node_CallFunction* Old=nullptr; UK2Node_VariableGet* Size=nullptr; UEdGraphNode* Loop=nullptr; UK2Node_CallFunction* Location=nullptr;
	for(UEdGraphNode* N:G->Nodes)
	{
		if(auto* F=Cast<UK2Node_CallFunction>(N))
		{
			if(F->FunctionReference.GetMemberName()==TEXT("PlanCorridorLightPositions")) Old=F;
			if(F->FunctionReference.GetMemberName()==TEXT("K2_SetWorldLocation")) Location=F;
		}
		if(auto* V=Cast<UK2Node_VariableGet>(N); V && V->VariableReference.GetMemberName()==TEXT("Grid_size")) Size=V;
		if(N->GetFName()==TEXT("FPS_Loop")) Loop=N;
	}
	check(Old && Size && Loop && Location);
	BP->Modify();G->Modify();
	const UEdGraphSchema_K2* Schema=GetDefault<UEdGraphSchema_K2>();
	auto Link=[&](UEdGraphPin* A,UEdGraphPin* B) { check(A && B); check(Schema->TryCreateConnection(A,B)); };
	auto Move=[&](UEdGraphPin* A,UEdGraphPin* B) { auto Links=A->LinkedTo; A->BreakAllPinLinks();for(auto* P:Links) Link(P,B); };
	FGraphNodeCreator<UK2Node_CallFunction> Creator(*G);
	auto* Plan=Creator.CreateNode();
	Plan->SetFromFunction(UDungeonLightPlacementLibrary::StaticClass()->FindFunctionByName(TEXT("PlanRegionLightPositions")));
	Plan->NodePosX=Old->NodePosX;Plan->NodePosY=Old->NodePosY;
	Plan->NodeComment=TEXT("One light per open space; 1-2 cell necks separate regions. Run after route changes.");
	Plan->bCommentBubbleVisible=true; Creator.Finalize();
	for(FName Name:{FName(TEXT("execute")),FName(TEXT("then")),FName(TEXT("CandidateWorldPositions")),FName(TEXT("ExistingLightWorldPositions")),FName(TEXT("ReturnValue"))})
		Move(Old->FindPinChecked(Name),Plan->FindPinChecked(Name));
	Link(Size->FindPinChecked(TEXT("Grid_size")),Plan->FindPinChecked(TEXT("GridSize")));
	Old->BreakAllNodeLinks();G->RemoveNode(Old);
	FGraphNodeCreator<UK2Node_GetArrayItem> GetCreator(*G);
	auto* Get=GetCreator.CreateNode();Get->NodePosX=Plan->NodePosX+400;Get->NodePosY=Plan->NodePosY+300;GetCreator.Finalize();
	Link(Plan->FindPinChecked(TEXT("AttenuationRadii")),Get->GetTargetArrayPin());
	Link(Loop->FindPinChecked(TEXT("Array Index")),Get->GetIndexPin());
	FGraphNodeCreator<UK2Node_CallFunction> RadiusCreator(*G);
	auto* Radius=RadiusCreator.CreateNode();Radius->SetFromFunction(ULocalLightComponent::StaticClass()->FindFunctionByName(TEXT("SetAttenuationRadius")));
	Radius->NodePosX=Location->NodePosX;Radius->NodePosY=Location->NodePosY-250;RadiusCreator.Finalize();
	Link(Get->Pins[2],Radius->FindPinChecked(TEXT("NewRadius")));
	check(Location->FindPinChecked(TEXT("self"))->LinkedTo.Num()==1);
	Link(Location->FindPinChecked(TEXT("self"))->LinkedTo[0],Radius->FindPinChecked(TEXT("self")));
	Move(Location->FindPinChecked(TEXT("execute")),Radius->FindPinChecked(TEXT("execute")));
	Link(Radius->FindPinChecked(TEXT("then")),Location->FindPinChecked(TEXT("execute")));
	// Keep the old room function for reference, but remove its independent spawn pass.
	UK2Node_CallFunction *Room=nullptr,*Corridor=nullptr,*Routes=nullptr;
	for(UEdGraph* Graph:BP->UbergraphPages) for(UEdGraphNode* N:Graph->Nodes) if(auto* F=Cast<UK2Node_CallFunction>(N))
	{
		const FName Name=F->FunctionReference.GetMemberName();
		if(Name==TEXT("Generate_Room_Lights")) Room=F;
		if(Name==TEXT("Generate_Corridor_Lights")) Corridor=F;
		if(Name==TEXT("Ensure_reserved_routes")) Routes=F;
	}
	check(Room && Corridor && Routes);
	Room->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();Room->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
	Room->NodeComment=TEXT("Disabled: region planner now covers both rooms and corridors.");Room->bCommentBubbleVisible=true;
	Corridor->FindPinChecked(TEXT("execute"))->BreakAllPinLinks();
	Move(Routes->FindPinChecked(TEXT("then")),Corridor->FindPinChecked(TEXT("then")));
	Link(Routes->FindPinChecked(TEXT("then")),Corridor->FindPinChecked(TEXT("execute")));
	Corridor->NodeComment=TEXT("All floor regions: one light per region, after final route geometry.");Corridor->bCommentBubbleVisible=true;
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
	FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Log);
	if(Log.NumErrors) { UE_LOG(LogTemp,Error,TEXT("REGION_INTEGRATION failed: %d errors; not saved"),Log.NumErrors);FPlatformMisc::RequestExit(false);return; }
	FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
	bool Saved=UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
	UE_LOG(LogTemp,Display,TEXT("REGION_INTEGRATION saved=%d warnings=%d"),Saved,Log.NumWarnings);
	FPlatformMisc::RequestExit(false);
 }));
#endif
