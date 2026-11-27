#if WITH_EDITOR
#include "CoreMinimal.h"
#include "Generation/Backroom/Lighting/BackroomSynchronizedFlicker.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Engine/Blueprint.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Kismet/KismetMathLibrary.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"

// Editor-only authoring: the game executes the saved Blueprint and GPU materials.
namespace BackroomFlickerEditor
{
static const TCHAR* Asset=TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Backroom_Renderer.BP_Backroom_Renderer");
static const TCHAR* FunctionName=TEXT("ConfigureBackroomFlicker");
void Apply()
{
 UBlueprint* BP=LoadObject<UBlueprint>(nullptr,Asset);
 check(BP&&BP->GeneratedClass);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("BackroomFlicker");
 IFileManager::Get().MakeDirectory(*Dir,true);
 UEdGraph* Generate=nullptr;
 for(UEdGraph* G:BP->FunctionGraphs)if(G->GetName()==TEXT("Generate_Corridor_Lights"))Generate=G;
 check(Generate);
 UK2Node_CallFunction* SetMaterial=nullptr; UEdGraphNode* AddLight=nullptr; UK2Node_CallFunction* Installed=nullptr;
 for(UEdGraphNode* N:Generate->Nodes)
 {
  if(auto* F=Cast<UK2Node_CallFunction>(N))
  {
   if(F->FunctionReference.GetMemberName()==TEXT("SetMaterial"))SetMaterial=F;
   if(F->FunctionReference.GetMemberName()==FunctionName)Installed=F;
  }
  if(N->GetFName()==TEXT("K2Node_AddComponent_0"))AddLight=N;
 }
 check(SetMaterial&&AddLight);
 auto* LightOut=AddLight->FindPinChecked(TEXT("ReturnValue"));
 auto* MeshInput=SetMaterial->FindPinChecked(TEXT("self"));
 check(MeshInput->LinkedTo.Num()==1);
 auto* MeshOut=MeshInput->LinkedTo[0];
 UEdGraph* G=nullptr;
 for(UEdGraph* Graph:BP->FunctionGraphs)if(Graph->GetName()==FunctionName)G=Graph;
 if(!G)
 {
  G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(FunctionName),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
  FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP,G,false,nullptr);
 }
 UK2Node_FunctionEntry* Entry=nullptr;
 auto Old=G->Nodes;
 for(UEdGraphNode* N:Old)
 {
  N->BreakAllNodeLinks();
  if(auto* E=Cast<UK2Node_FunctionEntry>(N))Entry=E;
  else G->RemoveNode(N);
 }
 check(Entry);
 auto Obj=[](UClass* Class){FEdGraphPinType T;T.PinCategory=UEdGraphSchema_K2::PC_Object;T.PinSubCategoryObject=Class;return T;};
 if(!Entry->FindPin(TEXT("Light")))Entry->CreateUserDefinedPin(TEXT("Light"),Obj(ULightComponent::StaticClass()),EGPD_Output,false);
 if(!Entry->FindPin(TEXT("Fixture")))Entry->CreateUserDefinedPin(TEXT("Fixture"),Obj(UStaticMeshComponent::StaticClass()),EGPD_Output,false);
 FEdGraphPinType Real;Real.PinCategory=UEdGraphSchema_K2::PC_Real;Real.PinSubCategory=UEdGraphSchema_K2::PC_Double;
 if(!Entry->FindPin(TEXT("Chance")))Entry->CreateUserDefinedPin(TEXT("Chance"),Real,EGPD_Output,false);
 if(!BP->NewVariables.ContainsByPredicate([](const FBPVariableDescription& V){return V.VarName==TEXT("FlickerProbability");}))
 {
  FBlueprintEditorUtils::AddMemberVariable(BP,TEXT("FlickerProbability"),Real,TEXT("0.2"));
  FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,TEXT("FlickerProbability"),false);
  FBlueprintEditorUtils::SetBlueprintVariableCategory(BP,TEXT("FlickerProbability"),nullptr,FText::FromString(TEXT("Lighting|Flicker")));
  FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("FlickerProbability"),nullptr,TEXT("ClampMin"),TEXT("0"));
  FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("FlickerProbability"),nullptr,TEXT("ClampMax"),TEXT("1"));
  FBlueprintEditorUtils::SetBlueprintVariableMetaData(BP,TEXT("FlickerProbability"),nullptr,TEXT("ToolTip"),TEXT("Probability per generated light: 0 = steady, 1 = all flickering. Mesh and illumination share the same decision."));
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
 const auto* Schema=GetDefault<UEdGraphSchema_K2>();
 auto Link=[&](UEdGraphPin* A,UEdGraphPin* B){check(A&&B);check(Schema->TryCreateConnection(A,B));};
 auto Call=[&](UClass* Class,const TCHAR* Name,int32 X,int32 Y)
 {
  auto* N=NewObject<UK2Node_CallFunction>(G);G->AddNode(N,false,false);N->CreateNewGuid();
  N->SetFromFunction(Class->FindFunctionByName(Name));N->AllocateDefaultPins();N->NodePosX=X;N->NodePosY=Y;return N;
 };
 auto* Random=Call(UKismetMathLibrary::StaticClass(),TEXT("RandomFloat"),0,200);
 auto* Less=Call(UKismetMathLibrary::StaticClass(),TEXT("Less_DoubleDouble"),240,200);
 Link(Random->FindPinChecked(TEXT("ReturnValue")),Less->FindPinChecked(TEXT("A")));
 Link(Entry->FindPinChecked(TEXT("Chance")),Less->FindPinChecked(TEXT("B")));
 auto* Branch=NewObject<UK2Node_IfThenElse>(G);G->AddNode(Branch,false,false);Branch->CreateNewGuid();Branch->AllocateDefaultPins();Branch->NodePosX=480;
 Link(Entry->FindPinChecked(TEXT("then")),Branch->FindPinChecked(TEXT("execute")));
 Link(Less->FindPinChecked(TEXT("ReturnValue")),Branch->FindPinChecked(TEXT("Condition")));
 Branch->NodeComment=TEXT("1灯につき1回だけ抽選。発光メッシュと照明を同じ分岐で設定する。");Branch->bCommentBubbleVisible=true;
 Entry->NodePosX=-500;Entry->NodeComment=TEXT("点滅の計算は共通のMF_BackroomFlicker。通常光は明るさ1で固定。");Entry->bCommentBubbleVisible=true;
 const FString Folder=TEXT("/Game/AutoCreateMap/Room/AutoMapMesh/MaterialInstances/Flicker/");
 for(int32 Mode=0;Mode<2;++Mode)
 {
  const bool Flicker=Mode==0;const int32 Y=Flicker?-100:400;
  auto* Light=Call(ULightComponent::StaticClass(),TEXT("SetLightFunctionMaterial"),800,Y);
  auto* Mesh=Call(UPrimitiveComponent::StaticClass(),TEXT("SetMaterial"),1200,Y);
  Link(Branch->FindPinChecked(Flicker?TEXT("then"):TEXT("else")),Light->FindPinChecked(TEXT("execute")));
  Link(Entry->FindPinChecked(TEXT("Light")),Light->FindPinChecked(TEXT("self")));
  Link(Light->FindPinChecked(TEXT("then")),Mesh->FindPinChecked(TEXT("execute")));
  Link(Entry->FindPinChecked(TEXT("Fixture")),Mesh->FindPinChecked(TEXT("self")));
  const FString MeshName=Flicker?TEXT("MI_BackroomFixtureFlicker"):TEXT("MI_BackroomFixtureSteady");
  UObject* MeshMaterial=LoadObject<UMaterialInterface>(nullptr,*(Folder+MeshName+TEXT(".")+MeshName));check(MeshMaterial);
  Schema->TrySetDefaultObject(*Mesh->FindPinChecked(TEXT("Material")),MeshMaterial);
  if(Flicker)
  {
   auto* Material=LoadObject<UMaterialInterface>(nullptr,*(Folder+TEXT("MI_BackroomLightFlicker.MI_BackroomLightFlicker")));check(Material);
   Schema->TrySetDefaultObject(*Light->FindPinChecked(TEXT("NewLightFunctionMaterial")),Material);
  }
  auto* Return=NewObject<UK2Node_FunctionResult>(G);G->AddNode(Return,false,false);Return->CreateNewGuid();Return->FunctionReference=Entry->FunctionReference;Return->AllocateDefaultPins();Return->NodePosX=1650;Return->NodePosY=Y;
  auto* Sync=Call(UBackroomSynchronizedFlicker::StaticClass(),TEXT("SetSynchronizedPair"),1500,Y);
  Link(Mesh->FindPinChecked(TEXT("then")),Sync->FindPinChecked(TEXT("execute")));
  Link(Entry->FindPinChecked(TEXT("Light")),Sync->FindPinChecked(TEXT("Light")));
  Link(Entry->FindPinChecked(TEXT("Fixture")),Sync->FindPinChecked(TEXT("Fixture")));
  Schema->TrySetDefaultValue(*Sync->FindPinChecked(TEXT("bFlicker")),Flicker?TEXT("true"):TEXT("false"));
  Return->NodePosX=1950;
  Link(Sync->FindPinChecked(TEXT("then")),Return->FindPinChecked(TEXT("execute")));
 }
 FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(G,FText::FromString(TEXT("Lighting|Flicker")),true);
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
 if(!Installed)
 {
  Installed=NewObject<UK2Node_CallFunction>(Generate);Generate->AddNode(Installed,false,false);Installed->CreateNewGuid();
  Installed->SetFromFunction(BP->SkeletonGeneratedClass->FindFunctionByName(FunctionName));Installed->FunctionReference.SetSelfMember(FunctionName);Installed->AllocateDefaultPins();
  Installed->NodePosX=SetMaterial->NodePosX+350;Installed->NodePosY=SetMaterial->NodePosY;
  auto Next=SetMaterial->FindPinChecked(TEXT("then"))->LinkedTo;
  SetMaterial->FindPinChecked(TEXT("then"))->BreakAllPinLinks();
  Link(SetMaterial->FindPinChecked(TEXT("then")),Installed->FindPinChecked(TEXT("execute")));
  for(auto* Pin:Next)Link(Installed->FindPinChecked(TEXT("then")),Pin);
  Link(LightOut,Installed->FindPinChecked(TEXT("Light")));
  Link(MeshOut,Installed->FindPinChecked(TEXT("Fixture")));
  auto* Probability=NewObject<UK2Node_VariableGet>(Generate);Generate->AddNode(Probability,false,false);Probability->CreateNewGuid();Probability->VariableReference.SetSelfMember(TEXT("FlickerProbability"));Probability->AllocateDefaultPins();
  Probability->NodePosX=Installed->NodePosX;Probability->NodePosY=Installed->NodePosY+230;
  Link(Probability->FindPinChecked(TEXT("FlickerProbability")),Installed->FindPinChecked(TEXT("Chance")));
  Installed->NodeComment=TEXT("発光メッシュとRect Lightを同期。点滅率はFlickerProbability。");Installed->bCommentBubbleVisible=true;
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
 FString Report=FString::Printf(TEXT("errors=%d warnings=%d\n"),Log.NumErrors,Log.NumWarnings);
 for(auto& Message:Log.Messages)Report+=Message->ToText().ToString()+TEXT("\n");
 if(!Log.NumErrors)
 {
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  const bool Saved=UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
  Report+=FString::Printf(TEXT("saved=%d\n"),Saved);
 }
 FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("apply_result.txt")));
 UE_LOG(LogTemp,Display,TEXT("BACKROOM_FLICKER %s"),*Report);
}
}
static FAutoConsoleCommand ApplyBackroomFlicker(TEXT("BackroomFlicker.Apply"),TEXT("Install synchronized probabilistic light flicker into the renderer Blueprint."),FConsoleCommandDelegate::CreateStatic(&BackroomFlickerEditor::Apply));
#endif

