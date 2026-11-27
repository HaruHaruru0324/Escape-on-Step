#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_IfThenElse.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"

namespace CourtyardOrganicEditor
{
void Apply()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_ISM.BP_Courtyard_Generator_ISM"));check(BP);
 UEdGraph* Graph=nullptr;for(UEdGraph* G:BP->FunctionGraphs)if(G->GetName()==TEXT("GenerateCourtyardBP"))Graph=G;check(Graph);
 UK2Node_FunctionEntry* Entry=nullptr;UK2Node_FunctionResult* ExistingResult=nullptr;
 for(UEdGraphNode* N:Graph->Nodes)
 {
  if(auto* Call=Cast<UK2Node_CallFunction>(N);Call&&Call->FunctionReference.GetMemberName()==TEXT("GenerateOrganicCourtyard")){UE_LOG(LogTemp,Display,TEXT("ORGANIC_BP_ALREADY_INSTALLED"));return;}
  if(auto* E=Cast<UK2Node_FunctionEntry>(N))Entry=E;
  if(auto* R=Cast<UK2Node_FunctionResult>(N))ExistingResult=R;
 }
 check(Entry&&ExistingResult);auto* Start=Entry->FindPinChecked(TEXT("then"));check(Start->LinkedTo.Num()==1);auto* Old=Start->LinkedTo[0];
 // Preserve the existing implementation on the lower (False) lane.
 for(UEdGraphNode* N:Graph->Nodes)if(N!=Entry)N->NodePosY+=1600;
 Entry->NodePosX=-1520;Entry->NodePosY=0;
 auto* Branch=NewObject<UK2Node_IfThenElse>(Graph);Graph->AddNode(Branch,false,false);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=-760;Branch->NodePosY=0;
 auto* Get=NewObject<UK2Node_VariableGet>(Graph);Graph->AddNode(Get,false,false);Get->CreateNewGuid();Get->VariableReference.SetSelfMember(TEXT("bUseOrganicLayout"));Get->AllocateDefaultPins();Get->NodePosX=-1100;Get->NodePosY=320;
 auto* Call=NewObject<UK2Node_CallFunction>(Graph);Graph->AddNode(Call,false,false);Call->CreateNewGuid();Call->SetFromFunction(ACourtyardISMGenerator::StaticClass()->FindFunctionByName(TEXT("GenerateOrganicCourtyard")));Call->AllocateDefaultPins();Call->NodePosX=0;Call->NodePosY=0;
 Call->NodeComment=TEXT("Scatter irregular rooms; connect short tunnels and branching alleys. Validate loops before ISM rendering.");Call->bCommentBubbleVisible=true;
 auto* Result=DuplicateObject<UK2Node_FunctionResult>(ExistingResult,Graph);Graph->AddNode(Result,false,false);Result->CreateNewGuid();for(auto* P:Result->Pins)P->LinkedTo.Reset();Result->NodePosX=760;Result->NodePosY=0;
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();
 const auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){check(Schema->TryCreateConnection(A,B));};
 Start->BreakAllPinLinks();Link(Start,Branch->GetExecPin());Link(Get->FindPinChecked(TEXT("bUseOrganicLayout")),Branch->GetConditionPin());
 Link(Branch->GetThenPin(),Call->FindPinChecked(TEXT("execute")));Link(Branch->GetElsePin(),Old);
 Link(Call->FindPinChecked(TEXT("then")),Result->FindPinChecked(TEXT("execute")));Link(Call->FindPinChecked(TEXT("ReturnValue")),Result->FindPinChecked(TEXT("Value")));
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);check(Results.NumErrors==0);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 UE_LOG(LogTemp,Display,TEXT("ORGANIC_BP_INSTALLED"));
}
static FAutoConsoleCommand Command(TEXT("CourtyardOrganic.Apply"),TEXT("Install the optional organic layout entry on the owned ISM Blueprint."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
