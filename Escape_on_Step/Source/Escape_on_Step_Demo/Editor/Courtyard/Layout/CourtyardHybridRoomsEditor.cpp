#if WITH_EDITOR
#include "Generation/Courtyard/Layout/CourtyardRoomShapes.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_MakeStruct.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "HAL/IConsoleManager.h"
#include "UObject/SavePackage.h"

namespace
{
    void InstallHybridRooms()
    {
        FString Report;
        const auto Fail = [&](const FString& Message)
        {
            FFileHelper::SaveStringToFile(Message,
                TEXT("C:/_Team/Escape-on-Step-Demo/Escape_on_Step_Demo/Saved/HybridRooms/install_result.txt"));
            UE_LOG(LogTemp, Error, TEXT("HYBRID_ROOM_INSTALL %s"), *Message);
        };
        UBlueprint* BP = LoadObject<UBlueprint>(nullptr,
            TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_OrganicBP.BP_Courtyard_Generator_OrganicBP"));
        if (!BP)
        {
            Fail(TEXT("Generator Blueprint not found"));
            return;
        }
        UEdGraph* Graph = nullptr;
        for (UEdGraph* Function : BP->FunctionGraphs)
        {
            if (Function->GetFName() == TEXT("OBTryPlaceRoom"))
            {
                Graph = Function;
            }
        }
        if (!Graph)
        {
            Fail(TEXT("OBTryPlaceRoom not found"));
            return;
        }
        UK2Node_MakeStruct* MakeNode = nullptr;
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (auto* Call = Cast<UK2Node_CallFunction>(Node);
                Call && Call->FunctionReference.GetMemberName() == TEXT("MakeHybridRoomFloor"))
            {
                Fail(TEXT("Hybrid shape call already installed; refusing to duplicate it"));
                return;
            }
            if (auto* Make = Cast<UK2Node_MakeStruct>(Node);
                Make && Make->StructType == FCourtyardNode::StaticStruct())
            {
                if (MakeNode)
                {
                    Fail(TEXT("Multiple room constructors; manual review required"));
                    return;
                }
                MakeNode = Make;
            }
        }
        UEdGraphPin* FloorInput = MakeNode ? MakeNode->FindPin(TEXT("FloorCells")) : nullptr;
        UEdGraphPin* IdInput = MakeNode ? MakeNode->FindPin(TEXT("Id")) : nullptr;
        if (!FloorInput || FloorInput->LinkedTo.Num() != 1 || !IdInput)
        {
            Fail(TEXT("Room floor input differs from expected graph; nothing saved"));
            return;
        }
        auto* Source = FloorInput->LinkedTo[0];
        auto* Call = NewObject<UK2Node_CallFunction>(Graph);
        Graph->AddNode(Call, false, false);
        Call->CreateNewGuid();
        Call->SetFromFunction(UCourtyardRoomShapes::StaticClass()->FindFunctionByName(TEXT("MakeHybridRoomFloor")));
        Call->AllocateDefaultPins();
        Call->NodePosX = MakeNode->NodePosX - 320;
        Call->NodePosY = MakeNode->NodePosY + 320;
        Call->NodeComment = TEXT("太い帯を残し、一部だけ細くする。部屋の配置・経路探索より前に確定。");
        Call->bCommentBubbleVisible = true;
        auto* Seed = NewObject<UK2Node_VariableGet>(Graph);
        Graph->AddNode(Seed, false, false);
        Seed->CreateNewGuid();
        Seed->VariableReference.SetSelfMember(TEXT("OBSeed"));
        Seed->AllocateDefaultPins();
        Seed->NodePosX = Call->NodePosX - 192;
        Seed->NodePosY = Call->NodePosY + 120;
        const auto* Schema = GetDefault<UEdGraphSchema_K2>();
        bool Linked = Schema->TryCreateConnection(Source, Call->FindPin(TEXT("OriginalFloor")));
        Linked &= Schema->TryCreateConnection(Seed->FindPin(TEXT("OBSeed")), Call->FindPin(TEXT("LayoutSeed")));
        if (IdInput->LinkedTo.Num() == 1)
        {
            Linked &= Schema->TryCreateConnection(IdInput->LinkedTo[0], Call->FindPin(TEXT("RoomId")));
        }
        else
        {
            Call->FindPin(TEXT("RoomId"))->DefaultValue = IdInput->DefaultValue;
        }
        Call->FindPin(TEXT("NarrowingLength"))->DefaultValue = TEXT("2");
        FloorInput->BreakLinkTo(Source);
        Linked &= Schema->TryCreateConnection(Call->FindPin(TEXT("ReturnValue")), FloorInput);
        if (!Linked)
        {
            Fail(TEXT("Pin connection failed; nothing saved"));
            return;
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP);
        FCompilerResultsLog Log;
        FKismetEditorUtilities::CompileBlueprint(BP, EBlueprintCompileOptions::SkipSave, &Log);
        Report = FString::Printf(TEXT("compileErrors=%d warnings=%d\n"), Log.NumErrors, Log.NumWarnings);
        for (const auto& Message : Log.Messages)
        {
            Report += Message->ToText().ToString() + TEXT("\n");
        }
        if (Log.NumErrors)
        {
            Fail(Report);
            return;
        }
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        const bool Saved = UPackage::SavePackage(BP->GetOutermost(), BP,
            *FPackageName::LongPackageNameToFilename(BP->GetOutermost()->GetName(), FPackageName::GetAssetPackageExtension()), Args);
        Report += FString::Printf(TEXT("saved=%d\n"), Saved);
        FFileHelper::SaveStringToFile(Report,
            TEXT("C:/_Team/Escape-on-Step-Demo/Escape_on_Step_Demo/Saved/HybridRooms/install_result.txt"));
    }
}

static FAutoConsoleCommand HybridRoomsInstall(TEXT("CourtyardHybridRooms.Install"),
    TEXT("Add the hybrid floor call only to the prototype room creation function."),
    FConsoleCommandDelegate::CreateStatic(&InstallHybridRooms));
#endif
