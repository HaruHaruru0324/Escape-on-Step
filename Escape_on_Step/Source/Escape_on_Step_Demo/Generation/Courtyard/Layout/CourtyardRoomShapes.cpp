#include "Generation/Courtyard/Layout/CourtyardRoomShapes.h"

namespace
{
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    bool Connected(const TSet<FIntPoint>& Floor)
    {
        if (Floor.IsEmpty())
        {
            return false;
        }
        TArray<FIntPoint> Queue{*Floor.CreateConstIterator()};
        TSet<FIntPoint> Seen{Queue[0]};
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            for (const auto Step : Steps)
            {
                const FIntPoint Next = Queue[Head] + Step;
                if (Floor.Contains(Next) && !Seen.Contains(Next))
                {
                    Seen.Add(Next);
                    Queue.Add(Next);
                }
            }
        }
        return Seen.Num() == Floor.Num();
    }
}

TArray<FIntPoint> UCourtyardRoomShapes::MakeHybridRoomFloor(
    const TArray<FIntPoint>& OriginalFloor, int32 LayoutSeed, int32 RoomId, int32 NarrowingLength)
{
    TSet<FIntPoint> Floor(OriginalFloor);
    struct FStrip
    {
        TArray<FIntPoint> Cells;
    };
    TArray<FStrip> Strips;

    // 外側の1列だけを削れる直線部分を探す。両端と曲がり角は幅2セルのまま残す。
    for (int32 Direction = 0; Direction < 4; ++Direction)
    {
        const auto Inward = Steps[Direction];
        const FIntPoint Along(-Inward.Y, Inward.X);
        const auto CanNarrow = [&](FIntPoint P)
        {
            return Floor.Contains(P) && !Floor.Contains(P - Inward) &&
                   Floor.Contains(P + Inward) && !Floor.Contains(P + Inward * 2) &&
                   Floor.Contains(P - Along) && Floor.Contains(P + Along) &&
                   Floor.Contains(P + Inward - Along) && Floor.Contains(P + Inward + Along);
        };
        for (const auto P : OriginalFloor)
        {
            if (!CanNarrow(P) || CanNarrow(P - Along))
            {
                continue;
            }
            FStrip Strip;
            for (FIntPoint Q = P; CanNarrow(Q); Q += Along)
            {
                Strip.Cells.Add(Q);
            }
            Strips.Add(MoveTemp(Strip));
        }
    }
    if (Strips.IsEmpty() || NarrowingLength <= 0)
    {
        return OriginalFloor;
    }

    FRandomStream Random(int32(HashCombineFast(GetTypeHash(LayoutSeed), GetTypeHash(RoomId))));
    const FStrip& Strip = Strips[Random.RandRange(0, Strips.Num() - 1)];
    const int32 Count = FMath::Min(FMath::Clamp(NarrowingLength, 1, 3), Strip.Cells.Num());
    const int32 Start = Random.RandRange(0, Strip.Cells.Num() - Count);
    for (int32 Index = 0; Index < Count; ++Index)
    {
        Floor.Remove(Strip.Cells[Start + Index]);
    }
    // 将来別の形状を入力しても、床を切断する削り方は採用しない。
    if (!Connected(Floor))
    {
        return OriginalFloor;
    }
    TArray<FIntPoint> Result;
    for (const auto P : OriginalFloor)
    {
        if (Floor.Contains(P))
        {
            Result.Add(P);
        }
    }
    return Result;
}

int32 UCourtyardRoomShapes::FillEnclosedRoomHoles(FCourtyardPlan& Plan)
{
    TSet<FIntPoint> Floor;
    for (const auto& Cell : Plan.Cells)
    {
        Floor.Add(Cell.Position);
    }
    int32 Filled = 0;
    for (auto& Node : Plan.Nodes)
    {
        const FIntPoint Min = Node.Min - FIntPoint(1, 1);
        const FIntPoint Max = Node.Min + Node.Size;
        if (int64(Node.Size.X + 2) * (Node.Size.Y + 2) > 262144)
        {
            continue;
        }
        TArray<FIntPoint> Queue;
        TSet<FIntPoint> Outside;
        // 「部屋の外へ開く」と「マップ全体の外へ開く」は別。
        // 部屋の周囲1セルへ達するくぼみは、遠くの周回路に囲まれていても埋めない。
        for (int32 X = Min.X; X <= Max.X; ++X)
        {
            for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
            {
                const FIntPoint P(X, Y);
                if ((X == Min.X || X == Max.X || Y == Min.Y || Y == Max.Y) && !Floor.Contains(P))
                {
                    Outside.Add(P);
                    Queue.Add(P);
                }
            }
        }
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            for (const auto Step : Steps)
            {
                const auto Next = Queue[Head] + Step;
                if (Next.X >= Min.X && Next.X <= Max.X && Next.Y >= Min.Y && Next.Y <= Max.Y &&
                    !Floor.Contains(Next) && !Outside.Contains(Next))
                {
                    Outside.Add(Next);
                    Queue.Add(Next);
                }
            }
        }
        for (int32 X = Node.Min.X; X < Node.Min.X + Node.Size.X; ++X)
        {
            for (int32 Y = Node.Min.Y; Y < Node.Min.Y + Node.Size.Y; ++Y)
            {
                const FIntPoint P(X, Y);
                if (!Floor.Contains(P) && !Outside.Contains(P))
                {
                    Node.FloorCells.Add(P);
                    FCourtyardCell Cell;
                    Cell.Position = P;
                    Cell.Kind = Node.Kind;
                    Cell.NodeId = Node.Id;
                    Plan.Cells.Add(Cell);
                    Floor.Add(P);
                    ++Filled;
                }
            }
        }
    }
    if (!Filled)
    {
        return 0;
    }

    // 穴を埋めた結果、既存トンネルの側面へ床が接したら、その経路は屋外通路へ戻す。
    // 横へ道があるのに直線トンネルの側壁で塞ぐ事故を防ぐ。
    for (auto& Link : Plan.Links)
    {
        if (Link.bAlley || Link.Direction < 0 || Link.Direction >= 4)
        {
            continue;
        }
        const auto Step = Steps[Link.Direction];
        const FIntPoint Side(-Step.Y, Step.X);
        bool SideOpened = false;
        for (const auto P : Link.Cells)
        {
            SideOpened |= Floor.Contains(P + Side) || Floor.Contains(P - Side);
        }
        if (SideOpened)
        {
            Link.bAlley = true;
        }
    }
    for (auto& Cell : Plan.Cells)
    {
        if (Cell.Kind == ECourtyardCellKind::Tunnel && Plan.Links.IsValidIndex(Cell.LinkId) &&
            Plan.Links[Cell.LinkId].bAlley)
        {
            Cell.Kind = ECourtyardCellKind::Street;
            Cell.LinkId = INDEX_NONE;
        }
    }
    return Filled;
}

int32 UCourtyardRoomShapes::RemovePlainRectangularRooms(FCourtyardPlan& Plan, const TArray<FIntPoint>& ProtectedCells)
{
    TSet<FIntPoint> All;
    TMap<FIntPoint, int32> Owners;
    for (const auto& Cell : Plan.Cells)
    {
        All.Add(Cell.Position);
        Owners.Add(Cell.Position, Cell.NodeId);
    }
    int32 Changed = 0;
    for (auto& Node : Plan.Nodes)
    {
        TSet<FIntPoint> Floor(Node.FloorCells);
        const auto BroadCount = [](const TSet<FIntPoint>& Cells)
        {
            int32 Count = 0;
            for (const auto P : Cells)
            {
                bool Full = true;
                for (int32 X = 0; X < 4 && Full; ++X)
                {
                    for (int32 Y = 0; Y < 4; ++Y)
                    {
                        Full &= Cells.Contains(P + FIntPoint(X, Y));
                    }
                }
                Count += Full;
            }
            return Count;
        };
        const FIntPoint Min = Node.Min - FIntPoint(1, 1);
        const FIntPoint Max = Node.Min + Node.Size;
        TSet<FIntPoint> Outside;
        TArray<FIntPoint> Open;
        for (int32 X = Min.X; X <= Max.X; ++X)
        {
            for (int32 Y = Min.Y; Y <= Max.Y; ++Y)
            {
                const FIntPoint P(X, Y);
                if ((X == Min.X || X == Max.X || Y == Min.Y || Y == Max.Y) && !All.Contains(P))
                {
                    Outside.Add(P);
                    Open.Add(P);
                }
            }
        }
        const auto ExpandOutside = [&]()
        {
            for (int32 Head = 0; Head < Open.Num(); ++Head)
            {
                for (const auto Step : Steps)
                {
                    const auto Next = Open[Head] + Step;
                    if (Next.X >= Min.X && Next.X <= Max.X && Next.Y >= Min.Y && Next.Y <= Max.Y &&
                        !All.Contains(Next) && !Outside.Contains(Next))
                    {
                        Outside.Add(Next);
                        Open.Add(Next);
                    }
                }
            }
        };
        if (BroadCount(Floor) > 0)
        {
            ExpandOutside();
        }
        bool NodeChanged = false;
        // 穴埋めでできた四角い広場には、外へ開いた幅2セルの切り欠きを入れる。
        // 床のない島を部屋の中に作るのではなく、壁の輪郭そのものをL/U字へ戻す。
        for (int32 Pass = 0; Pass < 2 && BroadCount(Floor) > 0; ++Pass)
        {
            TSet<FIntPoint> Protected{Plan.StartCell, Plan.GoalCell};
            Protected.Append(ProtectedCells);
            for (const auto P : Floor)
            {
                for (const auto Step : Steps)
                {
                    if (const auto Owner = Owners.Find(P + Step); Owner && *Owner != Node.Id)
                    {
                        Protected.Add(P);
                    }
                }
            }
            TArray<FIntPoint> Best;
            int32 BestBroad = BroadCount(Floor);
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                const FIntPoint Inward = Steps[Direction];
                const bool AlongX = Inward.X != 0;
                const int32 Length = AlongX ? Node.Size.X : Node.Size.Y;
                const int32 SideLength = AlongX ? Node.Size.Y : Node.Size.X;
                for (int32 Offset = 1; Offset + 2 < SideLength; ++Offset)
                {
                    for (int32 Depth = 2; Depth <= FMath::Min(4, Length - 2); ++Depth)
                    {
                        TArray<FIntPoint> Remove;
                        bool Safe = true;
                        for (int32 Side = 0; Side < 2 && Safe; ++Side)
                        {
                            FIntPoint Edge = Node.Min;
                            if (AlongX)
                            {
                                Edge.X += Inward.X > 0 ? 0 : Node.Size.X - 1;
                                Edge.Y += Offset + Side;
                            }
                            else
                            {
                                Edge.Y += Inward.Y > 0 ? 0 : Node.Size.Y - 1;
                                Edge.X += Offset + Side;
                            }
                            // 外側に別の床があれば、空白を作っても穴になるため採用しない。
                            if (!Outside.Contains(Edge - Inward))
                            {
                                Safe = false;
                                break;
                            }
                            for (int32 D = 0; D < Depth; ++D)
                            {
                                const auto P = Edge + Inward * D;
                                if (Protected.Contains(P) || (All.Contains(P) && !Floor.Contains(P)))
                                {
                                    Safe = false;
                                    break;
                                }
                                if (Floor.Contains(P))
                                {
                                    Remove.Add(P);
                                }
                            }
                        }
                        if (!Safe || Remove.IsEmpty())
                        {
                            continue;
                        }
                        TSet<FIntPoint> Candidate = Floor;
                        for (const auto P : Remove)
                        {
                            Candidate.Remove(P);
                        }
                        if (!Connected(Candidate))
                        {
                            continue;
                        }
                        const int32 Broad = BroadCount(Candidate);
                        if (Broad < BestBroad || (Broad == BestBroad && !Best.IsEmpty() && Remove.Num() > Best.Num()))
                        {
                            BestBroad = Broad;
                            Best = MoveTemp(Remove);
                        }
                    }
                }
            }
            if (Best.IsEmpty())
            {
                break;
            }
            TSet<FIntPoint> Removed(Best);
            for (const auto P : Best)
            {
                Floor.Remove(P);
                All.Remove(P);
                Owners.Remove(P);
                Outside.Add(P);
                Open.Add(P);
            }
            Node.FloorCells.RemoveAll([&](FIntPoint P) { return Removed.Contains(P); });
            Plan.Cells.RemoveAll([&](const FCourtyardCell& Cell) { return Removed.Contains(Cell.Position); });
            ExpandOutside();
            NodeChanged = true;
        }
        Changed += NodeChanged;
    }
    return Changed;
}
