#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
static FAutoConsoleCommand DumpBackroomProps(TEXT("BackroomProps.Dump"),TEXT("Export renderer graphs without modifying them."),FConsoleCommandDelegate::CreateLambda([]()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer"));
 if(!BP) return;
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("BackroomPhysicsProps/Graphs");IFileManager::Get().MakeDirectory(*Dir,true);
 TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);
 for(auto* G:Graphs){TSet<UObject*> Nodes;for(UEdGraphNode* N:G->Nodes)Nodes.Add(N);FString Text;FEdGraphUtilities::ExportNodesToText(Nodes,Text);FFileHelper::SaveStringToFile(Text,*(Dir/(G->GetName()+TEXT(".txt"))));}
}));
#endif


