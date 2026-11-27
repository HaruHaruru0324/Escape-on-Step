#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"

static FAutoConsoleCommand DumpBackroomBP(TEXT("RemoteEndRepair.Dump"), TEXT("Inspect the open Blueprint for graph editing."),
 FConsoleCommandDelegate::CreateLambda([]()
 {
  UBlueprint* BP = LoadObject<UBlueprint>(nullptr, TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Dungeon_Generator_BackRoom.BP_Dungeon_Generator_BackRoom"));
  if (!BP) { UE_LOG(LogTemp, Error, TEXT("BACKROOM_BP blueprint missing")); return; }
  const FString Directory = FPaths::ProjectSavedDir() / TEXT("RemoteEndRepair");
  IFileManager::Get().MakeDirectory(*Directory, true);
  FString Report;
  for (TFieldIterator<FProperty> It(BP->GeneratedClass); It; ++It)
  {
   FProperty* P = *It;
   Report += P->GetName() + TEXT(" ") + P->GetCPPType() + TEXT("\n");
  }
  TArray<UEdGraph*> Graphs;
  BP->GetAllGraphs(Graphs);
  for (UEdGraph* Graph : Graphs)
  {
   TSet<UObject*> Nodes;
   for (UEdGraphNode* Node : Graph->Nodes) Nodes.Add(Node);
   FString Export;
   FEdGraphUtilities::ExportNodesToText(Nodes, Export);
   FFileHelper::SaveStringToFile(Export, *(Directory / (Graph->GetName() + TEXT(".t3d"))));
   Report += TEXT("GRAPH ") + Graph->GetName() + FString::Printf(TEXT(" nodes=%d\n"), Graph->Nodes.Num());
  }
  for (TObjectIterator<UScriptStruct> It; It; ++It)
  {
   if (!It->GetPathName().StartsWith(TEXT("/Game/AutoCreateMap/Room/"))) continue;
   Report += TEXT("STRUCT ") + It->GetPathName() + TEXT("\n");
   for (TFieldIterator<FProperty> P(*It); P; ++P)
    Report += TEXT("  ") + P->GetName() + TEXT(" ") + P->GetCPPType() + TEXT("\n");
  }
  FFileHelper::SaveStringToFile(Report, *(Directory / TEXT("schema.txt")));
  UE_LOG(LogTemp, Display, TEXT("BACKROOM_BP dump saved %s"), *Directory);
 }));

#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_BreakStruct.h"
#include "K2Node_GetArrayItem.h"
#include "K2Node_MakeArray.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "EdGraphNode_Comment.h"
#include "Logging/TokenizedMessage.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "K2Node_Event.h"
#include "K2Node_SpawnActorFromClass.h"
#include "K2Node_Self.h"

namespace BackroomEditor
{
const FString AssetPath = TEXT("/Game/AutoCreateMap/Room/AutoMapData/Blueprints/BP_Dungeon_Generator_BackRoom.BP_Dungeon_Generator_BackRoom");
UScriptStruct* Struct(const FString& Name)
{
 if (Name == TEXT("point")) return TBaseStructure<FIntPoint>::Get();
 if (Name == TEXT("vector")) return TBaseStructure<FVector>::Get();
 if (Name == TEXT("transform")) return TBaseStructure<FTransform>::Get();
 return LoadObject<UScriptStruct>(nullptr, *(TEXT("/Game/AutoCreateMap/Room/AutoMapData/StructureData/ST_") + Name + TEXT(".ST_") + Name));
}
FEdGraphPinType Type(FString Name)
{
 FEdGraphPinType T;
 if(Name.Contains(TEXT(":")))
 {
  FString Key,Value;Name.Split(TEXT(":"),&Key,&Value);
  T=Type(Key);T.ContainerType=EPinContainerType::Map;
  const auto V=Type(Value);T.PinValueType.TerminalCategory=V.PinCategory;T.PinValueType.TerminalSubCategoryObject=V.PinSubCategoryObject;
  return T;
 }
 if (Name.EndsWith(TEXT("[]"))) { T.ContainerType=EPinContainerType::Array;Name.LeftChopInline(2); }
 if (Name.EndsWith(TEXT("{}"))) { T.ContainerType=EPinContainerType::Set;Name.LeftChopInline(2); }
 if(Name==TEXT("actor")){T.PinCategory=UEdGraphSchema_K2::PC_Object;T.PinSubCategoryObject=AActor::StaticClass();}
 else if(Name==TEXT("int")) T.PinCategory=UEdGraphSchema_K2::PC_Int;
 else if(Name==TEXT("bool")) T.PinCategory=UEdGraphSchema_K2::PC_Boolean;
 else if(Name==TEXT("real")) {T.PinCategory=UEdGraphSchema_K2::PC_Real;T.PinSubCategory=UEdGraphSchema_K2::PC_Double;}
 else {T.PinCategory=UEdGraphSchema_K2::PC_Struct;T.PinSubCategoryObject=Struct(Name);}
 return T;
}
UEdGraph* Graph(UBlueprint* BP, const FString& Name)
{
 for(UEdGraph* G:BP->FunctionGraphs)if(G->GetName()==Name)return G;
 UEdGraph* G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(*Name),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());
 FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP,G,false,nullptr);
 return G;
}
UK2Node_FunctionEntry* Entry(UEdGraph* G)
{
 for(UEdGraphNode* N:G->Nodes)if(auto* E=Cast<UK2Node_FunctionEntry>(N))return E;
 return nullptr;
}
UEdGraphPin* Pin(UEdGraphNode* N,const FString& Name)
{
 if(!N)return nullptr;
 if(Name==TEXT("$"))for(auto* P:N->Pins)if(P->Direction==EGPD_Output&&P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec&&P->PinName!=TEXT("self"))return P;
 for(auto* P:N->Pins)if(P->PinName.ToString()==Name)return P;
 for(auto* P:N->Pins)if(P->PinName.ToString().StartsWith(Name+TEXT("_")))return P;
 return nullptr;
}
template<class T> T* New(UEdGraph* G)
{
 T* N=NewObject<T>(G);G->AddNode(N,false,false);N->CreateNewGuid();return N;
}
void Apply()
{
 UBlueprint* BP=LoadObject<UBlueprint>(nullptr,*AssetPath);
 if(!BP)return;
 FString Source;const FString Dir=FPaths::ProjectSavedDir()/TEXT("RemoteEndRepair");
 if(!FFileHelper::LoadFileToString(Source,*(Dir/TEXT("edit.txt"))))return;
 const FString Backup=Dir/TEXT("BP_Dungeon_Generator_BackRoom.before.uasset");
 if(!IFileManager::Get().FileExists(*Backup))
  IFileManager::Get().Copy(*Backup,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()));
 TArray<FString> Lines;Source.ParseIntoArrayLines(Lines,true);

 // Restore the user's original initialization before appending the growth pass.
 for(const FString& Line:Lines)
 {
  TArray<FString> A;Line.ParseIntoArray(A,TEXT("|"),false);
  if(A[0]!=TEXT("graph"))continue;
  UEdGraph* Target=A[1]==TEXT("EventGraph")?BP->UbergraphPages[0].Get():Graph(BP,A[1]);
  if(A.Num()>2&&A[2]==TEXT("append"))
  {
   if(A[1]==TEXT("BuildAttempt"))
   {
    FString Original;
    if(FFileHelper::LoadFileToString(Original,*(Dir/TEXT("BuildAttempt.t3d"))))
    {
     auto Old=Target->Nodes;
     for(UEdGraphNode* Node:Old){Node->BreakAllNodeLinks();Target->RemoveNode(Node);}
     TSet<UEdGraphNode*> Imported;FEdGraphUtilities::ImportNodesFromText(Target,Original,Imported);
    }
   }
   continue;
  }
  auto Old=Target->Nodes;
  for(UEdGraphNode* Node:Old)
  {
   Node->BreakAllNodeLinks();
   if(!Cast<UK2Node_FunctionEntry>(Node)&&!Cast<UK2Node_FunctionResult>(Node))Target->RemoveNode(Node);
  }
 }
 Entry(Graph(BP,TEXT("AreRoomsConnected")))->LocalVariables.RemoveAll([](const FBPVariableDescription& V){return V.VarName==TEXT("Connected");});
 UEdGraph* LocalGraph=nullptr;
 for(const FString& Line:Lines)
 {
  TArray<FString> A;Line.ParseIntoArray(A,TEXT("|"),false);
  if(A[0]==TEXT("graph"))LocalGraph=A[1]==TEXT("EventGraph")?BP->UbergraphPages[0].Get():Graph(BP,A[1]);
  if(A[0]==TEXT("local")&&LocalGraph)
  {
   auto* E=Entry(LocalGraph);
   if(!E->LocalVariables.ContainsByPredicate([&](const FBPVariableDescription& V){return V.VarName==FName(*A[1]);}))
   {
    FBPVariableDescription V;V.VarName=FName(*A[1]);V.VarGuid=FGuid::NewGuid();V.VarType=Type(A[2]);V.FriendlyName=A[1];V.PropertyFlags=CPF_Edit|CPF_BlueprintVisible;E->LocalVariables.Add(V);
   }
  }
 }

 // Declare function signatures first so self calls use the actual Blueprint skeleton.
 for(const FString& Line:Lines)
 {
  TArray<FString> A;Line.ParseIntoArray(A,TEXT("|"),false);
  if(A[0]==TEXT("member"))FBlueprintEditorUtils::AddMemberVariable(BP,FName(*A[1]),Type(A[2]));
  if(A[0]==TEXT("component"))
  {
   if(!BP->SimpleConstructionScript->FindSCSNode(FName(*A[1])))
   {
    USCS_Node* Node=BP->SimpleConstructionScript->CreateNode(UInstancedStaticMeshComponent::StaticClass(),FName(*A[1]));
    auto* Mesh=CastChecked<UInstancedStaticMeshComponent>(Node->ComponentTemplate);
    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,A.Num()>3?*A[3]:TEXT("/Engine/BasicShapes/Cube.Cube")));
    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
    if(A.Num()>2&&!A[2].IsEmpty())Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*A[2]));
    BP->SimpleConstructionScript->AddNode(Node);
   }
  }
  if(A[0]==TEXT("declare"))Graph(BP,A[1]);
  if(A[0]==TEXT("input")&&!Pin(Entry(Graph(BP,A[1])),A[2]))Entry(Graph(BP,A[1]))->CreateUserDefinedPin(FName(*A[2]),Type(A[3]),EGPD_Output);
  if(A[0]==TEXT("output"))
  {
   UEdGraph* G=Graph(BP,A[1]);UK2Node_FunctionResult* R=nullptr;
   for(UEdGraphNode* N:G->Nodes)if(auto* Result=Cast<UK2Node_FunctionResult>(N))R=Result;
   if(!R){R=New<UK2Node_FunctionResult>(G);R->FunctionReference=Entry(G)->FunctionReference;R->AllocateDefaultPins();}
   if(!Pin(R,A[2]))R->CreateUserDefinedPin(FName(*A[2]),Type(A[3]),EGPD_Input);
  }
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave);
 UEdGraph* G=nullptr;TMap<FString,UEdGraphNode*> Nodes;int32 Index=0;int32 Errors=0;
 auto Fail=[&](const FString& Msg){++Errors;UE_LOG(LogTemp,Error,TEXT("BACKROOM_BP %s"),*Msg);};
 auto Resolve=[&](const FString& Ref)->UEdGraphPin*
 {
  FString Id,Name;if(!Ref.Split(TEXT("."),&Id,&Name))return nullptr;
  return Pin(Nodes.FindRef(Id),Name);
 };
 for(const FString& Line:Lines)
 {
  TArray<FString> A;Line.ParseIntoArray(A,TEXT("|"),false);if(A.IsEmpty())continue;
  const FString Op=A[0];
  if(Op==TEXT("graph"))
  {
   G=A[1]==TEXT("EventGraph")?BP->UbergraphPages[0].Get():Graph(BP,A[1]);G->Modify();Nodes.Empty();Index=0;
   if(A.Num()>2&&A[2]==TEXT("append"))
   {
    Nodes.Add(TEXT("entry"),Entry(G));continue;
   }
   auto Existing=G->Nodes;
   for(UEdGraphNode* N:Existing)
   {
    if(auto* E=Cast<UK2Node_FunctionEntry>(N)){Nodes.Add(TEXT("entry"),E);E->BreakAllNodeLinks();continue;}
    if(auto* R=Cast<UK2Node_FunctionResult>(N)){Nodes.Add(TEXT("result"),R);R->BreakAllNodeLinks();continue;}
    N->BreakAllNodeLinks();G->RemoveNode(N);
   }
   continue;
  }
  if(Op==TEXT("existing"))
  {
   for(UEdGraphNode* N:G->Nodes)
   {
    if(A[2]==TEXT("name")&&N->GetName()==A[3])Nodes.Add(A[1],N);
    if(A[2]==TEXT("call")){if(auto* C=Cast<UK2Node_CallFunction>(N);C&&C->FunctionReference.GetMemberName()==FName(*A[3]))Nodes.Add(A[1],N);}
    if(A[2]==TEXT("event")){if(auto* E=Cast<UK2Node_Event>(N);E&&E->EventReference.GetMemberName()==FName(*A[3]))Nodes.Add(A[1],N);}
   }
   continue;
  }
  if(Op==TEXT("local"))
  {
   auto* E=Entry(G);if(!E->LocalVariables.ContainsByPredicate([&](const FBPVariableDescription& V){return V.VarName==FName(*A[1]);}))
   {FBPVariableDescription V;V.VarName=FName(*A[1]);V.VarGuid=FGuid::NewGuid();V.VarType=Type(A[2]);V.FriendlyName=A[1];V.PropertyFlags=CPF_BlueprintVisible;E->LocalVariables.Add(V);}
   continue;
  }
  if(Op==TEXT("node"))
  {
   const FString Kind=A[2],Arg=A.Num()>3?A[3]:TEXT("");UEdGraphNode* N=nullptr;
   if(Kind==TEXT("get")||Kind==TEXT("set"))
   {
    auto* V=Kind==TEXT("get")?static_cast<UK2Node_Variable*>(New<UK2Node_VariableGet>(G)):static_cast<UK2Node_Variable*>(New<UK2Node_VariableSet>(G));
    if(const FBPVariableDescription* Local=Entry(G)->LocalVariables.FindByPredicate([&](const FBPVariableDescription& L){return L.VarName==FName(*Arg);}))
     V->VariableReference.SetLocalMember(FName(*Arg),G->GetName(),Local->VarGuid);
    else V->VariableReference.SetSelfMember(FName(*Arg));
    N=V;
   }
   else if(Kind==TEXT("call")||Kind==TEXT("array"))
   {
    FString ClassName,Fn;UClass* Owner=nullptr;
    if(Arg.Split(TEXT(":"),&ClassName,&Fn))Owner=LoadObject<UClass>(nullptr,*(TEXT("/Script/")+ClassName));
    else {Fn=Arg;Owner=BP->SkeletonGeneratedClass;}
    UFunction* Function=Owner?Owner->FindFunctionByName(FName(*Fn)):nullptr;
    if(!Function){Fail(TEXT("Missing function ")+Arg);continue;}
    auto* C=Kind==TEXT("array")?static_cast<UK2Node_CallFunction*>(New<UK2Node_CallArrayFunction>(G)):New<UK2Node_CallFunction>(G);
    C->SetFromFunction(Function);if(Owner==BP->SkeletonGeneratedClass)C->FunctionReference.SetSelfMember(FName(*Fn));N=C;
   }
   else if(Kind==TEXT("spawn"))N=New<UK2Node_SpawnActorFromClass>(G);
   else if(Kind==TEXT("self"))N=New<UK2Node_Self>(G);
   else if(Kind==TEXT("branch"))N=New<UK2Node_IfThenElse>(G);
   else if(Kind==TEXT("macro"))
   {
    auto* M=New<UK2Node_MacroInstance>(G);
    UBlueprint* Macros=LoadObject<UBlueprint>(nullptr,TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
    TArray<UEdGraph*> All;Macros->GetAllGraphs(All);
    for(auto* MG:All)if(MG->GetName()==Arg)M->SetMacroGraph(MG);
    N=M;
   }
   else if(Kind==TEXT("make")){auto* M=New<UK2Node_MakeStruct>(G);M->StructType=Struct(Arg);N=M;}
   else if(Kind==TEXT("break")){auto* M=New<UK2Node_BreakStruct>(G);M->StructType=Struct(Arg);N=M;}
   else if(Kind==TEXT("item"))N=New<UK2Node_GetArrayItem>(G);
   else if(Kind==TEXT("arraymake"))N=New<UK2Node_MakeArray>(G);
   if(!N){Fail(TEXT("Unknown node ")+Line);continue;}
   N->AllocateDefaultPins();N->NodePosX=(Index%8)*320;N->NodePosY=(Index/8)*300;++Index;Nodes.Add(A[1],N);
   continue;
  }
  if(Op==TEXT("unlink")){if(auto* P=Resolve(A[1]))P->BreakAllPinLinks();else Fail(TEXT("Missing unlink ")+Line);continue;}
  if(Op==TEXT("link"))
  {
   UEdGraphPin* From=Resolve(A[1]);UEdGraphPin* To=Resolve(A[2]);
   if(!From||!To){Fail(TEXT("Missing pin ")+Line);continue;}
   if(!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From,To))Fail(TEXT("Cannot connect ")+Line);
   continue;
  }
  if(Op==TEXT("default"))
  {
   if(auto* P=Resolve(A[1]))GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P,A[2]);
   else Fail(TEXT("Missing default ")+Line);
   continue;
  }
  if(Op==TEXT("object"))
  {
   if(auto* P=Resolve(A[1]))GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(*P,LoadObject<UObject>(nullptr,*A[2]));
   else Fail(TEXT("Missing object pin ")+Line);
   continue;
  }
  if(Op==TEXT("type"))
  {
   if(auto* P=Resolve(A[1])){P->PinType=Type(A[2]);P->GetOwningNode()->PinTypeChanged(P);}
   continue;
  }
  if(Op==TEXT("editable")){FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP,FName(*A[1]),false);continue;}
  if(Op==TEXT("position")){if(auto* N=Nodes.FindRef(A[1])){N->NodePosX=FCString::Atoi(*A[2]);N->NodePosY=FCString::Atoi(*A[3]);}continue;}
  if(Op==TEXT("comment")){if(auto* N=Nodes.FindRef(A[1])){N->NodeComment=A[2];N->bCommentBubbleVisible=true;}continue;}
 }
 FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
 FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(BP,EBlueprintCompileOptions::SkipSave,&Log);
 FString Report=FString::Printf(TEXT("editErrors=%d compileErrors=%d warnings=%d\n"),Errors,Log.NumErrors,Log.NumWarnings);
 for(const auto& Message:Log.Messages)Report+=Message->ToText().ToString()+TEXT("\n");
 FFileHelper::SaveStringToFile(Report,*(Dir/TEXT("apply-result.txt")));
 if(!Errors&&!Log.NumErrors)
 {
  for(const FString& Line:Lines)
  {
   TArray<FString> A;Line.ParseIntoArray(A,TEXT("|"),false);
   if(A[0]==TEXT("value"))
   {
    if(FProperty* P=BP->GeneratedClass->FindPropertyByName(FName(*A[1])))
     P->ImportText_Direct(*A[2],P->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()),BP->GeneratedClass->GetDefaultObject(),PPF_None);
   }
  }
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(BP->GetOutermost(),BP,*FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
 UE_LOG(LogTemp,Display,TEXT("BACKROOM_BP %s"),*Report);
}
}
static FAutoConsoleCommand ApplyBackroomBP(TEXT("RemoteEndRepair.Apply"),TEXT("Build Blueprint graph nodes from the reviewed editor plan."),FConsoleCommandDelegate::CreateStatic(&BackroomEditor::Apply));

#endif






