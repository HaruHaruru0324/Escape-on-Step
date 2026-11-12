#if WITH_DEV_AUTOMATION_TESTS
#include "DungeonAdjacentLayout.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAdjacentLayoutTest, "EscapeOnStep.Dungeon.AdjacentRooms",
								 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FAdjacentLayoutTest::RunTest(const FString&)
{
	using namespace DungeonAdjacent;
	int32 LayoutsWithCorridors = 0;
	int32 Branches = 0, CorridorRooms = 0;
	// 100シードで部屋・床・開口・壁・周回路の不変条件を検査する。
	for (int32 Seed = 0; Seed < 100; ++Seed)
	{
		const auto L = Build({0, 0}, {39, 39}, 0, Seed);
		TestTrue(TEXT("Generates a playable group of rooms"), L.Rooms.Num() >= 24 && L.Rooms.Num() <= 30);
		LayoutsWithCorridors += L.Corridors > 0;
		Branches += L.Branches;
		CorridorRooms += L.CorridorRooms;
		const int32 DirectDoors = L.Doors.Num() - 2 * L.Corridors - L.Branches - 2 * L.CorridorRooms;
		TestTrue(TEXT("The direct doors alone can connect the rooms"),
				 DirectDoors >= L.Rooms.Num() - L.CorridorRooms - 1);
		TestTrue(TEXT("Each bent corridor adds an independent cycle"),
				 L.Doors.Num() - L.Rooms.Num() - L.Corridors + 1 >= L.Corridors);
		TestEqual(TEXT("Every room has a second exit"), L.DeadEnds, 0);
		int32 Area = 0;
		for (const auto& Cell : L.Cells)
		{
			Area += Cell.Value < 0;
		}
		for (const auto& R : L.Rooms)
		{
			Area += R.W * R.H - (R.CutCorner == INDEX_NONE ? 0 : 4);
			TestTrue(TEXT("Room center belongs to footprint"), R.Contains(R.Center()));
			TestTrue(TEXT("Within map bounds"), R.X >= 0 && R.Y >= 0 && R.X + R.W <= 40 && R.Y + R.H <= 40);
		}
		TestEqual(TEXT("No overlapping rooms"), Area, L.Cells.Num());
		// 壁キーを考慮した床グラフの幅優先探索。孤立した床がないことを確認する。
		TSet<FIntPoint> Visited;
		TArray<FIntPoint> Queue;
		const auto& First = L.Rooms[0];
		Queue.Add(First.Center());
		Visited.Add(Queue[0]);
		const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
		for (int32 I = 0; I < Queue.Num(); ++I)
		{
			for (int32 D = 0; D < 4; ++D)
			{
				const auto P = Queue[I], Q = P + Steps[D];
				const auto E = D == 0	? FIntVector(P.X, P.Y, 0)
							   : D == 1 ? FIntVector(P.X, P.Y, 1)
							   : D == 2 ? FIntVector(P.X - 1, P.Y, 0)
										: FIntVector(P.X, P.Y - 1, 1);
				if (L.Cells.Contains(Q) && !L.Walls.Contains(E) && !Visited.Contains(Q))
				{
					Visited.Add(Q);
					Queue.Add(Q);
				}
			}
		}
		TestEqual(TEXT("Every floor cell is reachable through the planned doors"), Visited.Num(), L.Cells.Num());
		// 内部の単独壁と、同室の開口が一直線に並ぶ配置を検出する。
		TMap<FIntPoint, int32> WallEnds;
		for (const auto& E : L.Walls)
		{
			++WallEnds.FindOrAdd({E.X + (E.Z == 0), E.Y + (E.Z == 1)});
			++WallEnds.FindOrAdd({E.X + 1, E.Y + 1});
		}
		for (const auto& E : L.Walls)
		{
			const FIntPoint P(E.X, E.Y);
			const bool Interior = L.Cells.Contains(P) && L.Cells.Contains(P + Steps[E.Z]);
			if (Interior)
			{
				TestTrue(TEXT("Interior wall touches another wall"),
						 WallEnds.FindChecked({E.X + (E.Z == 0), E.Y + (E.Z == 1)}) > 1 ||
							 WallEnds.FindChecked({E.X + 1, E.Y + 1}) > 1);
			}
		}
		for (const auto& D : L.Doors)
		{
			TestTrue(TEXT("Opening is one or two cells"), D.Edges.Num() >= 1 && D.Edges.Num() <= 2);
			for (auto E : D.Edges)
			{
				TestFalse(TEXT("No wall in an opening"), L.Walls.Contains(E));
			}
		}
		for (int32 I = 0; I < L.Doors.Num(); ++I)
		{
			for (int32 J = I + 1; J < L.Doors.Num(); ++J)
			{
				const auto& A = L.Doors[I];
				const auto& B = L.Doors[J];
				if (A.A != B.A && A.A != B.B && A.B != B.A && (A.B < 0 || A.B != B.B))
				{
					continue;
				}
				for (auto E : A.Edges)
				{
					for (auto F : B.Edges)
					{
						if (E.Z == F.Z)
						{
							TestTrue(TEXT("Doors sharing a room have staggered sight lines"),
									 FMath::Abs(E.Z == 0 ? E.Y - F.Y : E.X - F.X) > 1);
						}
					}
				}
			}
		}
		const auto Again = Build({0, 0}, {39, 39}, 0, Seed);
		TestTrue(TEXT("Same seed produces same walls"), L.Walls.Includes(Again.Walls) && Again.Walls.Includes(L.Walls));
	}
	TestTrue(TEXT("Bent corridors appear across the seed sample"), LayoutsWithCorridors >= 90);
	TestTrue(TEXT("Seed sample produces branches and corridor rooms"), Branches > 0 && CorridorRooms > 0);
	TestTrue(TEXT("Too small bounds fail safely"), Build({0, 0}, {2, 2}, 14, 1).Rooms.IsEmpty());
	return true;
}
#endif
