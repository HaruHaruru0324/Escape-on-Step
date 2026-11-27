#if WITH_DEV_AUTOMATION_TESTS
#include "Generation/Courtyard/Layout/CourtyardRoomShapes.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourtyardRoomShapesTest,
    "EscapeOnStep.Dungeon.CourtyardRoomShapes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourtyardRoomShapesTest::RunTest(const FString&)
{
    int32 HybridCount = 0;
    for (int32 Width = 5; Width <= 6; ++Width)
    {
        for (int32 UShape = 0; UShape <= 1; ++UShape)
        {
            for (int32 Rotation = 0; Rotation < 4; ++Rotation)
            {
                TArray<FIntPoint> Original;
                for (int32 X = 0; X < Width; ++X)
                {
                    for (int32 Y = 0; Y < Width; ++Y)
                    {
                        if (Y < 2 || X >= Width - 2 || (UShape && Y >= Width - 2))
                        {
                            FIntPoint P(X, Y);
                            for (int32 R = 0; R < Rotation; ++R)
                            {
                                P = FIntPoint(Width - 1 - P.Y, P.X);
                            }
                            Original.Add(P);
                        }
                    }
                }
                for (int32 Seed = 0; Seed < 8; ++Seed)
                {
                    const auto Hybrid = UCourtyardRoomShapes::MakeHybridRoomFloor(Original, Seed, 3);
                    TestTrue(TEXT("Hybrid narrows a short section"), Hybrid.Num() < Original.Num());
                    TestTrue(TEXT("Only a short section is narrowed"), Original.Num() - Hybrid.Num() <= 2);
                    TestTrue(TEXT("Hybrid is deterministic"), Hybrid == UCourtyardRoomShapes::MakeHybridRoomFloor(Original, Seed, 3));
                    TSet<FIntPoint> Floor(Hybrid);
                    bool Thin = false;
                    bool Thick = false;
                    for (const auto P : Hybrid)
                    {
                        TestTrue(TEXT("Never expands outside the reserved room"), Original.Contains(P));
                        Thin |= Floor.Contains(P + FIntPoint(1, 0)) && Floor.Contains(P - FIntPoint(1, 0)) &&
                                !Floor.Contains(P + FIntPoint(0, 1)) && !Floor.Contains(P - FIntPoint(0, 1));
                        Thin |= Floor.Contains(P + FIntPoint(0, 1)) && Floor.Contains(P - FIntPoint(0, 1)) &&
                                !Floor.Contains(P + FIntPoint(1, 0)) && !Floor.Contains(P - FIntPoint(1, 0));
                        Thick |= Floor.Contains(P + FIntPoint(1, 0)) && Floor.Contains(P + FIntPoint(0, 1)) &&
                                 Floor.Contains(P + FIntPoint(1, 1));
                    }
                    TestTrue(TEXT("Contains both narrow and wide parts"), Thin && Thick);
                    HybridCount += Thin && Thick;
                }
            }
        }
    }

    FCourtyardPlan Ring;
    FCourtyardNode Node;
    Node.Id = 0;
    Node.Min = FIntPoint(0, 0);
    Node.Size = FIntPoint(5, 5);
    for (int32 X = 0; X < 5; ++X)
    {
        for (int32 Y = 0; Y < 5; ++Y)
        {
            if (X == 0 || X == 4 || Y == 0 || Y == 4)
            {
                FCourtyardCell Cell;
                Cell.Position = FIntPoint(X, Y);
                Cell.NodeId = 0;
                Node.FloorCells.Add(Cell.Position);
                Ring.Cells.Add(Cell);
            }
        }
    }
    Ring.Nodes.Add(Node);
    TestEqual(TEXT("Closed 3x3 room hole is filled"), UCourtyardRoomShapes::FillEnclosedRoomHoles(Ring), 9);
    TestEqual(TEXT("Closed room now has continuous floor"), Ring.Nodes[0].FloorCells.Num(), 25);
    TestEqual(TEXT("Filling is idempotent"), UCourtyardRoomShapes::FillEnclosedRoomHoles(Ring), 0);
    FCourtyardPlan Concave = Ring;
    TestEqual(TEXT("Filled square becomes a bent room"), UCourtyardRoomShapes::RemovePlainRectangularRooms(Concave), 1);
    TestTrue(TEXT("Concave room is not a plain square"), Concave.Nodes[0].FloorCells.Num() < 25);
    TestEqual(TEXT("The notch is open, not another enclosed hole"), UCourtyardRoomShapes::FillEnclosedRoomHoles(Concave), 0);
    FCourtyardPlan Open = Ring;
    Open.Cells.RemoveAll([](const FCourtyardCell& Cell) { return Cell.Position.X < 4 && Cell.Position.Y > 1 && Cell.Position.Y < 4; });
    Open.Nodes[0].FloorCells.RemoveAll([](FIntPoint P) { return P.X < 4 && P.Y > 1 && P.Y < 4; });
    TestEqual(TEXT("Open U notch remains open"), UCourtyardRoomShapes::FillEnclosedRoomHoles(Open), 0);

    int32 FilledInLayouts = 0;
    for (int32 Seed = 0; Seed < 16; ++Seed)
    {
        FCourtyardSettings Settings;
        Settings.RoomChance = 0;
        Settings.MaxRooms = 0;
        FCourtyardPlan Plan;
        FString Error;
        if (TestTrue(TEXT("Hybrid organic layout builds"), UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(Settings, Seed, Plan, Error)))
        {
            FilledInLayouts += UCourtyardRoomShapes::FillEnclosedRoomHoles(Plan);
            UCourtyardRoomShapes::RemovePlainRectangularRooms(Plan);
            for (const auto& Room : Plan.Nodes)
            {
                TSet<FIntPoint> RoomFloor(Room.FloorCells);
                for (const auto P : Room.FloorCells)
                {
                    bool Plain = true;
                    for (int32 X = 0; X < 4; ++X)
                    {
                        for (int32 Y = 0; Y < 4; ++Y)
                        {
                            Plain &= RoomFloor.Contains(P + FIntPoint(X, Y));
                        }
                    }
                    TestFalse(TEXT("No broad empty 4x4 square in a room"), Plain);
                }
            }
            TestTrue(TEXT("Filled layout retains valid connections and loops"), UCourtyardLayoutLibrary::ValidateOrganicCourtyardPlan(Plan, Error));
            TestEqual(TEXT("No room holes remain"), UCourtyardRoomShapes::FillEnclosedRoomHoles(Plan), 0);
        }
    }
    AddInfo(FString::Printf(TEXT("%d hybrid footprints; 16 layouts; %d enclosed cells filled"), HybridCount, FilledInLayouts));
    return true;
}
#endif
