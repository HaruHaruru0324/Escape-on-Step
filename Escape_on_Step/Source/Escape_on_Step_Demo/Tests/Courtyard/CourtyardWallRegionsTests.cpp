#if WITH_DEV_AUTOMATION_TESTS
#include "Generation/Courtyard/Layout/CourtyardWallRegions.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourtyardWallRegionsTest,
    "EscapeOnStep.Dungeon.CourtyardWallRegions",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourtyardWallRegionsTest::RunTest(const FString&)
{
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    FCourtyardPlan Simple;
    for (int32 X = -2; X <= 3; ++X)
    {
        FCourtyardCell Cell;
        Cell.Position = FIntPoint(X, 0);
        Cell.NodeId = X < 0 ? 0 : X > 1 ? 1 : INDEX_NONE;
        Simple.Cells.Add(Cell);
    }
    FCourtyardLink Route;
    Route.From = 0;
    Route.To = 1;
    Route.bAlley = true;
    Route.Cells = {FIntPoint(0, 0), FIntPoint(1, 0)};
    Simple.Links.Add(Route);
    const auto Split = CourtyardWallRegions::Build(Simple);
    TestEqual(TEXT("Straight alley gets one short separator"), Split.AddedTunnels.Num(), 1);
    TestEqual(TEXT("The connected rooms become separate spaces"), Split.RegionCount, 2);
    TestNotEqual(TEXT("Each side keeps its own wall choice"),
        Split.StyleOwnerByCell.FindChecked(FIntPoint(-1, 0)),
        Split.StyleOwnerByCell.FindChecked(FIntPoint(2, 0)));

    const auto EntranceProtected = CourtyardWallRegions::Build(Simple, TSet<FIntPoint>{FIntPoint(0, 0)});
    TestEqual(TEXT("Introduction opening never gets a perpendicular tunnel side wall"), EntranceProtected.AddedTunnels.Num(), 0);
    TestEqual(TEXT("Without a safe separator both sides share one wall style"),
        EntranceProtected.StyleOwnerByCell.FindChecked(FIntPoint(-1, 0)),
        EntranceProtected.StyleOwnerByCell.FindChecked(FIntPoint(2, 0)));

    FCourtyardPlan Touching = Simple;
    Touching.Links.Reset();
    const auto Merged = CourtyardWallRegions::Build(Touching);
    TestEqual(TEXT("Without a tunnel direct contact shares one owner"),
        Merged.StyleOwnerByCell.FindChecked(FIntPoint(-1, 0)),
        Merged.StyleOwnerByCell.FindChecked(FIntPoint(2, 0)));

    // 実際の迷路でも、元の床・経路・周回条件を変えずに境界を作れることを確認する。
    int32 PlansWithMultipleRooms = 0;
    int32 TotalAddedTunnels = 0;
    for (int32 Seed = 0; Seed < 24; ++Seed)
    {
        FCourtyardSettings Settings;
        Settings.RoomChance = 0;
        Settings.MaxRooms = 0;
        FCourtyardPlan Plan;
        FString Error;
        if (!TestTrue(TEXT("Organic layout builds"),
            UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(Settings, Seed, Plan, Error)))
        {
            AddError(Error);
            continue;
        }
        const auto Regions = CourtyardWallRegions::Build(Plan);
        const auto Again = CourtyardWallRegions::Build(Plan);
        PlansWithMultipleRooms += Regions.RegionCount > 1;
        TotalAddedTunnels += Regions.AddedTunnels.Num();
        TMap<FIntPoint, const FCourtyardCell*> Floors;
        for (const auto& Cell : Plan.Cells)
        {
            Floors.Add(Cell.Position, &Cell);
        }
        for (const auto& Cell : Plan.Cells)
        {
            if (Cell.Kind == ECourtyardCellKind::Tunnel || Regions.AddedTunnels.Contains(Cell.Position))
            {
                continue;
            }
            const int32* Owner = Regions.StyleOwnerByCell.Find(Cell.Position);
            TestTrue(TEXT("Every room and outdoor alley has a valid BP room ID"), Owner && Plan.Nodes.IsValidIndex(*Owner));
            if (!Owner)
            {
                continue;
            }
            TestEqual(TEXT("Region ownership is deterministic"), *Owner, Again.StyleOwnerByCell.FindChecked(Cell.Position));
            for (const auto Step : Steps)
            {
                if (const auto NeighborOwner = Regions.StyleOwnerByCell.Find(Cell.Position + Step))
                {
                    TestEqual(TEXT("No wall type boundary without a tunnel"), *Owner, *NeighborOwner);
                }
            }
        }
        for (const auto& Pair : Regions.AddedTunnels)
        {
            const auto P = Pair.Key;
            const auto Step = Steps[Pair.Value.Direction];
            const FIntPoint Side(-Step.Y, Step.X);
            TestEqual(TEXT("Separator length is one cell"), Pair.Value.Cells.Num(), 1);
            TestTrue(TEXT("Never converts room floor to a tunnel"), Floors.FindChecked(P)->NodeId == INDEX_NONE);
            TestTrue(TEXT("Both tunnel exits have floor"), Floors.Contains(P - Step) && Floors.Contains(P + Step));
            TestFalse(TEXT("No branch on tunnel left"), Floors.Contains(P + Side));
            TestFalse(TEXT("No branch on tunnel right"), Floors.Contains(P - Side));
            TestTrue(TEXT("Same seed chooses the same separators"), Again.AddedTunnels.Contains(P));
        }
        TestTrue(TEXT("Original floor connectivity and return loops remain valid"),
            UCourtyardLayoutLibrary::ValidateOrganicCourtyardPlan(Plan, Error));
    }
    TestTrue(TEXT("Real layouts keep distinct spaces instead of merging everything"), PlansWithMultipleRooms >= 20);
    TestTrue(TEXT("Real layouts receive short tunnel boundaries"), TotalAddedTunnels > 0);
    AddInfo(FString::Printf(TEXT("24 layouts: %d separated layouts; %d short tunnels"), PlansWithMultipleRooms, TotalAddedTunnels));
    return true;
}
#endif
