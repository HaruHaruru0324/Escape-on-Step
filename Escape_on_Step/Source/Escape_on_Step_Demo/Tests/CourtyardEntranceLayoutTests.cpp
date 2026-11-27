#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Generation/Courtyard/Layout/CourtyardEntranceLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourtyardEntranceLayoutTest,
    "EscapeOnStep.Dungeon.CourtyardEntranceLayout",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCourtyardEntranceLayoutTest::RunTest(const FString& Parameters)
{
    TSet<int32> Directions;
    int32 Cases = 0;
    int32 InteriorCases = 0;
    for (int32 Seed = 0; Seed < 120; ++Seed)
    {
        FCourtyardPlan Original;
        FString Error;
        if (!UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(
                FCourtyardSettings(), Seed, Original, Error))
        {
            AddError(Error);
            return false;
        }
        for (float Width : {320.f, 480.f, 640.f})
        {
            auto Plan = Original;
            FCourtyardEntranceSelection Entrance, Repeat;
            if (!CourtyardEntranceLayout::SelectEntrance(
                    Plan, 320, Width, 1280, Seed, Entrance, Error))
            {
                AddError(FString::Printf(TEXT("seed=%d width=%.0f: %s"), Seed, Width, *Error));
                return false;
            }
            TestEqual(TEXT("Correct number of opened cells"),
                Entrance.Cells.Num(),
                FMath::CeilToInt((Width + 160) / 320));
            TestEqual(TEXT("Floor count is preserved"), Plan.Cells.Num(), Original.Cells.Num());
            TestTrue(TEXT("Goal is reachable and plan remains valid"),
                UCourtyardLayoutLibrary::ValidateOrganicCourtyardPlan(Plan, Error));
            TestTrue(TEXT("Same seed selects same entrance"),
                CourtyardEntranceLayout::SelectEntrance(
                    Plan, 320, Width, 1280, Seed, Repeat, Error) &&
                    Entrance.Cells == Repeat.Cells &&
                    Entrance.OutwardDirection == Repeat.OutwardDirection);
            Directions.Add(Entrance.OutwardDirection);
            InteriorCases += Entrance.bInterior;
            ++Cases;
        }
    }
    TestEqual(TEXT("Entrances use all four sides, not only west"), Directions.Num(), 4);

    TestTrue(TEXT("Interior rooms are included"), InteriorCases > 0);
    FCourtyardPlan Plan;
    FString Error;
    TestTrue(TEXT("Create fixed geometry"),
        UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(FCourtyardSettings(), 0, Plan, Error));
    TSet<FIntPoint> Starts;
    for (int32 Seed = 0; Seed < 32; ++Seed)
    {
        FCourtyardEntranceSelection Selection;
        TestTrue(TEXT("Can vary start without rebuilding rooms"),
            CourtyardEntranceLayout::SelectEntrance(Plan, 320, 480, 1280, Seed, Selection, Error));
        Starts.Add(Plan.StartCell);
    }
    TestTrue(TEXT("Fixed geometry has several possible starts"), Starts.Num() > 1);
    FCourtyardEntranceSelection Invalid;
    TestFalse(TEXT("Reject invalid width"),
        CourtyardEntranceLayout::SelectEntrance(Plan, 320, 0, 1280, 0, Invalid, Error));
    AddInfo(FString::Printf(
        TEXT("Validated %d width/seed cases and %d fixed-layout starts"), Cases, Starts.Num()));
    return true;
}
#endif
