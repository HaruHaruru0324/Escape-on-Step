#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"

namespace CourtyardStaggerEditor
{
void Apply()
{
 auto* BP=LoadObject<UBlueprint>(nullptr,TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_ISM.BP_Courtyard_Generator_ISM"));
 if(!BP)return;
 for(const FName GraphName:{FName(TEXT("BuildBentNode")),FName(TEXT("CanAddDetail"))})
 {
 UEdGraph* G=nullptr;
 for(UEdGraph* Graph:BP->FunctionGraphs)if(Graph->GetFName()==GraphName){G=Graph;break;}
 check(G);
 UK2Node_VariableSet* SetOrigin=nullptr;UK2Node_FunctionEntry* Entry=nullptr;
 bool AlreadyPatched=false;
 for(UEdGraphNode* N:G->Nodes)
 {
  if(auto* C=Cast<UK2Node_CallFunction>(N);C&&C->FunctionReference.GetMemberName()==TEXT("GetStaggeredOrigin"))AlreadyPatched=true;
  if(auto* V=Cast<UK2Node_VariableSet>(N);V&&V->VariableReference.GetMemberName()==TEXT("origin"))SetOrigin=V;
  if(auto* E=Cast<UK2Node_FunctionEntry>(N))Entry=E;
 }
 if(AlreadyPatched)continue;
 check(SetOrigin&&Entry);
 auto* Input=SetOrigin->FindPinChecked(TEXT("origin"));check(Input->LinkedTo.Num()==1);
 auto* Original=Input->LinkedTo[0];
 auto* Call=NewObject<UK2Node_CallFunction>(G);G->AddNode(Call,false,false);Call->CreateNewGuid();
 Call->SetFromFunction(UCourtyardLayoutLibrary::StaticClass()->FindFunctionByName(TEXT("GetStaggeredOrigin")));Call->AllocateDefaultPins();
 Call->NodePosX=SetOrigin->NodePosX-320;Call->NodePosY=SetOrigin->NodePosY+500;
 Call->NodeComment=TEXT("Offset rows/columns by one cell; keep short straight tunnels and grid collision.");Call->bCommentBubbleVisible=true;
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();
 Input->BreakAllPinLinks();
 check(Schema->TryCreateConnection(Original,Call->FindPinChecked(TEXT("Origin"))));
 check(Schema->TryCreateConnection(Call->FindPinChecked(TEXT("ReturnValue")),Input));
 auto* Id=Entry->FindPin(TEXT("Id"));if(!Id)Id=Entry->FindPinChecked(TEXT("NodeId"));
 check(Schema->TryCreateConnection(Id,Call->FindPinChecked(TEXT("Id"))));
 int32 Y=0;
 for(const auto& Pair:TArray<TPair<FName,FName>>{{TEXT("Columns"),TEXT("Columns")},{TEXT("WorkSeed"),TEXT("Seed")},{TEXT("WorkSettings"),TEXT("Settings")}})
 {
  auto* Get=NewObject<UK2Node_VariableGet>(G);G->AddNode(Get,false,false);Get->CreateNewGuid();
  Get->VariableReference.SetSelfMember(Pair.Key);Get->AllocateDefaultPins();
  Get->NodePosX=Call->NodePosX-260;Get->NodePosY=Call->NodePosY+Y;Y+=100;
  check(Schema->TryCreateConnection(Get->FindPinChecked(Pair.Key),Call->FindPinChecked(Pair.Value)));
 }
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Results;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::None,&Results);check(Results.NumErrors==0);
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
 check(UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
 UE_LOG(LogTemp,Display,TEXT("COURTYARD_STAGGER_APPLIED"));
}
static FAutoConsoleCommand Command(TEXT("CourtyardStagger.Apply"),TEXT("Add staggered origins to the owned ISM layout Blueprint."),FConsoleCommandDelegate::CreateStatic(&Apply));
}
#endif
