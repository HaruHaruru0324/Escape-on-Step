/**
 * BP生成版の編集用ノードを作成する補助。製品の実行時には使わない。
 * edit.txt は関数・ノード・ピン接続・既定値・座標の明示的な一覧。
 * Apply は複製した専用BPだけを編集し、コンパイルエラーがあれば保存しない。
 */
#if WITH_EDITOR
#include "CoreMinimal.h"
#include "HAL/IConsoleManager.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/UnrealType.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallArrayFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_IfThenElse.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_SetFieldsInStruct.h"
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

namespace CourtyardOrganicBPEditor
{
    const FString AssetPath =
        TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/"
             "BP_Courtyard_Generator_OrganicBP.BP_Courtyard_Generator_OrganicBP");

    UScriptStruct* Struct(const FString& Name)
    {
        if (Name == TEXT("vector"))
        {
            return TBaseStructure<FVector>::Get();
        }
        if (Name == TEXT("point"))
        {
            return TBaseStructure<FIntPoint>::Get();
        }
        if (Name == TEXT("stream"))
        {
            return TBaseStructure<FRandomStream>::Get();
        }
        const FString Native = TEXT("/Script/Escape_on_Step_Demo.Courtyard") + Name;
        return LoadObject<UScriptStruct>(nullptr, *Native);
    }

    FEdGraphPinType Type(FString Name)
    {
        FEdGraphPinType T;
        if (Name.Contains(TEXT(":")))
        {
            FString Key, Value;
            Name.Split(TEXT(":"), &Key, &Value);
            T = Type(Key);
            T.ContainerType = EPinContainerType::Map;
            const auto V = Type(Value);
            T.PinValueType.TerminalCategory = V.PinCategory;
            T.PinValueType.TerminalSubCategoryObject = V.PinSubCategoryObject;
            return T;
        }
        if (Name.EndsWith(TEXT("[]")))
        {
            T.ContainerType = EPinContainerType::Array;
            Name.LeftChopInline(2);
        }
        if (Name.EndsWith(TEXT("{}")))
        {
            T.ContainerType = EPinContainerType::Set;
            Name.LeftChopInline(2);
        }
        if (Name == TEXT("actor"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Object;
            T.PinSubCategoryObject = AActor::StaticClass();
        }
        else if (Name == TEXT("int"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Int;
        }
        else if (Name == TEXT("bool"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Boolean;
        }
        else if (Name == TEXT("real"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Real;
            T.PinSubCategory = UEdGraphSchema_K2::PC_Double;
        }
        else if (Name == TEXT("string"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_String;
        }
        else if (Name == TEXT("kind"))
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Byte;
            T.PinSubCategoryObject =
                LoadObject<UEnum>(nullptr, TEXT("/Script/Escape_on_Step_Demo.ECourtyardCellKind"));
        }
        else
        {
            T.PinCategory = UEdGraphSchema_K2::PC_Struct;
            T.PinSubCategoryObject = Struct(Name);
        }
        return T;
    }

    UEdGraph* Graph(UBlueprint* BP, const FString& Name)
    {
        for (UEdGraph* G : BP->FunctionGraphs)
        {
            if (G->GetName() == Name)
            {
                return G;
            }
        }
        UEdGraph* G = FBlueprintEditorUtils::CreateNewGraph(
            BP, FName(*Name), UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
        UFunction* ParentFunction = BP->ParentClass->FindFunctionByName(FName(*Name));
        if (ParentFunction)
        {
            FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, G, false, BP->ParentClass);
        }
        else
        {
            FBlueprintEditorUtils::AddFunctionGraph<UClass>(BP, G, false, nullptr);
        }
        return G;
    }

    UK2Node_FunctionEntry* Entry(UEdGraph* G)
    {
        for (UEdGraphNode* N : G->Nodes)
        {
            if (auto* E = Cast<UK2Node_FunctionEntry>(N))
            {
                return E;
            }
        }
        return nullptr;
    }

    UEdGraphPin* Pin(UEdGraphNode* N, const FString& Name)
    {
        if (!N)
        {
            return nullptr;
        }
        if (Name == TEXT("$"))
        {
            for (auto* P : N->Pins)
            {
                if (P->Direction == EGPD_Output &&
                    P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec &&
                    P->PinName != TEXT("self"))
                {
                    return P;
                }
            }
        }
        for (auto* P : N->Pins)
        {
            if (P->PinName.ToString() == Name)
            {
                return P;
            }
        }
        for (auto* P : N->Pins)
        {
            if (P->PinName.ToString().StartsWith(Name + TEXT("_")))
            {
                return P;
            }
        }
        return nullptr;
    }

    template <class T> T* New(UEdGraph* G)
    {
        T* N = NewObject<T>(G);
        G->AddNode(N, false, false);
        N->CreateNewGuid();
        return N;
    }

    void Apply()
    {
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr, *AssetPath);
        if (!BP)
        {
            return;
        }
        FString Source;
        const FString Dir = FPaths::ProjectSavedDir() / TEXT("CourtyardOrganicBP");
        if (!FFileHelper::LoadFileToString(Source, *(Dir / TEXT("edit.txt"))))
        {
            return;
        }
        const FString Backup = Dir / TEXT("BP_Courtyard_Generator_BP.before.uasset");
        if (!IFileManager::Get().FileExists(*Backup))
        {
            IFileManager::Get().Copy(*Backup,
                *FPackageName::LongPackageNameToFilename(
                    BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()));
        }
        TArray<FString> Lines;
        Source.ParseIntoArrayLines(Lines, true);

        // Restore the user's original initialization before appending the growth pass.
        for (const FString& Line : Lines)
        {
            TArray<FString> A;
            Line.ParseIntoArray(A, TEXT("|"), false);
            if (A[0] != TEXT("graph") || (A.Num() > 2 && A[2] == TEXT("append")))
            {
                continue;
            }
            UEdGraph* Target =
                A[1] == TEXT("EventGraph") ? BP->UbergraphPages[0].Get() : Graph(BP, A[1]);

            auto Old = Target->Nodes;
            bool KeptResult = false;
            for (UEdGraphNode* Node : Old)
            {
                Node->BreakAllNodeLinks();
                if (Cast<UK2Node_FunctionEntry>(Node))
                {
                    continue;
                }
                if (Cast<UK2Node_FunctionResult>(Node) && !KeptResult)
                {
                    KeptResult = true;
                    continue;
                }
                Target->RemoveNode(Node);
            }
        }
        UEdGraph* LocalGraph = nullptr;
        for (const FString& Line : Lines)
        {
            TArray<FString> A;
            Line.ParseIntoArray(A, TEXT("|"), false);
            if (A[0] == TEXT("graph"))
            {
                LocalGraph =
                    A[1] == TEXT("EventGraph") ? BP->UbergraphPages[0].Get() : Graph(BP, A[1]);
            }
            if (A[0] == TEXT("local") && LocalGraph)
            {
                auto* E = Entry(LocalGraph);
                if (!E->LocalVariables.ContainsByPredicate(
                        [&](const FBPVariableDescription& V)
                        {
                            return V.VarName == FName(*A[1]);
                        }))
                {
                    FBPVariableDescription V;
                    V.VarName = FName(*A[1]);
                    V.VarGuid = FGuid::NewGuid();
                    V.VarType = Type(A[2]);
                    V.FriendlyName = A[1];
                    V.PropertyFlags = CPF_Edit | CPF_BlueprintVisible;
                    E->LocalVariables.Add(V);
                }
            }
        }

        // Declare function signatures first so self calls use the actual Blueprint skeleton.
        for (const FString& Line : Lines)
        {
            TArray<FString> A;
            Line.ParseIntoArray(A, TEXT("|"), false);
            if (A[0] == TEXT("member"))
            {
                FBlueprintEditorUtils::AddMemberVariable(BP, FName(*A[1]), Type(A[2]));
            }
            if (A[0] == TEXT("component"))
            {
                if (!BP->SimpleConstructionScript->FindSCSNode(FName(*A[1])))
                {
                    USCS_Node* Node = BP->SimpleConstructionScript->CreateNode(
                        UInstancedStaticMeshComponent::StaticClass(), FName(*A[1]));
                    auto* Mesh =
                        CastChecked<UInstancedStaticMeshComponent>(Node->ComponentTemplate);
                    Mesh->SetStaticMesh(LoadObject<UStaticMesh>(
                        nullptr, A.Num() > 3 ? *A[3] : TEXT("/Engine/BasicShapes/Cube.Cube")));
                    Mesh->SetCollisionProfileName(TEXT("BlockAll"));
                    if (A.Num() > 2 && !A[2].IsEmpty())
                    {
                        Mesh->SetMaterial(0, LoadObject<UMaterialInterface>(nullptr, *A[2]));
                    }
                    BP->SimpleConstructionScript->AddNode(Node);
                }
            }
            if (A[0] == TEXT("declare"))
            {
                Graph(BP, A[1]);
            }
            if (A[0] == TEXT("input") && !Pin(Entry(Graph(BP, A[1])), A[2]))
            {
                Entry(Graph(BP, A[1]))->CreateUserDefinedPin(FName(*A[2]), Type(A[3]), EGPD_Output);
            }
            if (A[0] == TEXT("output"))
            {
                UEdGraph* G = Graph(BP, A[1]);
                UK2Node_FunctionResult* R = nullptr;
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (auto* Result = Cast<UK2Node_FunctionResult>(N))
                    {
                        R = Result;
                    }
                }
                if (!R)
                {
                    R = New<UK2Node_FunctionResult>(G);
                    R->FunctionReference = Entry(G)->FunctionReference;
                    R->AllocateDefaultPins();
                }
                if (!Pin(R, A[2]))
                {
                    R->CreateUserDefinedPin(FName(*A[2]), Type(A[3]), EGPD_Input);
                }
            }
        }
        for (const auto& Line : Lines)
        {
            TArray<FString> A;
            Line.ParseIntoArray(A, TEXT("|"), false);
            if (A[0] == TEXT("void"))
            {
                for (auto Node : Graph(BP, A[1])->Nodes)
                {
                    if (auto* Result = Cast<UK2Node_FunctionResult>(Node))
                    {
                        Result->UserDefinedPins.Empty();
                        Result->ReconstructNode();
                    }
                }
            }
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave);
        UEdGraph* G = nullptr;
        TMap<FString, UEdGraphNode*> Nodes;
        int32 Index = 0;
        int32 Errors = 0;
        auto Fail = [&](const FString& Msg)
        {
            ++Errors;
            UE_LOG(LogTemp, Error, TEXT("COURTYARD_BP %s"), *Msg);
        };
        auto Resolve = [&](const FString& Ref) -> UEdGraphPin*
        {
            FString Id, Name;
            if (!Ref.Split(TEXT("."), &Id, &Name))
            {
                return nullptr;
            }
            return Pin(Nodes.FindRef(Id), Name);
        };
        for (const FString& Line : Lines)
        {
            TArray<FString> A;
            Line.ParseIntoArray(A, TEXT("|"), false);
            if (A.IsEmpty())
            {
                continue;
            }
            const FString Op = A[0];
            if (Op == TEXT("graph"))
            {
                G = A[1] == TEXT("EventGraph") ? BP->UbergraphPages[0].Get() : Graph(BP, A[1]);
                G->Modify();
                Nodes.Empty();
                Index = 0;
                if (A.Num() > 2 && A[2] == TEXT("append"))
                {
                    Nodes.Add(TEXT("entry"), Entry(G));
                    continue;
                }
                auto Existing = G->Nodes;
                for (UEdGraphNode* N : Existing)
                {
                    if (auto* E = Cast<UK2Node_FunctionEntry>(N))
                    {
                        Nodes.Add(TEXT("entry"), E);
                        E->BreakAllNodeLinks();
                        continue;
                    }
                    if (auto* R = Cast<UK2Node_FunctionResult>(N))
                    {
                        Nodes.Add(TEXT("result"), R);
                        R->BreakAllNodeLinks();
                        continue;
                    }
                    N->BreakAllNodeLinks();
                    G->RemoveNode(N);
                }
                continue;
            }
            if (Op == TEXT("existing"))
            {
                for (UEdGraphNode* N : G->Nodes)
                {
                    if (A[2] == TEXT("name") && N->GetName() == A[3])
                    {
                        Nodes.Add(A[1], N);
                    }
                    if (A[2] == TEXT("call"))
                    {
                        if (auto* C = Cast<UK2Node_CallFunction>(N);
                            C && C->FunctionReference.GetMemberName() == FName(*A[3]))
                        {
                            Nodes.Add(A[1], N);
                        }
                    }
                    if (A[2] == TEXT("event"))
                    {
                        if (auto* E = Cast<UK2Node_Event>(N);
                            E && E->EventReference.GetMemberName() == FName(*A[3]))
                        {
                            Nodes.Add(A[1], N);
                        }
                    }
                }
                continue;
            }
            if (Op == TEXT("local"))
            {
                auto* E = Entry(G);
                if (!E->LocalVariables.ContainsByPredicate(
                        [&](const FBPVariableDescription& V)
                        {
                            return V.VarName == FName(*A[1]);
                        }))
                {
                    FBPVariableDescription V;
                    V.VarName = FName(*A[1]);
                    V.VarGuid = FGuid::NewGuid();
                    V.VarType = Type(A[2]);
                    V.FriendlyName = A[1];
                    V.PropertyFlags = CPF_BlueprintVisible;
                    E->LocalVariables.Add(V);
                }
                continue;
            }
            if (Op == TEXT("node"))
            {
                const FString Kind = A[2], Arg = A.Num() > 3 ? A[3] : TEXT("");
                UEdGraphNode* N = nullptr;
                if (Kind == TEXT("get") || Kind == TEXT("set"))
                {
                    auto* V = Kind == TEXT("get")
                                  ? static_cast<UK2Node_Variable*>(New<UK2Node_VariableGet>(G))
                                  : static_cast<UK2Node_Variable*>(New<UK2Node_VariableSet>(G));
                    if (const FBPVariableDescription* Local =
                            Entry(G)->LocalVariables.FindByPredicate(
                                [&](const FBPVariableDescription& L)
                                {
                                    return L.VarName == FName(*Arg);
                                }))
                    {
                        V->VariableReference.SetLocalMember(
                            FName(*Arg), G->GetName(), Local->VarGuid);
                    }
                    else
                    {
                        V->VariableReference.SetSelfMember(FName(*Arg));
                    }
                    N = V;
                }
                else if (Kind == TEXT("call") || Kind == TEXT("array"))
                {
                    FString ClassName, Fn;
                    UClass* Owner = nullptr;
                    if (Arg.Split(TEXT(":"), &ClassName, &Fn))
                    {
                        Owner = LoadObject<UClass>(nullptr, *(TEXT("/Script/") + ClassName));
                    }
                    else
                    {
                        Fn = Arg;
                        Owner = BP->SkeletonGeneratedClass;
                    }
                    UFunction* Function = Owner ? Owner->FindFunctionByName(FName(*Fn)) : nullptr;
                    if (!Function)
                    {
                        Fail(TEXT("Missing function ") + Arg);
                        continue;
                    }
                    auto* C = Kind == TEXT("array") ? static_cast<UK2Node_CallFunction*>(
                                                          New<UK2Node_CallArrayFunction>(G))
                                                    : New<UK2Node_CallFunction>(G);
                    C->SetFromFunction(Function);
                    if (Owner == BP->SkeletonGeneratedClass)
                    {
                        C->FunctionReference.SetSelfMember(FName(*Fn));
                    }
                    N = C;
                }
                else if (Kind == TEXT("event"))
                {
                    auto* E = New<UK2Node_Event>(G);
                    E->EventReference.SetExternalMember(FName(*Arg), AActor::StaticClass());
                    E->bOverrideFunction = true;
                    N = E;
                }
                else if (Kind == TEXT("result"))
                {
                    auto* R = New<UK2Node_FunctionResult>(G);
                    R->FunctionReference = Entry(G)->FunctionReference;
                    for (UEdGraphNode* Existing : G->Nodes)
                    {
                        if (auto* Template = Cast<UK2Node_FunctionResult>(Existing);
                            Template && Template != R &&
                            !BP->ParentClass->FindFunctionByName(G->GetFName()))
                        {
                            for (auto* P : Template->Pins)
                            {
                                if (P->Direction == EGPD_Input &&
                                    P->PinType.PinCategory != UEdGraphSchema_K2::PC_Exec)
                                {
                                    R->CreateUserDefinedPin(
                                        P->PinName, P->PinType, EGPD_Input, false);
                                }
                            }
                            break;
                        }
                    }
                    N = R;
                }
                else if (Kind == TEXT("spawn"))
                {
                    N = New<UK2Node_SpawnActorFromClass>(G);
                }
                else if (Kind == TEXT("self"))
                {
                    N = New<UK2Node_Self>(G);
                }
                else if (Kind == TEXT("branch"))
                {
                    N = New<UK2Node_IfThenElse>(G);
                }
                else if (Kind == TEXT("macro"))
                {
                    auto* M = New<UK2Node_MacroInstance>(G);
                    UBlueprint* Macros = LoadObject<UBlueprint>(nullptr,
                        TEXT("/Engine/EditorBlueprintResources/StandardMacros.StandardMacros"));
                    TArray<UEdGraph*> All;
                    Macros->GetAllGraphs(All);
                    for (auto* MG : All)
                    {
                        if (MG->GetName() == Arg)
                        {
                            M->SetMacroGraph(MG);
                        }
                    }
                    N = M;
                }
                else if (Kind == TEXT("fields"))
                {
                    FString TypeName, Field;
                    Arg.Split(TEXT(":"), &TypeName, &Field);
                    auto* M = New<UK2Node_SetFieldsInStruct>(G);
                    M->StructType = Struct(TypeName);
                    M->bMadeAfterOverridePinRemoval = true;
                    N = M;
                }
                else if (Kind == TEXT("make"))
                {
                    auto* M = New<UK2Node_MakeStruct>(G);
                    M->StructType = Struct(Arg);
                    M->bMadeAfterOverridePinRemoval = true;
                    N = M;
                }
                else if (Kind == TEXT("break"))
                {
                    auto* M = New<UK2Node_BreakStruct>(G);
                    M->StructType = Struct(Arg);
                    N = M;
                }
                else if (Kind == TEXT("item"))
                {
                    N = New<UK2Node_GetArrayItem>(G);
                }
                else if (Kind == TEXT("arraymake"))
                {
                    N = New<UK2Node_MakeArray>(G);
                }
                if (!N)
                {
                    Fail(TEXT("Unknown node ") + Line);
                    continue;
                }
                N->AllocateDefaultPins();
                if (auto* B = Cast<UK2Node_BreakStruct>(N))
                {
                    for (auto& Optional : B->ShowPinForProperties)
                    {
                        Optional.bShowPin = true;
                    }
                    B->ReconstructNode();
                }
                if (auto* F = Cast<UK2Node_SetFieldsInStruct>(N))
                {
                    FString TypeName, Field;
                    Arg.Split(TEXT(":"), &TypeName, &Field);
                    for (auto& Optional : F->ShowPinForProperties)
                    {
                        Optional.bShowPin = Optional.PropertyName == FName(*Field);
                    }
                    F->ReconstructNode();
                }
                else if (auto* M = Cast<UK2Node_MakeStruct>(N))
                {
                    for (auto& Optional : M->ShowPinForProperties)
                    {
                        Optional.bShowPin = true;
                    }
                    M->ReconstructNode();
                }
                N->NodePosX = (Index % 8) * 320;
                N->NodePosY = (Index / 8) * 300;
                ++Index;
                Nodes.Add(A[1], N);
                continue;
            }
            if (Op == TEXT("redirect"))
            {
                UFunction* Function = BP->SkeletonGeneratedClass->FindFunctionByName(FName(*A[2]));
                check(Function);
                TArray<UEdGraph*> Graphs;
                BP->GetAllGraphs(Graphs);
                for (UEdGraph* Target : Graphs)
                {
                    auto Existing = Target->Nodes;
                    for (auto Node : Existing)
                    {
                        if (auto* Old = Cast<UK2Node_CallFunction>(Node);
                            Old && Old->FunctionReference.GetMemberName() == FName(*A[1]))
                        {
                            auto* Replacement = New<UK2Node_CallFunction>(Target);
                            Replacement->SetFromFunction(Function);
                            Replacement->FunctionReference.SetSelfMember(FName(*A[2]));
                            Replacement->AllocateDefaultPins();
                            Replacement->NodePosX = Old->NodePosX;
                            Replacement->NodePosY = Old->NodePosY;
                            for (auto* OldPin : Old->Pins)
                            {
                                FName Name = OldPin->PinName == TEXT("ReturnValue")
                                                 ? FName(TEXT("Value"))
                                                 : OldPin->PinName;
                                if (auto* NewPin = Replacement->FindPin(Name))
                                {
                                    const auto Links = OldPin->LinkedTo;
                                    OldPin->BreakAllPinLinks();
                                    for (auto* Other : Links)
                                    {
                                        if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(
                                                NewPin, Other))
                                        {
                                            Fail(TEXT("redirect pin ") + Name.ToString());
                                        }
                                    }
                                }
                            }
                            Old->BreakAllNodeLinks();
                            Target->RemoveNode(Old);
                        }
                    }
                }
                continue;
            }
            if (Op == TEXT("unlink"))
            {
                if (auto* P = Resolve(A[1]))
                {
                    P->BreakAllPinLinks();
                }
                else
                {
                    Fail(TEXT("Missing unlink ") + Line);
                }
                continue;
            }
            if (Op == TEXT("link"))
            {
                UEdGraphPin* From = Resolve(A[1]);
                UEdGraphPin* To = Resolve(A[2]);
                if (!From || !To)
                {
                    Fail(TEXT("Missing pin ") + Line);
                    continue;
                }
                if (!GetDefault<UEdGraphSchema_K2>()->TryCreateConnection(From, To))
                {
                    Fail(TEXT("Cannot connect ") + Line);
                }
                continue;
            }
            if (Op == TEXT("default"))
            {
                if (auto* P = Resolve(A[1]))
                {
                    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultValue(*P, A[2]);
                }
                else
                {
                    Fail(TEXT("Missing default ") + Line);
                }
                continue;
            }
            if (Op == TEXT("object"))
            {
                if (auto* P = Resolve(A[1]))
                {
                    GetDefault<UEdGraphSchema_K2>()->TrySetDefaultObject(
                        *P, LoadObject<UObject>(nullptr, *A[2]));
                }
                else
                {
                    Fail(TEXT("Missing object pin ") + Line);
                }
                continue;
            }
            if (Op == TEXT("type"))
            {
                if (auto* P = Resolve(A[1]))
                {
                    P->PinType = Type(A[2]);
                    P->GetOwningNode()->PinTypeChanged(P);
                }
                continue;
            }
            if (Op == TEXT("editable"))
            {
                FBlueprintEditorUtils::SetBlueprintOnlyEditableFlag(BP, FName(*A[1]), false);
                continue;
            }
            if (Op == TEXT("category"))
            {
                FBlueprintEditorUtils::SetBlueprintVariableCategory(
                    BP, FName(*A[1]), nullptr, FText::FromString(A[2]));
                continue;
            }
            if (Op == TEXT("position"))
            {
                if (auto* N = Nodes.FindRef(A[1]))
                {
                    N->NodePosX = FCString::Atoi(*A[2]);
                    N->NodePosY = FCString::Atoi(*A[3]);
                }
                continue;
            }
            if (Op == TEXT("comment"))
            {
                if (auto* N = Nodes.FindRef(A[1]))
                {
                    N->NodeComment = A[2];
                    N->bCommentBubbleVisible = true;
                }
                continue;
            }
        }
        // Give the editable functions a reading order and remove unused signature-only return nodes.
        for (UEdGraph* FunctionGraph : BP->FunctionGraphs)
        {
            const FString Name = FunctionGraph->GetName();
            const bool bEditedGraph = Lines.ContainsByPredicate(
                [&](const FString& Line)
                {
                    return Line == TEXT("graph|") + Name;
                });
            if (!bEditedGraph)
            {
                continue;
            }

            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            if (!Name.StartsWith(TEXT("OB")) && Name != TEXT("GenerateOrganicCourtyardBP") &&
                Name != TEXT("GenerateCourtyardBP"))
            {
                continue;
            }
            FString Category = TEXT("Organic BP|08 Utility");
            if (Name == TEXT("GenerateOrganicCourtyardBP") || Name == TEXT("GenerateCourtyardBP") ||
                Name == TEXT("OBStartLayout") || Name == TEXT("OBRunTimer") ||
                Name == TEXT("OBAdvance") || Name == TEXT("OBRetry") ||
                Name == TEXT("OBResetAttempt"))
            {
                Category = TEXT("Organic BP|01 Flow");
            }
            else if (Name == TEXT("OBTryPlaceRoom") || Name == TEXT("OBRoomFits"))
            {
                Category = TEXT("Organic BP|02 Room shape");
            }
            else if (Name.Contains(TEXT("Packing")) || Name == TEXT("OBPackOneColumn"))
            {
                Category = TEXT("Organic BP|03 Compact rooms");
            }
            else if (Name.Contains(TEXT("Route")) || Name == TEXT("OBMakePairs") ||
                     Name == TEXT("OBAdvancePair") || Name == TEXT("OBAvailablePorts") ||
                     Name == TEXT("OBUsedCount") || Name == TEXT("OBCountLoops"))
            {
                Category = TEXT("Organic BP|04 Connections and loops");
            }
            else if (Name == TEXT("OBChooseIndoorRooms") || Name == TEXT("OBMakeCells") ||
                     Name == TEXT("OBClassifyTunnel"))
            {
                Category = TEXT("Organic BP|05 Cells and tunnels");
            }
            else if (Name.Contains(TEXT("Entrance")))
            {
                Category = TEXT("Organic BP|07 Entrance");
            }
            else if (Name.Contains(TEXT("Goal")) || Name == TEXT("OBValidatePlan"))
            {
                Category = TEXT("Organic BP|06 Goal and validation");
            }
            FBlueprintEditorUtils::SetBlueprintFunctionOrMacroCategory(
                FunctionGraph, FText::FromString(Category), true);
            auto AllNodes = FunctionGraph->Nodes;
            for (UEdGraphNode* Node : AllNodes)
            {
                if (auto* R = Cast<UK2Node_FunctionResult>(Node))
                {
                    auto* Exec = R->FindPin(UEdGraphSchema_K2::PN_Execute);
                    if (Exec && Exec->LinkedTo.IsEmpty())
                    {
                        FunctionGraph->RemoveNode(R);
                    }
                }
            }
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log);
        FString Report = FString::Printf(TEXT("editErrors=%d compileErrors=%d warnings=%d\n"),
            Errors,
            Log.NumErrors,
            Log.NumWarnings);
        for (const auto& Message : Log.Messages)
        {
            Report += Message->ToText().ToString() + TEXT("\n");
        }
        FFileHelper::SaveStringToFile(Report, *(Dir / TEXT("apply-result.txt")));
        if (!Errors && !Log.NumErrors)
        {
            for (const FString& Line : Lines)
            {
                TArray<FString> A;
                Line.ParseIntoArray(A, TEXT("|"), false);
                if (A[0] == TEXT("value"))
                {
                    if (FProperty* P = BP->GeneratedClass->FindPropertyByName(FName(*A[1])))
                    {
                        P->ImportText_Direct(*A[2],
                            P->ContainerPtrToValuePtr<void>(BP->GeneratedClass->GetDefaultObject()),
                            BP->GeneratedClass->GetDefaultObject(),
                            PPF_None);
                    }
                }
            }
            FSavePackageArgs Args;
            Args.TopLevelFlags = RF_Public | RF_Standalone;
            UPackage::SavePackage(BP->GetOutermost(),
                BP,
                *FPackageName::LongPackageNameToFilename(
                    BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()),
                Args);
        }
        UE_LOG(LogTemp, Display, TEXT("COURTYARD_BP %s"), *Report);
    }
} // namespace CourtyardOrganicBPEditor

static FAutoConsoleCommand ApplyCourtyardBP(TEXT("CourtyardOrganicBP.Apply"),
    TEXT("Create the Courtyard Blueprint algorithm graphs."),
    FConsoleCommandDelegate::CreateStatic(&CourtyardOrganicBPEditor::Apply));
#endif
