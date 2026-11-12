#include "CourtyardLayout.h"

namespace
{
const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
bool Fail(FString& Error, const TCHAR* Message)
{
	Error = Message;
	return false;
}
bool InRect(FIntPoint P, const FCourtyardRoomSection& R)
{
	return P.X >= R.Min.X && P.Y >= R.Min.Y && P.X < R.Min.X + R.Size.X && P.Y < R.Min.Y + R.Size.Y;
}
bool Contains(const FCourtyardNode& N, FIntPoint P)
{
	return N.FloorCells.Contains(P);
}
int32 SectionAxis(const FCourtyardRoomSection& R)
{
	return R.Size.X > R.Size.Y ? 0 : 1;
}
int32 Along(FIntPoint P, int32 Axis)
{
	return Axis == 0 ? P.X : P.Y;
}

// 2本の長方形が、それぞれの「端」の角だけでL字に重なることを確認する。
// 片方でも中央に重なればT字/十字の継ぎ足しなので不採用。
bool BuildSectionChain(const TArray<FCourtyardRoomSection>& Sections, TArray<TArray<int32>>& Adj, FString& Error)
{
	if (Sections.Num() < 2 || Sections.Num() > 4)
	{
		return Fail(Error, TEXT("A wide space needs 2..4 turned sections"));
	}
	Adj.SetNum(Sections.Num());
	int32 Edges = 0;
	for (const auto& R : Sections)
	{
		if (R.Size.X < 2 || R.Size.Y < 2 || R.Size.X > 256 || R.Size.Y > 256 || R.Size.X == R.Size.Y)
		{
			return Fail(Error, TEXT("Invalid straight section dimensions"));
		}
	}
	for (int32 I = 0; I < Sections.Num(); ++I)
	{
		for (int32 J = I + 1; J < Sections.Num(); ++J)
		{
			const auto& A = Sections[I];
			const auto& B = Sections[J];
			const FIntPoint Min(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y));
			const FIntPoint Max(FMath::Min(A.Min.X + A.Size.X, B.Min.X + B.Size.X),
								FMath::Min(A.Min.Y + A.Size.Y, B.Min.Y + B.Size.Y));
			if (Max.X <= Min.X || Max.Y <= Min.Y)
			{
				continue;
			}
			if (SectionAxis(A) == SectionAxis(B))
			{
				return Fail(Error, TEXT("Parallel room sections overlap"));
			}
			const auto& H = SectionAxis(A) == 0 ? A : B;
			const auto& V = SectionAxis(A) == 1 ? A : B;
			const bool AtHEnd = Min.X == H.Min.X || Max.X == H.Min.X + H.Size.X;
			const bool AtVEnd = Min.Y == V.Min.Y || Max.Y == V.Min.Y + V.Size.Y;
			if (!AtHEnd || !AtVEnd || Min.X != V.Min.X || Max.X != V.Min.X + V.Size.X || Min.Y != H.Min.Y ||
				Max.Y != H.Min.Y + H.Size.Y)
			{
				return Fail(Error, TEXT("Sections must meet at both ends, never at their middle"));
			}
			Adj[I].Add(J);
			Adj[J].Add(I);
			++Edges;
		}
	}
	if (Edges != Sections.Num() - 1)
	{
		return Fail(Error, TEXT("Section chain is disconnected or branched"));
	}
	int32 Ends = 0;
	for (const auto& Neighbors : Adj)
	{
		if (Neighbors.Num() == 1)
		{
			++Ends;
		}
		else if (Neighbors.Num() != 2)
		{
			return Fail(Error, TEXT("Cross-shaped section junction"));
		}
	}
	return Ends == 2 ? true : Fail(Error, TEXT("A section chain needs two ends"));
}
bool OrderSections(FCourtyardNode& N, FString& Error)
{
	TArray<TArray<int32>> Adj;
	if (!BuildSectionChain(N.Sections, Adj, Error))
	{
		return false;
	}
	int32 Current = 0;
	while (Adj[Current].Num() != 1)
	{
		++Current;
	}
	TArray<FCourtyardRoomSection> Ordered;
	int32 Previous = INDEX_NONE;
	for (int32 I = 0; I < N.Sections.Num(); ++I)
	{
		Ordered.Add(N.Sections[Current]);
		int32 Next = INDEX_NONE;
		for (int32 Candidate : Adj[Current])
		{
			if (Candidate != Previous)
			{
				Next = Candidate;
			}
		}
		if (Next == INDEX_NONE)
		{
			break;
		}
		Previous = Current;
		Current = Next;
	}
	if (Ordered.Num() != N.Sections.Num())
	{
		return Fail(Error, TEXT("Section chain cannot be ordered"));
	}
	N.Sections = MoveTemp(Ordered);
	return true;
}
// 接続口も長方形の中央ではなく、連結鎖の両端にある自由な端へ置く。
} // namespace

// 分岐口は各矩形の端の角に限る。広い区画の中央へ別の広い区画を継ぎ足さない。
static bool AtSectionEnd(const FCourtyardNode& N, FIntPoint P)
{
	for (const auto& R : N.Sections)
	{
		if (!InRect(P, R))
		{
			continue;
		}
		const int32 Axis = SectionAxis(R), T = Along(P - R.Min, Axis);
		const int32 Width = Along(R.Size, 1 - Axis), Length = Along(R.Size, Axis);
		if (T < Width || T >= Length - Width)
		{
			return true;
		}
	}
	return false;
}

// 両端の矩形で、接続口のない長い腕だけを検出・短縮する。
// 接続済みの腕を途中で切らず、曲がり角から最大1セルの出っ張りを残す。
static bool FixBlindEnds(FCourtyardNode& N, const TArray<FIntPoint>& Ports, bool Trim)
{
	bool Valid = true;
	// 袋小路は複数の曲がり角にまたがる場合もある。出口のない末端区画を
	// 順に戻し、最初の接続口がある区画まで追う。最後の1本だけの短縮では不十分。
	bool Removed = true;
	while (Removed && N.Sections.Num() > 2)
	{
		Removed = false;
		for (int32 End : {0, N.Sections.Num() - 1})
		{
			const auto& R = N.Sections[End];
			const auto& Neighbor = N.Sections[End == 0 ? 1 : End - 1];
			bool CanRemove = true;
			for (auto P : Ports)
			{
				if (InRect(P, R) && !InRect(P, Neighbor))
				{
					CanRemove = false;
					break;
				}
			}
			if (CanRemove)
			{
				if (!Trim)
				{
					return false;
				}
				N.Sections.RemoveAt(End);
				Removed = true;
				break;
			}
		}
	}
	for (int32 End : {0, N.Sections.Num() - 1})
	{
		auto& R = N.Sections[End];
		const auto& Join = N.Sections[End == 0 ? 1 : End - 1];
		const int32 Axis = SectionAxis(R), Width = Along(R.Size, 1 - Axis), Length = Along(R.Size, Axis);
		const bool JoinedAtStart = Along(Join.Min, Axis) <= Along(R.Min, Axis);
		bool Exit = false;
		for (auto P : Ports)
		{
			if (InRect(P, R))
			{
				const int32 T = Along(P - R.Min, Axis);
				Exit |= JoinedAtStart ? T >= Length - Width : T < Width;
			}
		}
		if (Exit || Length <= Width + 1)
		{
			continue;
		}
		Valid = false;
		if (Trim)
		{
			if (!JoinedAtStart)
			{
				if (Axis == 0)
				{
					R.Min.X += Length - Width - 1;
				}
				else
				{
					R.Min.Y += Length - Width - 1;
				}
			}
			if (Axis == 0)
			{
				R.Size.X = Width + 1;
			}
			else
			{
				R.Size.Y = Width + 1;
			}
		}
	}
	return Valid;
}

bool UCourtyardLayoutLibrary::BuildCourtyardPlan(const FCourtyardSettings& Input, int32 Seed, FCourtyardPlan& Plan,
												 FString& Error)
{
	Plan = FCourtyardPlan();
	Plan.Seed = Seed;
	Error.Reset();
	FCourtyardSettings S = Input;
	S.StreetCount = FMath::Clamp(S.StreetCount, 9, 24);
	S.StreetWidth = FMath::Clamp(S.StreetWidth, 2, 3);
	S.MinStreetLength = FMath::Clamp(S.MinStreetLength, 4, 12);
	S.MaxStreetLength = FMath::Clamp(S.MaxStreetLength, S.MinStreetLength, 12);
	S.MinTunnelCells = FMath::Clamp(S.MinTunnelCells, 1, 3);
	S.MaxTunnelCells = FMath::Clamp(S.MaxTunnelCells, S.MinTunnelCells, 3);
	S.MinRoomSections = FMath::Clamp(S.MinRoomSections, 2, 4);
	S.MaxRoomSections = FMath::Clamp(S.MaxRoomSections, S.MinRoomSections, 4);
	S.MaxRooms = FMath::Clamp(S.MaxRooms, 0, 6);
	Plan.TunnelCellLimit = S.MaxTunnelCells;
	FRandomStream Random(Seed);
	int32 RoomCount = 0;
	for (int32 I = 0; I < S.StreetCount && RoomCount < S.MaxRooms; ++I)
	{
		RoomCount += Random.FRand() < FMath::Clamp(S.RoomChance, 0.f, 1.f);
	}
	if (S.RoomChance > 0.f)
	{
		RoomCount = FMath::Max(RoomCount, FMath::Min(2, S.MaxRooms));
	}
	const int32 Requested = S.StreetCount;
	const int32 Columns = FMath::CeilToInt(FMath::Sqrt(float(Requested)));
	const int32 Rows = FMath::DivideAndRoundUp(Requested, Columns), Count = Columns * Rows;
	const int32 Width = S.StreetWidth;
	// 偶数の中点を必要としない、端の角で重なる2/3/4本の矩形テンプレート。
	const int32 Span = FMath::Max(2 * Width + 3, Random.RandRange(S.MinStreetLength, S.MaxStreetLength));
	const int32 Extent = Span + Width, Middle = Span / 2;
	TArray<int32> TileX{0}, TileY{0}, Parts, Kinds;
	for (int32 I = 1; I < Columns; ++I)
	{
		TileX.Add(TileX.Last() + Extent + Random.RandRange(S.MinTunnelCells, S.MaxTunnelCells));
	}
	for (int32 I = 1; I < Rows; ++I)
	{
		TileY.Add(TileY.Last() + Extent + Random.RandRange(S.MinTunnelCells, S.MaxTunnelCells));
	}
	// 開始ノードは屋外。残りのルームの配置をシャッフルする。
	TArray<int32> RoomIds;
	for (int32 I = 1; I < Count; ++I)
	{
		RoomIds.Add(I);
	}
	for (int32 I = RoomIds.Num() - 1; I > 0; --I)
	{
		RoomIds.Swap(I, Random.RandRange(0, I));
	}
	Kinds.Init(0, Count);
	for (int32 I = 0; I < RoomCount; ++I)
	{
		Kinds[RoomIds[I]] = 1;
	}
	for (int32 I = 0; I < Count; ++I)
	{
		Parts.Add(Kinds[I] ? Random.RandRange(S.MinRoomSections, S.MaxRoomSections) : Random.RandRange(2, 4));
	}
	struct FCandidate
	{
		int32 A, B, Direction;
		TArray<FIntPoint> Cells;
	};
	TArray<FCandidate> Candidates;
	// 向きを抽選し、接続候補が全体を結べない場合だけ再抽選する。最後は接続可能な向きへ戻す。
	for (int32 Attempt = 0; Attempt < 33; ++Attempt)
	{
		Plan.Nodes.Reset();
		Candidates.Reset();
		for (int32 Id = 0; Id < Count; ++Id)
		{
			FCourtyardNode N;
			N.Id = Id;
			N.Kind = ECourtyardCellKind::Street;
			const FIntPoint Origin(TileX[Id % Columns], TileY[Id / Columns]);
			const int32 Rotation = Attempt == 32 ? 0 : Random.RandRange(0, 3);
			const auto Rotate = [&](FIntPoint P)
			{
				for (int32 I = 0; I < Rotation; ++I)
				{
					P = {Span - P.Y, P.X};
				}
				return P + Origin;
			};
			TArray<FIntPoint> Corners{{0, 0}, {Span, 0}, {Span, Span}};
			if (Parts[Id] >= 3)
			{
				Corners.Add({Parts[Id] == 3 ? 0 : Middle, Span});
			}
			if (Parts[Id] == 4)
			{
				Corners.Add({Middle, Middle});
			}
			for (int32 I = 1; I < Corners.Num(); ++I)
			{
				const auto A = Rotate(Corners[I - 1]), B = Rotate(Corners[I]);
				FCourtyardRoomSection R;
				R.Min = {FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)};
				R.Size = {FMath::Abs(A.X - B.X) + Width, FMath::Abs(A.Y - B.Y) + Width};
				N.Sections.Add(R);
			}
			if (!OrderSections(N, Error))
			{
				return false;
			}
			N.Min = Origin;
			N.Size = {Extent, Extent};
			for (const auto& R : N.Sections)
			{
				for (int32 Dx = 0; Dx < R.Size.X; ++Dx)
				{
					for (int32 Dy = 0; Dy < R.Size.Y; ++Dy)
					{
						N.FloorCells.AddUnique(R.Min + FIntPoint(Dx, Dy));
					}
				}
			}
			Plan.Nodes.Add(N);
		}
		for (int32 Id = 0; Id < Count; ++Id)
		{
			for (int32 Direction = 0; Direction < 2; ++Direction)
			{
				const int32 Other = Direction == 0 ? Id + 1 : Id + Columns;
				if (Other >= Count || (Direction == 0 && Id % Columns == Columns - 1))
				{
					continue;
				}
				const auto Step = Steps[Direction];
				const int32 Gap = Direction == 0 ? TileX[Other % Columns] - TileX[Id % Columns] - Extent
												 : TileY[Other / Columns] - TileY[Id / Columns] - Extent;
				TArray<FCandidate> Choices;
				for (const auto P : Plan.Nodes[Id].FloorCells)
				{
					const auto Q = P + Step * (Gap + 1);
					if (!Contains(Plan.Nodes[Other], Q) || !AtSectionEnd(Plan.Nodes[Id], P) ||
						!AtSectionEnd(Plan.Nodes[Other], Q))
					{
						continue;
					}
					// 両端の間はタイル間の空白だけ。側壁沿いの中途半端な入り口は作らない。
					if (Contains(Plan.Nodes[Id], P + Step) || Contains(Plan.Nodes[Other], Q - Step))
					{
						continue;
					}
					FCandidate C;
					C.A = Id;
					C.B = Other;
					C.Direction = Direction;
					for (int32 J = 1; J <= Gap; ++J)
					{
						C.Cells.Add(P + Step * J);
					}
					Choices.Add(C);
				}
				if (!Choices.IsEmpty())
				{
					const int32 First = Random.RandRange(0, Choices.Num() - 1);
					Candidates.Add(Choices[First]);
					for (const auto& Choice : Choices)
					{
						if (FMath::Abs(Choice.Cells[0].X - Choices[First].Cells[0].X) +
								FMath::Abs(Choice.Cells[0].Y - Choices[First].Cells[0].Y) >=
							Width + 2)
						{
							Candidates.Add(Choice);
							break;
						}
					}
				}
			}
		}
		// 候補グラフ自体に複数の周回と選択肢がなければ、この向きは採用しない。
		TSet<int32> Seen{0};
		TArray<int32> Queue{0};
		TArray<int32> Degree;
		Degree.Init(0, Count);
		for (const auto& C : Candidates)
		{
			++Degree[C.A];
			++Degree[C.B];
		}
		for (int32 I = 0; I < Queue.Num(); ++I)
		{
			for (const auto& C : Candidates)
			{
				const int32 Next = C.A == Queue[I] ? C.B : C.B == Queue[I] ? C.A : INDEX_NONE;
				if (Next != INDEX_NONE && !Seen.Contains(Next))
				{
					Seen.Add(Next);
					Queue.Add(Next);
				}
			}
		}
		bool AllTwo = true;
		int32 Branches = 0;
		for (int32 D : Degree)
		{
			AllTwo &= D >= 2;
			Branches += D >= 3;
		}
		if (Seen.Num() == Count && AllTwo && Branches >= 4 && Candidates.Num() - Count + 1 >= 3)
		{
			break;
		}
		if (Attempt == 32)
		{
			return Fail(Error, TEXT("Cannot construct a branching maze"));
		}
	}
	for (int32 I = Candidates.Num() - 1; I > 0; --I)
	{
		Candidates.Swap(I, Random.RandRange(0, I));
	}
	TArray<int32> Parent, Degree;
	Degree.Init(0, Count);
	for (int32 I = 0; I < Count; ++I)
	{
		Parent.Add(I);
	}
	const auto Root = [&](int32 V)
	{
		while (Parent[V] != V)
		{
			V = Parent[V];
		}
		return V;
	};
	TArray<int32> Selected;
	const auto Select = [&](int32 I)
	{
		Selected.Add(I);
		++Degree[Candidates[I].A];
		++Degree[Candidates[I].B];
	};
	// 全域木で孤立をなくし、その後で戻り道を足す。単なる一筆書きには戻さない。
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		const auto& C = Candidates[I];
		const int32 A = Root(C.A), B = Root(C.B);
		if (A != B)
		{
			Parent[A] = B;
			Select(I);
		}
	}
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		if (!Selected.Contains(I) && Random.FRand() < FMath::Clamp(S.ExtraConnectionChance, 0.f, 1.f))
		{
			Select(I);
		}
	}
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		if (!Selected.Contains(I) && (Degree[Candidates[I].A] < 2 || Degree[Candidates[I].B] < 2))
		{
			Select(I);
		}
	}
	const auto BranchCount = [&]()
	{
		int32 N = 0;
		for (int32 D : Degree)
		{
			N += D >= 3;
		}
		return N;
	};
	for (int32 I = 0; I < Candidates.Num() && (Selected.Num() - Count + 1 < 3 || BranchCount() < 4); ++I)
	{
		if (!Selected.Contains(I))
		{
			Select(I);
		}
	}
	// 候補がある自由端は必ず別の空間へつなぐ。全域木だけでは腕の袋小路は防げない。
	for (const auto& N : Plan.Nodes)
	{
		for (int32 End : {0, N.Sections.Num() - 1})
		{
			const auto& R = N.Sections[End];
			const auto& Join = N.Sections[End == 0 ? 1 : End - 1];
			const int32 Axis = SectionAxis(R), WidthR = Along(R.Size, 1 - Axis), Length = Along(R.Size, Axis);
			const bool JoinedAtStart = Along(Join.Min, Axis) <= Along(R.Min, Axis);
			const auto Covers = [&](int32 Index)
			{
				const auto& C = Candidates[Index];
				const FIntPoint P = C.A == N.Id	  ? C.Cells[0] - Steps[C.Direction]
									: C.B == N.Id ? C.Cells.Last() + Steps[C.Direction]
												  : FIntPoint(MAX_int32, MAX_int32);
				if (!InRect(P, R))
				{
					return false;
				}
				const int32 T = Along(P - R.Min, Axis);
				return JoinedAtStart ? T >= Length - WidthR : T < WidthR;
			};
			bool Covered = false;
			for (int32 I : Selected)
			{
				Covered |= Covers(I);
			}
			if (!Covered)
			{
				for (int32 I = 0; I < Candidates.Num(); ++I)
				{
					if (Covers(I))
					{
						Select(I);
						break;
					}
				}
			}
		}
	}
	for (int32 I : Selected)
	{
		const auto& C = Candidates[I];
		FCourtyardLink L;
		L.From = C.A;
		L.To = C.B;
		L.Direction = C.Direction;
		L.Axis = C.Direction % 2;
		L.Cells = C.Cells;
		Plan.Links.Add(L);
	}
	// 接続先が存在しない外周の長い腕は、短い出っ張りまで戻す。
	for (auto& N : Plan.Nodes)
	{
		TArray<FIntPoint> Ports;
		for (const auto& L : Plan.Links)
		{
			if (L.From == N.Id)
			{
				Ports.Add(L.Cells[0] - Steps[L.Direction]);
			}
			if (L.To == N.Id)
			{
				Ports.Add(L.Cells.Last() + Steps[L.Direction]);
			}
		}
		FixBlindEnds(N, Ports, true);
		N.FloorCells.Reset();
		FIntPoint Min(MAX_int32, MAX_int32), Max(MIN_int32, MIN_int32);
		for (const auto& R : N.Sections)
		{
			for (int32 X = 0; X < R.Size.X; ++X)
			{
				for (int32 Y = 0; Y < R.Size.Y; ++Y)
				{
					auto P = R.Min + FIntPoint(X, Y);
					N.FloorCells.AddUnique(P);
					Min.X = FMath::Min(Min.X, P.X);
					Min.Y = FMath::Min(Min.Y, P.Y);
					Max.X = FMath::Max(Max.X, P.X);
					Max.Y = FMath::Max(Max.Y, P.Y);
				}
			}
		}
		N.Min = Min;
		N.Size = Max - Min + FIntPoint(1, 1);
	}
	TMap<FIntPoint, FCourtyardCell> Cells;
	for (const auto& N : Plan.Nodes)
	{
		for (const auto P : N.FloorCells)
		{
			FCourtyardCell C;
			C.Position = P;
			C.Kind = N.Kind;
			C.NodeId = N.Id;
			int32 Members = 0;
			for (int32 I = 0; I < N.Sections.Num(); ++I)
			{
				if (InRect(P, N.Sections[I]))
				{
					++Members;
					C.SectionIndex = I;
				}
			}
			if (Members > 1)
			{
				C.SectionIndex = INDEX_NONE;
			}
			Cells.Add(P, C);
		}
	}
	for (int32 Id = 0; Id < Plan.Links.Num(); ++Id)
	{
		for (const auto P : Plan.Links[Id].Cells)
		{
			if (Cells.Contains(P))
			{
				return Fail(Error, TEXT("Maze tunnel overlaps a floor"));
			}
			FCourtyardCell C;
			C.Position = P;
			C.Kind = ECourtyardCellKind::Tunnel;
			C.LinkId = Id;
			C.Axis = Plan.Links[Id].Axis;
			Cells.Add(P, C);
		}
	}
	// 廊下全体を部屋にせず、角の実床に収まる2×3セルの独立した屋内空間を作る。
	for (int32 Id : RoomIds)
	{
		if (Plan.RoomAreas.Num() >= RoomCount)
		{
			break;
		}
		const auto& N = Plan.Nodes[Id];
		bool Added = false;
		for (int32 Part = 1; Part < N.Sections.Num() && !Added; ++Part)
		{
			const auto& A = N.Sections[Part - 1];
			const auto& B = N.Sections[Part];
			const FIntPoint Corner(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y));
			for (const FIntPoint Size : {FIntPoint(2, 3), FIntPoint(3, 2)})
			{
				for (int32 DX = -1; DX <= 0 && !Added; ++DX)
				{
					for (int32 DY = -1; DY <= 0 && !Added; ++DY)
					{
						FCourtyardRoomArea Area;
						Area.Min = Corner + FIntPoint(DX, DY);
						Area.Size = Size;
						bool Fits = true;
						for (int32 X = 0; X < Size.X; ++X)
						{
							for (int32 Y = 0; Y < Size.Y; ++Y)
							{
								const auto* C = Cells.Find(Area.Min + FIntPoint(X, Y));
								Fits &= C && C->NodeId == Id && C->Kind == ECourtyardCellKind::Street;
							}
						}
						if (!Fits)
						{
							continue;
						}
						FCourtyardRoomSection Bounds;
						Bounds.Min = Area.Min;
						Bounds.Size = Area.Size;
						for (int32 D = 0; D < 4; ++D)
						{
							TArray<FIntPoint> Doors;
							for (int32 X = 0; X < Size.X; ++X)
							{
								for (int32 Y = 0; Y < Size.Y; ++Y)
								{
									const auto P = Area.Min + FIntPoint(X, Y), Q = P + Steps[D];
									if (!InRect(Q, Bounds) && Cells.Contains(Q))
									{
										Doors.Add(P);
									}
								}
							}
							if (!Doors.IsEmpty())
							{
								Area.DoorCells.Add(Doors[Doors.Num() / 2]);
								Area.DoorDirections.Add(D);
							}
						}
						if (Area.DoorCells.Num() < 2)
						{
							continue;
						}
						for (int32 X = 0; X < Size.X; ++X)
						{
							for (int32 Y = 0; Y < Size.Y; ++Y)
							{
								Cells.FindChecked(Area.Min + FIntPoint(X, Y)).Kind = ECourtyardCellKind::Room;
							}
						}
						Plan.RoomAreas.Add(Area);
						Added = true;
					}
				}
				if (Added)
				{
					break;
				}
			}
		}
	}
	if (Plan.RoomAreas.Num() != RoomCount)
	{
		return Fail(Error, TEXT("Could not place compact through rooms"));
	}
	Cells.GenerateValueArray(Plan.Cells);
	Plan.Cells.Sort(
		[](const auto& A, const auto& B)
		{ return A.Position.X != B.Position.X ? A.Position.X < B.Position.X : A.Position.Y < B.Position.Y; });
	Plan.StartCell = Plan.Nodes[0].FloorCells[0];
	Plan.GoalCell = Plan.StartCell;
	TMap<FIntPoint, int32> Distance;
	Distance.Add(Plan.StartCell, 0);
	TArray<FIntPoint> Queue{Plan.StartCell};
	int32 Best = 0;
	for (int32 I = 0; I < Queue.Num(); ++I)
	{
		for (const auto D : Steps)
		{
			const auto Next = Queue[I] + D;
			const auto* C = Cells.Find(Next);
			if (!C || Distance.Contains(Next))
			{
				continue;
			}
			const int32 Dist = Distance.FindChecked(Queue[I]) + 1;
			Distance.Add(Next, Dist);
			Queue.Add(Next);
			if (C->Kind == ECourtyardCellKind::Street && Dist > Best)
			{
				Best = Dist;
				Plan.GoalCell = Next;
			}
		}
	}
	Plan.LoopCount = Plan.Links.Num() - Plan.Nodes.Num() + 1;
	return ValidateCourtyardPlan(Plan, Error);
}

bool UCourtyardLayoutLibrary::ValidateCourtyardPlan(const FCourtyardPlan& Plan, FString& Error)
{
	Error.Reset();
	if (Plan.Nodes.Num() < 4 || Plan.Nodes.Num() > 36 || Plan.Cells.IsEmpty() || Plan.Cells.Num() > 10000 ||
		Plan.TunnelCellLimit < 1 || Plan.TunnelCellLimit > 3)
	{
		return Fail(Error, TEXT("Invalid courtyard plan size"));
	}
	TMap<FIntPoint, const FCourtyardCell*> Cells;
	for (const auto& C : Plan.Cells)
	{
		if (Cells.Contains(C.Position))
		{
			return Fail(Error, TEXT("Duplicate floor"));
		}
		Cells.Add(C.Position, &C);
	}
	if (!Cells.Contains(Plan.StartCell) || !Cells.Contains(Plan.GoalCell))
	{
		return Fail(Error, TEXT("Invalid start/goal"));
	}
	TSet<FIntPoint> Indoor;
	for (const auto& R : Plan.RoomAreas)
	{
		if (!((R.Size == FIntPoint(2, 3)) || (R.Size == FIntPoint(3, 2))) || R.DoorCells.Num() < 2 ||
			R.DoorCells.Num() != R.DoorDirections.Num())
		{
			return Fail(Error, TEXT("Invalid compact room"));
		}
		FCourtyardRoomSection Bounds;
		Bounds.Min = R.Min;
		Bounds.Size = R.Size;
		for (int32 X = 0; X < R.Size.X; ++X)
		{
			for (int32 Y = 0; Y < R.Size.Y; ++Y)
			{
				auto P = R.Min + FIntPoint(X, Y);
				const auto* C = Cells.FindRef(P);
				if (!C || C->Kind != ECourtyardCellKind::Room || Indoor.Contains(P))
				{
					return Fail(Error, TEXT("Invalid indoor floor"));
				}
				Indoor.Add(P);
			}
		}
		for (int32 I = 0; I < R.DoorCells.Num(); ++I)
		{
			const int32 D = R.DoorDirections[I];
			const auto P = R.DoorCells[I];
			if (D < 0 || D > 3 || !InRect(P, Bounds) || InRect(P + Steps[D], Bounds) || !Cells.Contains(P + Steps[D]))
			{
				return Fail(Error, TEXT("Room door must open onto another path"));
			}
		}
	}
	TArray<TArray<FIntPoint>> Ports;
	Ports.SetNum(Plan.Nodes.Num());
	for (int32 Id = 0; Id < Plan.Links.Num(); ++Id)
	{
		const auto& L = Plan.Links[Id];
		if (!Plan.Nodes.IsValidIndex(L.From) || !Plan.Nodes.IsValidIndex(L.To) || L.Cells.IsEmpty() ||
			L.Cells.Num() > Plan.TunnelCellLimit || L.Direction < 0 || L.Direction > 3 || L.Axis != L.Direction % 2)
		{
			return Fail(Error, TEXT("Invalid straight tunnel"));
		}
		const auto Step = Steps[L.Direction], A = L.Cells[0] - Step, B = L.Cells.Last() + Step;
		if (!Contains(Plan.Nodes[L.From], A) || !Contains(Plan.Nodes[L.To], B))
		{
			return Fail(Error, TEXT("Tunnel endpoint misses the end of a space"));
		}
		Ports[L.From].Add(A);
		Ports[L.To].Add(B);
		for (int32 I = 0; I < L.Cells.Num(); ++I)
		{
			const auto* C = Cells.FindRef(L.Cells[I]);
			if (!C || C->Kind != ECourtyardCellKind::Tunnel || C->LinkId != Id || C->Axis != L.Axis ||
				(I > 0 && L.Cells[I] != L.Cells[I - 1] + Step))
			{
				return Fail(Error, TEXT("Tunnel must remain straight"));
			}
		}
	}
	for (int32 Id = 0; Id < Plan.Nodes.Num(); ++Id)
	{
		const auto& N = Plan.Nodes[Id];
		if (N.Id != Id || N.FloorCells.IsEmpty() || Ports[Id].Num() < 2)
		{
			return Fail(Error, TEXT("A maze space needs at least two connections"));
		}
		TArray<TArray<int32>> Adj;
		if (!BuildSectionChain(N.Sections, Adj, Error))
		{
			return false;
		}
		for (int32 I = 0; I < Adj.Num(); ++I)
		{
			for (int32 J : Adj[I])
			{
				if (FMath::Abs(I - J) != 1)
				{
					return Fail(Error, TEXT("Section order is not a simple chain"));
				}
			}
		}
		for (const auto P : Ports[Id])
		{
			if (!AtSectionEnd(N, P))
			{
				return Fail(Error, TEXT("Tunnel must attach near a section end corner"));
			}
		}
		auto Copy = N;
		if (!FixBlindEnds(Copy, Ports[Id], false))
		{
			return Fail(Error, TEXT("Long unconnected blind arm"));
		}
		TSet<FIntPoint> Owned;
		for (auto P : N.FloorCells)
		{
			const auto* C = Cells.FindRef(P);
			if (!C || C->NodeId != Id || C->Kind != (Indoor.Contains(P) ? ECourtyardCellKind::Room : N.Kind) ||
				Owned.Contains(P))
			{
				return Fail(Error, TEXT("Invalid node ownership"));
			}
			Owned.Add(P);
			int32 Membership = 0, Part = INDEX_NONE;
			for (int32 I = 0; I < N.Sections.Num(); ++I)
			{
				if (InRect(P, N.Sections[I]))
				{
					++Membership;
					Part = I;
				}
			}
			if (Membership < 1 || Membership > 2 || C->SectionIndex != (Membership == 1 ? Part : INDEX_NONE))
			{
				return Fail(Error, TEXT("Invalid corner footprint"));
			}
		}
		for (const auto& R : N.Sections)
		{
			for (int32 X = 0; X < R.Size.X; ++X)
			{
				for (int32 Y = 0; Y < R.Size.Y; ++Y)
				{
					if (!Owned.Contains(R.Min + FIntPoint(X, Y)))
					{
						return Fail(Error, TEXT("Missing section floor"));
					}
				}
			}
		}
	}
	for (const auto& C : Plan.Cells)
	{
		if (C.Kind == ECourtyardCellKind::Tunnel)
		{
			if (!Plan.Links.IsValidIndex(C.LinkId) || !Plan.Links[C.LinkId].Cells.Contains(C.Position))
			{
				return Fail(Error, TEXT("Orphan tunnel floor"));
			}
			continue;
		}
		if (!Plan.Nodes.IsValidIndex(C.NodeId) || !Contains(Plan.Nodes[C.NodeId], C.Position))
		{
			return Fail(Error, TEXT("Orphan space floor"));
		}
		for (auto D : Steps)
		{
			const auto* Other = Cells.FindRef(C.Position + D);
			if (Other && Other->Kind != ECourtyardCellKind::Tunnel && Other->NodeId != C.NodeId)
			{
				return Fail(Error, TEXT("Spaces touch outside their endpoint tunnels"));
			}
		}
	}
	TSet<FIntPoint> Seen{Plan.StartCell};
	TArray<FIntPoint> Queue{Plan.StartCell};
	for (int32 I = 0; I < Queue.Num(); ++I)
	{
		for (auto D : Steps)
		{
			const auto Next = Queue[I] + D;
			if (Cells.Contains(Next) && !Seen.Contains(Next))
			{
				Seen.Add(Next);
				Queue.Add(Next);
			}
		}
	}
	if (Seen.Num() != Cells.Num())
	{
		return Fail(Error, TEXT("Unreachable floor"));
	}
	int32 Branches = 0;
	for (const auto& P : Ports)
	{
		Branches += P.Num() >= 3;
	}
	if (Plan.LoopCount != Plan.Links.Num() - Plan.Nodes.Num() + 1 || Plan.LoopCount < 3 || Branches < 4)
	{
		return Fail(Error, TEXT("Maze needs at least three independent loops and four decision spaces"));
	}
	return true;
}
