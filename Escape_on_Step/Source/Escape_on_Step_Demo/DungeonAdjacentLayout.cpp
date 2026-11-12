#include "DungeonAdjacentLayout.h"

namespace DungeonAdjacent
{
namespace
{
// 方角は東・北・西・南の順。壁のキーと開口位置でも同じ順番を使う。
const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

// 共有壁を同じキーに正規化する。西/南の壁は隣セルの東/北として表す。
FIntVector MakeEdgeKey(FIntPoint P, int32 Direction)
{
	return Direction % 2 == 0 ? FIntVector(P.X - (Direction == 2), P.Y, 0) : FIntVector(P.X, P.Y - (Direction == 3), 1);
}

// 両端とも他の壁に接せず、両側に床がある単独の壁だけを取り除く。
void RemoveIsolatedInteriorWalls(FLayout& Layout)
{
	// Express the corners in grid units, with each cell occupying [X,X+1] by [Y,Y+1].
	const auto StartCorner = [](FIntVector Wall) { return FIntPoint(Wall.X + (Wall.Z == 0), Wall.Y + (Wall.Z == 1)); };
	const auto WallStep = [](FIntVector Wall) { return FIntPoint(Wall.Z, 1 - Wall.Z); };
	TMap<FIntPoint, int32> Connections;
	for (FIntVector Wall : Layout.Walls)
	{
		const FIntPoint Start = StartCorner(Wall);
		++Connections.FindOrAdd(Start);
		++Connections.FindOrAdd(Start + WallStep(Wall));
	}

	for (auto It = Layout.Walls.CreateIterator(); It; ++It)
	{
		const FIntVector Wall = *It;
		const FIntPoint Start = StartCorner(Wall);
		const FIntPoint Cell(Wall.X, Wall.Y);
		if (Connections.FindChecked(Start) == 1 && Connections.FindChecked(Start + WallStep(Wall)) == 1 &&
			Layout.Cells.Contains(Cell) && Layout.Cells.Contains(Cell + Steps[Wall.Z]))
		{
			It.RemoveCurrent();
		}
	}
}

// 同じ部屋の出入口が一直線や隣のレーンに並ぶと見通せすぎるため避ける。
bool CanPlaceDoor(const FLayout& Layout, int32 Room, FIntVector EdgeKey)
{
	for (const FDoor& Door : Layout.Doors)
	{
		if (Door.A != Room && Door.B != Room)
		{
			continue;
		}
		for (const FIntVector Other : Door.Edges)
		{
			if (Other.Z == EdgeKey.Z && FMath::Abs(Other[1 - Other.Z] - EdgeKey[1 - EdgeKey.Z]) <= 1)
			{
				return false;
			}
		}
	}
	return true;
}

// 共有境界の両端に余白を残し、1～2セル幅の開口候補から抽選する。
TArray<FIntVector> FindOpening(const FLayout& Layout, int32 AId, const FRoom& B, int32 BId, FRandomStream& Random)
{
	const FRoom& A = Layout.Rooms[AId];
	TArray<TArray<FIntVector>> Choices;
	for (int32 D = 0; D < 4; ++D)
	{
		const bool AlongY = D % 2 == 0;
		const int32 Fixed = D == 0 ? A.X + A.W - 1 : D == 1 ? A.Y + A.H - 1 : D == 2 ? A.X : A.Y;
		const int32 Begin = AlongY ? A.Y : A.X;
		const int32 End = Begin + (AlongY ? A.H : A.W);
		const auto Cell = [&](int32 I) { return AlongY ? FIntPoint(Fixed, I) : FIntPoint(I, Fixed); };
		const auto Shares = [&](int32 I) { return A.Contains(Cell(I)) && B.Contains(Cell(I) + Steps[D]); };
		for (int32 I = Begin + 1; I < End - 1; ++I)
		{
			if (!Shares(I - 1) || !Shares(I) || !Shares(I + 1))
			{
				continue;
			}
			const FIntVector First = MakeEdgeKey(Cell(I), D);
			if (!CanPlaceDoor(Layout, AId, First) || !CanPlaceDoor(Layout, BId, First))
			{
				continue;
			}
			Choices.Add({First});
			if (!Shares(I + 2))
			{
				continue;
			}
			const FIntVector Second = MakeEdgeKey(Cell(I + 1), D);
			if (CanPlaceDoor(Layout, AId, Second) && CanPlaceDoor(Layout, BId, Second))
			{
				Choices.Add({First, Second});
			}
		}
	}
	return Choices.IsEmpty() ? TArray<FIntVector>() : MoveTemp(Choices[Random.RandRange(0, Choices.Num() - 1)]);
}

// 接続済みの部屋間距離を幅優先で調べる。未訪問の部屋だけキューへ加える。
// 負のIDは専用通路。ここではその通路の反対側の部屋を直接たどる。
TArray<int32> CalculateRoomDistances(const FLayout& Layout, int32 StartRoom)
{
	TArray<int32> Distance;
	Distance.Init(INDEX_NONE, Layout.Rooms.Num());
	TArray<int32> RoomsToVisit{StartRoom};
	Distance[StartRoom] = 0;
	for (int32 J = 0; J < RoomsToVisit.Num(); ++J)
	{
		const int32 Current = RoomsToVisit[J];
		const auto Visit = [&](int32 Next)
		{
			if (Distance[Next] != INDEX_NONE)
			{
				return;
			}
			Distance[Next] = Distance[Current] + 1;
			RoomsToVisit.Add(Next);
		};
		for (const FDoor& Door : Layout.Doors)
		{
			const int32 Other = Door.A == Current ? Door.B : Door.B == Current ? Door.A : INDEX_NONE;
			if (Door.A != Current && Door.B != Current)
			{
				continue;
			}
			if (Other >= 0)
			{
				Visit(Other);
			}
			else
			{
				for (const FDoor& End : Layout.Doors)
				{
					if (End.B == Other)
					{
						Visit(End.A);
					}
				}
			}
		}
	}
	return Distance;
}

// 出口が1つの部屋から空き地を探索し、別の部屋へ戻る曲がり通路を作る。
void AddReturnCorridors(FLayout& Layout, FIntPoint Min, FIntPoint Max, FRandomStream& Random)
{
	struct FExit
	{
		FIntPoint Outside;
		FIntVector Door;
		int32 Room;
	};
	const auto InBounds = [&](FIntPoint P) { return P.X >= Min.X && P.X <= Max.X && P.Y >= Min.Y && P.Y <= Max.Y; };
	const auto Free = [&](FIntPoint P) { return InBounds(P) && !Layout.Cells.Contains(P); };
	TArray<int32> Degree;
	Degree.Init(0, Layout.Rooms.Num());
	for (const FDoor& Door : Layout.Doors)
	{
		++Degree[Door.A];
		++Degree[Door.B];
	}

	// Record candidates in grid order so a seed is independent of hash iteration.
	TArray<FExit> Exits;
	for (int32 X = Min.X; X <= Max.X; ++X)
	{
		for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
		{
			const FIntPoint Cell(X, Y);
			const int32* Room = Layout.Cells.Find(Cell);
			if (!Room || *Room < 0)
			{
				continue;
			}
			for (int32 D = 0; D < 4; ++D)
			{
				if (Free(Cell + Steps[D]))
				{
					Exits.Add({Cell + Steps[D], MakeEdgeKey(Cell, D), *Room});
				}
			}
		}
	}

	const int32 Offset = Random.RandRange(0, Layout.Rooms.Num() - 1);
	for (int32 I = 0; I < Layout.Rooms.Num(); ++I)
	{
		const int32 Room = (I + Offset) % Layout.Rooms.Num();
		if (Degree[Room] != 1)
		{
			continue;
		}

		// Prefer a return to a different branch, especially another dead end.
		const TArray<int32> Distance = CalculateRoomDistances(Layout, Room);

		// 複数の出口を始点にした幅優先探索。Previousを残して経路を復元する。
		struct FRouteStep
		{
			FIntPoint Cell;
			int32 Previous;
			int32 Length;
		};
		TArray<FRouteStep> Queue;
		TMap<FIntPoint, int32> Seen;
		TMap<FIntPoint, int32> Origins;
		for (int32 J = 0; J < Exits.Num(); ++J)
		{
			const FExit& Exit = Exits[J];
			if (Exit.Room != Room || !Free(Exit.Outside) || Seen.Contains(Exit.Outside) ||
				!CanPlaceDoor(Layout, Room, Exit.Door))
			{
				continue;
			}
			Seen.Add(Exit.Outside, Queue.Add({Exit.Outside, INDEX_NONE, 1}));
			Origins.Add(Exit.Outside, J);
		}
		for (int32 J = 0; J < Queue.Num(); ++J)
		{
			const FRouteStep Step = Queue[J];
			for (FIntPoint Direction : Steps)
			{
				const FIntPoint Next = Step.Cell + Direction;
				if (Free(Next) && !Seen.Contains(Next))
				{
					Seen.Add(Next, Queue.Add({Next, J, Step.Length + 1}));
				}
			}
		}

		// 接続上の遠さ・短い経路・曲がり・行き止まり同士の救済で候補を採点する。
		int32 BestScore = MIN_int32;
		const FExit* Destination = nullptr;
		const FExit* Origin = nullptr;
		int32 EndIndex = INDEX_NONE;
		for (const FExit& Exit : Exits)
		{
			if (Exit.Room == Room || !Free(Exit.Outside) || !CanPlaceDoor(Layout, Exit.Room, Exit.Door))
			{
				continue;
			}
			const int32* Index = Seen.Find(Exit.Outside);
			if (!Index)
			{
				continue;
			}
			const FRouteStep& End = Queue[*Index];
			int32 Root = *Index;
			int32 Axis = Exit.Door.Z;
			int32 Turns = 0;
			while (Queue[Root].Previous != INDEX_NONE)
			{
				const int32 Previous = Queue[Root].Previous;
				const int32 StepAxis = Queue[Root].Cell.X == Queue[Previous].Cell.X ? 1 : 0;
				Turns += Axis != StepAxis;
				Axis = StepAxis;
				Root = Previous;
			}
			const FExit& Start = Exits[Origins.FindChecked(Queue[Root].Cell)];
			Turns += Axis != Start.Door.Z;
			if (Turns == 0)
			{
				continue;
			}
			const int32 Score =
				3 * Distance[Exit.Room] - 2 * End.Length + 6 * FMath::Min(Turns, 3) + (Degree[Exit.Room] == 1 ? 16 : 0);
			if (Score <= BestScore)
			{
				continue;
			}
			BestScore = Score;
			Destination = &Exit;
			Origin = &Start;
			EndIndex = *Index;
		}
		if (!Destination)
		{
			continue;
		}

		// 専用通路は部屋数に含めず、-1, -2, ... のIDでCellsに記録する。
		const int32 Corridor = -1 - Layout.Corridors++;
		Layout.Doors.Add({Room, Corridor, {Origin->Door}});
		Layout.Doors.Add({Destination->Room, Corridor, {Destination->Door}});
		++Degree[Room];
		++Degree[Destination->Room];
		for (int32 J = EndIndex; J != INDEX_NONE; J = Queue[J].Previous)
		{
			Layout.Cells.Add(Queue[J].Cell, Corridor);
		}
	}
}

// 専用通路の途中から第三の部屋へ抜ける短い曲がり分岐を最大3本追加する。
void AddCorridorBranches(FLayout& Layout, FIntPoint Min, FIntPoint Max)
{
	TArray<FIntPoint> Cells;
	Layout.Cells.GetKeys(Cells);
	Cells.Sort([](FIntPoint A, FIntPoint B) { return A.X != B.X ? A.X < B.X : A.Y < B.Y; });
	TSet<int32> Branched;
	for (const FIntPoint Cell : Cells)
	{
		const int32 Corridor = Layout.Cells.FindChecked(Cell);
		if (Corridor >= 0 || Branched.Contains(Corridor))
		{
			continue;
		}
		bool Added = false;
		for (int32 D = 0; D < 4 && !Added; ++D)
		{
			const FIntPoint Branch = Cell + Steps[D];
			if (Branch.X < Min.X || Branch.X > Max.X || Branch.Y < Min.Y || Branch.Y > Max.Y ||
				Layout.Cells.Contains(Branch))
			{
				continue;
			}
			for (int32 Turn : {1, 3})
			{
				const int32 E = (D + Turn) % 4;
				const int32* Room = Layout.Cells.Find(Branch + Steps[E]);
				if (!Room || *Room < 0)
				{
					continue;
				}
				const FIntVector Door = MakeEdgeKey(Branch, E);
				if (!CanPlaceDoor(Layout, *Room, Door) ||
					Layout.Doors.ContainsByPredicate([&](const FDoor& Existing)
													 { return Existing.A == *Room && Existing.B == Corridor; }))
				{
					continue;
				}
				Layout.Doors.Add({*Room, Corridor, {Door}});
				Layout.Cells.Add(Branch, Corridor);
				Branched.Add(Corridor);
				++Layout.Branches;
				Added = true;
				break;
			}
		}
		if (Layout.Branches >= 3)
		{
			break;
		}
	}
}

// 空き3×3セルに、通路側の出入口を2つ取れる場合だけ小部屋を増築する。
// 追加は最大2室、総部屋数は30室まで。
void AddCorridorRooms(FLayout& Layout, FIntPoint Min, FIntPoint Max)
{
	const int32 MaxRooms = FMath::Min(30, Layout.Rooms.Num() + 2);
	for (int32 X = Min.X; X <= Max.X - 2 && Layout.Rooms.Num() < MaxRooms; ++X)
	{
		for (int32 Y = Min.Y; Y <= Max.Y - 2 && Layout.Rooms.Num() < MaxRooms; ++Y)
		{
			const FRoom Room{X, Y, 3, 3};
			bool Clear = true;
			for (int32 DX = 0; DX < 3; ++DX)
			{
				for (int32 DY = 0; DY < 3; ++DY)
				{
					Clear &= !Layout.Cells.Contains({X + DX, Y + DY});
				}
			}
			if (!Clear)
			{
				continue;
			}

			struct FContact
			{
				int32 Corridor;
				FIntPoint Floor;
				FIntVector Door;
			};
			TArray<FContact> Contacts;
			for (int32 D = 0; D < 4; ++D)
			{
				for (int32 I = 0; I < 3; ++I)
				{
					const FIntPoint Cell = D == 0	? FIntPoint(X + 2, Y + I)
										   : D == 1 ? FIntPoint(X + I, Y + 2)
										   : D == 2 ? FIntPoint(X, Y + I)
													: FIntPoint(X + I, Y);
					const FIntPoint Outside = Cell + Steps[D];
					const int32* Corridor = Layout.Cells.Find(Outside);
					if (Corridor && *Corridor < 0)
					{
						Contacts.Add({*Corridor, Outside, MakeEdgeKey(Cell, D)});
					}
				}
			}

			// The annex must be passable from both ends, with no new single-door room.
			bool Added = false;
			for (int32 A = 0; A < Contacts.Num() && !Added; ++A)
			{
				for (int32 B = A + 1; B < Contacts.Num(); ++B)
				{
					const FContact& First = Contacts[A];
					const FContact& Second = Contacts[B];
					const FIntPoint Delta = First.Floor - Second.Floor;
					if (FMath::Abs(Delta.X) + FMath::Abs(Delta.Y) < 2)
					{
						continue;
					}
					if (First.Door.Z == Second.Door.Z &&
						FMath::Abs(First.Door[1 - First.Door.Z] - Second.Door[1 - Second.Door.Z]) < 2)
					{
						continue;
					}

					const int32 Id = Layout.Rooms.Add(Room);
					for (int32 DX = 0; DX < 3; ++DX)
					{
						for (int32 DY = 0; DY < 3; ++DY)
						{
							Layout.Cells.Add({X + DX, Y + DY}, Id);
						}
					}
					Layout.Doors.Add({Id, First.Corridor, {First.Door}});
					Layout.Doors.Add({Id, Second.Corridor, {Second.Door}});
					++Layout.CorridorRooms;
					Added = true;
					break;
				}
			}
		}
	}
}

// 室の外接矩形ではなく、L字の欠けを除いた実床セルだけ登録する。
void AddRoom(FLayout& Layout, const FRoom& Room)
{
	const int32 Id = Layout.Rooms.Add(Room);
	for (int32 X = Room.X; X < Room.X + Room.W; ++X)
	{
		for (int32 Y = Room.Y; Y < Room.Y + Room.H; ++Y)
		{
			if (Room.Contains({X, Y}))
			{
				Layout.Cells.Add({X, Y}, Id);
			}
		}
	}
}

// 初期の直接接続に加え、接している別の部屋にも35%の抽選で開口する。
void AddExtraDoors(FLayout& Layout, FRandomStream& Random)
{
	for (int32 A = 0; A < Layout.Rooms.Num(); ++A)
	{
		for (int32 B = A + 1; B < Layout.Rooms.Num(); ++B)
		{
			if (Layout.Doors.ContainsByPredicate(
					[&](const FDoor& Door) { return (Door.A == A && Door.B == B) || (Door.A == B && Door.B == A); }))
			{
				continue;
			}
			if (Random.FRand() > 0.35f)
			{
				continue;
			}
			auto Door = FindOpening(Layout, A, Layout.Rooms[B], B, Random);
			if (!Door.IsEmpty())
			{
				Layout.Doors.Add({A, B, MoveTemp(Door)});
			}
		}
	}
}

// 床の外周と異なる所有IDの境界を壁にし、確定した開口を差し引く。
void BuildWalls(FLayout& Layout)
{
	for (const auto& Pair : Layout.Cells)
	{
		for (int32 D = 0; D < 4; ++D)
		{
			const int32* Other = Layout.Cells.Find(Pair.Key + Steps[D]);
			if (!Other || *Other != Pair.Value)
			{
				Layout.Walls.Add(MakeEdgeKey(Pair.Key, D));
			}
		}
	}
	for (const FDoor& Door : Layout.Doors)
	{
		for (const FIntVector Key : Door.Edges)
		{
			Layout.Walls.Remove(Key);
		}
	}
	RemoveIsolatedInteriorWalls(Layout);
}

} // namespace

// 1案を生成する。アクター・メッシュは作らず、グリッド上のデータだけを返す。
static FLayout BuildAttempt(FIntPoint Min, FIntPoint Max, int32 Target, int32 Seed)
{
	FLayout Layout;
	const int32 Width = Max.X - Min.X + 1, Height = Max.Y - Min.Y + 1;
	if (Width < 8 || Height < 8 || Width > 128 || Height > 128)
	{
		return Layout;
	}
	FRandomStream Random(Seed);
	const bool Standard = Target <= 0;
	Target = Standard ? Random.RandRange(24, 30) : FMath::Clamp(Target, 1, 40);

	// 1. 中央に最初の部屋を置く。乱数呼び出し順はシード再現のため固定する。
	const int32 W = Random.RandRange(5, FMath::Min(7, Width - 2));
	const int32 H = Random.RandRange(5, FMath::Min(7, Height - 2));
	AddRoom(Layout, {Min.X + (Width - W) / 2, Min.Y + (Height - H) / 2, W, H});

	// 2. 既存の部屋に隣接する候補を作り、範囲・重複・開口を検査して追加する。
	for (int32 Attempt = 0; Attempt < 5000 && Layout.Rooms.Num() < Target; ++Attempt)
	{
		const int32 Parent =
			Random.FRand() < 0.35f ? Layout.Rooms.Num() - 1 : Random.RandRange(0, Layout.Rooms.Num() - 1);
		const FRoom& A = Layout.Rooms[Parent];
		const int32 D = Random.RandRange(0, 3);
		FRoom B;
		switch (Random.RandRange(0, 3))
		{
		case 0:
			B.W = 3;
			B.H = Random.RandRange(5, 7);
			break;
		case 1:
			B.W = Random.RandRange(5, 7);
			B.H = 3;
			break;
		case 2:
			B.W = Random.RandRange(4, 6);
			B.H = Random.RandRange(4, 6);
			B.CutCorner = Random.RandRange(0, 3);
			break;
		default:
			B.W = Random.RandRange(3, 5);
			B.H = Random.RandRange(3, 5);
			break;
		}
		const int32 Shift = Random.RandRange(1, 3) * (Random.RandRange(0, 1) ? 1 : -1);
		if (D % 2 == 0)
		{
			B.X = D == 0 ? A.X + A.W : A.X - B.W;
			B.Y = A.Y + Shift;
		}
		else
		{
			B.Y = D == 1 ? A.Y + A.H : A.Y - B.H;
			B.X = A.X + Shift;
		}
		if (B.X < Min.X || B.Y < Min.Y || B.X + B.W - 1 > Max.X || B.Y + B.H - 1 > Max.Y)
		{
			continue;
		}

		bool Occupied = false;
		for (int32 X = B.X; X < B.X + B.W && !Occupied; ++X)
		{
			for (int32 Y = B.Y; Y < B.Y + B.H; ++Y)
			{
				if (B.Contains({X, Y}) && Layout.Cells.Contains({X, Y}))
				{
					Occupied = true;
					break;
				}
			}
		}
		if (Occupied)
		{
			continue;
		}

		const int32 Id = Layout.Rooms.Num();
		auto Door = FindOpening(Layout, Parent, B, Id, Random);
		if (Door.IsEmpty())
		{
			continue;
		}
		AddRoom(Layout, B);
		Layout.Doors.Add({Parent, Id, MoveTemp(Door)});
	}

	// 3. 直接の追加開口 → 戻り通路 → 分岐 → 通路沿いの小部屋。
	AddExtraDoors(Layout, Random);

	AddReturnCorridors(Layout, Min, Max, Random);
	AddCorridorBranches(Layout, Min, Max);
	if (Standard)
	{
		AddCorridorRooms(Layout, Min, Max);
	}

	// 4. 間取りが確定してから壁を作る。
	BuildWalls(Layout);
	return Layout;
}

// 最大8案から条件を満たす案を返す。全案が不採用なら最良案を呼び出し元へ返す。
// その場合、標準モードの最終合否・再生成はUE側アダプターが判断する。
FLayout Build(FIntPoint Min, FIntPoint Max, int32 Target, int32 Seed)
{
	FLayout Best;
	FRandomStream RetrySeeds(Seed);
	for (int32 Attempt = 0; Attempt < 8; ++Attempt)
	{
		FLayout Layout = BuildAttempt(Min, Max, Target, Attempt == 0 ? Seed : int32(RetrySeeds.GetUnsignedInt()));
		if (Layout.Rooms.Num() <= 1)
		{
			return Layout;
		}
		// ここで数えるDeadEndsは「出口が2つ未満の部屋数」。床セルの袋小路数とは違う。
		TArray<int32> Degree;
		Degree.Init(0, Layout.Rooms.Num());
		for (const FDoor& Door : Layout.Doors)
		{
			++Degree[Door.A];
			if (Door.B >= 0)
			{
				++Degree[Door.B];
			}
		}
		for (int32 Count : Degree)
		{
			Layout.DeadEnds += Count < 2;
		}
		if (Layout.DeadEnds == 0 && Layout.Rooms.Num() >= (Target <= 0 ? 24 : FMath::Min(40, Target)))
		{
			return Layout;
		}
		if (Best.Rooms.IsEmpty() || Layout.Rooms.Num() > Best.Rooms.Num() ||
			(Layout.Rooms.Num() == Best.Rooms.Num() && Layout.DeadEnds < Best.DeadEnds))
		{
			Best = MoveTemp(Layout);
		}
	}
	return Best;
}
} // namespace DungeonAdjacent
