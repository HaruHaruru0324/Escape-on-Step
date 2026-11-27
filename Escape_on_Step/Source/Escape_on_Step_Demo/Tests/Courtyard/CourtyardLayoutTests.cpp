#if WITH_DEV_AUTOMATION_TESTS
#include "Generation/Courtyard/Layout/CourtyardLayout.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCourtyardLayoutTest, "EscapeOnStep.Dungeon.CourtyardLayout",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCourtyardLayoutTest::RunTest(const FString&)
{
	const FIntPoint CourtyardTestSteps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	TSet<int32> RoomSizes;
	int32 RoomPlans = 0;
	double DenseFloors = 0, DenseBounds = 0, LegacyFloors = 0, LegacyBounds = 0;
	for (int32 Seed = 0; Seed < 240; ++Seed)
	{
		FCourtyardSettings S;
		S.ExtraConnectionChance = Seed % 3 == 0 ? 0.f : Seed % 3 == 1 ? 1.f : .5f;
		S.StreetCount = 9 + Seed % 16;
		S.StreetWidth = Seed % 5 == 0 ? 3 : 2;
		if (Seed % 4 == 0)
		{
			S.RoomChance = 0;
		}
		if (Seed % 4 == 1)
		{
			S.RoomChance = 1;
		}
		if (Seed % 7 == 0)
		{
			S.MinRoomSections = S.MaxRoomSections = 2;
		}
		if (Seed % 7 == 1)
		{
			S.MinRoomSections = S.MaxRoomSections = 3;
		}
		if (Seed % 7 == 2)
		{
			S.MinRoomSections = S.MaxRoomSections = 4;
		}
		FCourtyardPlan P, Again;
		FString Error;
		if (!TestTrue(TEXT("All spaces are bent end-to-end chains"),
					  UCourtyardLayoutLibrary::BuildCourtyardPlan(S, Seed, P, Error)))
		{
			AddError(FString::Printf(TEXT("seed=%d: %s"), Seed, *Error));
			continue;
		}

        const int32 Columns = FMath::CeilToInt(FMath::Sqrt(float(S.StreetCount)));
        TestTrue(TEXT("Adjacent columns no longer share the same Y origin"), P.Nodes[0].Min.Y != P.Nodes[1].Min.Y);
        TestTrue(TEXT("Adjacent rows no longer share the same X origin"), P.Nodes[0].Min.X != P.Nodes[Columns].Min.X);
        auto AlignedSettings = S; AlignedSettings.bStaggeredLayout = false;
        TestEqual(TEXT("Disabling stagger preserves the supplied origin"),
            UCourtyardLayoutLibrary::GetStaggeredOrigin(FIntPoint(10,20), Columns+1, Columns, Seed, AlignedSettings), FIntPoint(10,20));
		const auto BoundsArea = [](const FCourtyardPlan& Plan)
		{
			FIntPoint Min(MAX_int32, MAX_int32), Max(MIN_int32, MIN_int32);
			for (const auto& C : Plan.Cells)
			{
				Min.X = FMath::Min(Min.X, C.Position.X);
				Min.Y = FMath::Min(Min.Y, C.Position.Y);
				Max.X = FMath::Max(Max.X, C.Position.X);
				Max.Y = FMath::Max(Max.Y, C.Position.Y);
			}
			return (Max.X - Min.X + 1) * (Max.Y - Min.Y + 1);
		};
		auto LegacySettings = S;
		LegacySettings.bDensePacking = false;
		FCourtyardPlan Legacy;
		if (TestTrue(TEXT("Previous spacing remains supported"),
					 UCourtyardLayoutLibrary::BuildCourtyardPlan(LegacySettings, Seed, Legacy, Error)))
		{
			const int32 DenseArea = BoundsArea(P), LegacyArea = BoundsArea(Legacy);
			TestTrue(TEXT("Packing reduces actual footprint by at least 15 percent"), DenseArea < LegacyArea * .85);
			DenseFloors += P.Cells.Num();
			DenseBounds += DenseArea;
			LegacyFloors += Legacy.Cells.Num();
			LegacyBounds += LegacyArea;
		}
		int32 Streets = 0, Rooms = 0;
		TArray<int32> Degrees;
		Degrees.Init(0, P.Nodes.Num());
		for (const auto& N : P.Nodes)
		{
			Streets += N.Kind == ECourtyardCellKind::Street;
			Rooms = P.RoomAreas.Num();
			TestTrue(TEXT("No standalone straight street or room"), N.Sections.Num() >= 2 && N.Sections.Num() <= 4);
			for (int32 I = 1; I < N.Sections.Num(); ++I)
			{
				TestTrue(TEXT("Every join changes direction"),
						 (N.Sections[I].Size.X > N.Sections[I].Size.Y) !=
							 (N.Sections[I - 1].Size.X > N.Sections[I - 1].Size.Y));
			}
			if (N.Kind == ECourtyardCellKind::Room)
			{
				RoomSizes.Add(N.Sections.Num());
				TestTrue(TEXT("Room section setting is honored"),
						 N.Sections.Num() >= S.MinRoomSections && N.Sections.Num() <= S.MaxRoomSections);
			}
		}
		TestTrue(TEXT("Requested street minimum plus grid completion"),
				 Streets >= S.StreetCount && Streets <= S.StreetCount + 5);
		TestTrue(TEXT("Room count limit"), Rooms <= S.MaxRooms);
		if (S.RoomChance > 0)
		{
			TestTrue(TEXT("At least two compact rooms"), Rooms >= 2);
		}
		for (const auto& R : P.RoomAreas)
		{
			TestTrue(TEXT("Rooms have through doors"), R.DoorCells.Num() >= 2);
			TestTrue(TEXT("Wide irregular room, no large empty rectangle"),
					 R.Size == FIntPoint(4, 4) && R.FloorCells.Num() >= 12 && R.FloorCells.Num() <= 15);
		}
		RoomPlans += Rooms > 0;
		if (S.RoomChance == 0)
		{
			TestEqual(TEXT("Zero room chance"), Rooms, 0);
		}
		for (const auto& L : P.Links)
		{
			++Degrees[L.From];
			++Degrees[L.To];
			TestTrue(TEXT("Short tunnel"), L.Cells.Num() >= 1 && L.Cells.Num() <= 2);
			for (int32 I = 1; I < L.Cells.Num(); ++I)
			{
				TestEqual(TEXT("No turns inside a tunnel"), L.Cells[I] - L.Cells[I - 1],
						  CourtyardTestSteps[L.Direction]);
			}
		}
		for (int32 Degree : Degrees)
		{
			TestTrue(TEXT("Return connection for every space"), Degree >= 2);
		}
		TestTrue(TEXT("Multiple independent loops"), P.LoopCount >= 3);
		int32 Branches = 0;
		for (int32 D : Degrees)
		{
			Branches += D >= 3;
		}
		TestTrue(TEXT("At least four real choices"), Branches >= 4);
		TestTrue(TEXT("Repeatable seed"), UCourtyardLayoutLibrary::BuildCourtyardPlan(S, Seed, Again, Error));
		TestEqual(TEXT("Repeat floor count"), P.Cells.Num(), Again.Cells.Num());
		for (int32 I = 0; I < P.Cells.Num(); ++I)
		{
			const auto& A = P.Cells[I];
			const auto& B = Again.Cells[I];
			TestTrue(TEXT("Stable floor and corner ownership"),
					 A.Position == B.Position && A.NodeId == B.NodeId && A.LinkId == B.LinkId &&
						 A.SectionIndex == B.SectionIndex && A.Kind == B.Kind);
		}
		auto Broken = P;
		Broken.Nodes[0].Sections.SetNum(1);
		TestFalse(TEXT("Reject a straight-only space"), UCourtyardLayoutLibrary::ValidateCourtyardPlan(Broken, Error));
		Broken = P;
		Broken.Nodes[0].Sections.SetNum(2);
		Broken.Nodes[0].Sections[0].Min = {0, 0};
		Broken.Nodes[0].Sections[0].Size = {10, 2};
		Broken.Nodes[0].Sections[1].Min = {4, -4};
		Broken.Nodes[0].Sections[1].Size = {2, 10};
		TestFalse(TEXT("Reject middle-to-middle cross"), UCourtyardLayoutLibrary::ValidateCourtyardPlan(Broken, Error));
		Broken = P;
		Broken.Links[0].Direction = 99;
		TestFalse(TEXT("Reject bad tunnel direction safely"),
				  UCourtyardLayoutLibrary::ValidateCourtyardPlan(Broken, Error));
	}
	TestTrue(TEXT("Room plans appear"), RoomPlans > 100);
	TestTrue(TEXT("Packing improves floor density rather than just scaling down"),
			 DenseBounds > 0 && LegacyBounds > 0 && DenseFloors / DenseBounds > LegacyFloors / LegacyBounds + .05);

	FCourtyardSettings Extreme;
	Extreme.StreetCount = 999;
	Extreme.MinBlockGap = -99;
	Extreme.MaxBlockGap = 999;
	Extreme.MinStreetLength = -1;
	Extreme.MaxStreetLength = -99;
	FCourtyardPlan Safe;
	FString Error;
	TestTrue(TEXT("Extreme settings bounded"), UCourtyardLayoutLibrary::BuildCourtyardPlan(Extreme, 1, Safe, Error));
	return true;
}
#endif
