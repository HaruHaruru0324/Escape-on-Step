#include "Generation/Courtyard/Layout/CourtyardWallRegions.h"

namespace
{
    const FIntPoint WallRegionSteps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

FCourtyardWallRegions CourtyardWallRegions::Build(const FCourtyardPlan& Plan, const TSet<FIntPoint>& ProtectedCells)
{
    FCourtyardWallRegions Result;
    TMap<FIntPoint, const FCourtyardCell*> Floors;
    for (const auto& Cell : Plan.Cells)
    {
        Floors.Add(Cell.Position, &Cell);
    }

    // 既存の経路1本につき、中央に近い安全な直線部分を最大1か所選ぶ。
    // 壁だけで段差を隠すのではなく、実際のトンネルモデルと両端の開口を配置する。
    for (const auto& Route : Plan.Links)
    {
        if (!Route.bAlley || Route.Cells.IsEmpty())
        {
            continue;
        }
        int32 BestIndex = INDEX_NONE;
        int32 BestDirection = 0;
        int32 BestDistance = MAX_int32;
        for (int32 Index = 0; Index < Route.Cells.Num(); ++Index)
        {
            const FIntPoint P = Route.Cells[Index];
            const FCourtyardCell* const* Found = Floors.Find(P);
            if (!Found || (*Found)->Kind != ECourtyardCellKind::Street ||
                (*Found)->NodeId != INDEX_NONE || Result.AddedTunnels.Contains(P) || ProtectedCells.Contains(P))
            {
                continue;
            }

            int32 NeighborCount = 0;
            bool NearTunnel = false;
            for (const auto Step : WallRegionSteps)
            {
                if (const auto Neighbor = Floors.Find(P + Step))
                {
                    ++NeighborCount;
                    NearTunnel |= (*Neighbor)->Kind == ECourtyardCellKind::Tunnel ||
                                  (*Neighbor)->Kind == ECourtyardCellKind::Room ||
                                  Result.AddedTunnels.Contains(P + Step) || ProtectedCells.Contains(P + Step);
                }
            }
            // 分岐や曲がりに直線モデルを置くと、既存の道を塞いでしまう。
            if (NeighborCount != 2 || NearTunnel)
            {
                continue;
            }
            for (int32 Direction = 0; Direction < 2; ++Direction)
            {
                if (Floors.Contains(P - WallRegionSteps[Direction]) && Floors.Contains(P + WallRegionSteps[Direction]))
                {
                    const int32 Distance = FMath::Abs(2 * Index - (Route.Cells.Num() - 1));
                    if (Distance < BestDistance)
                    {
                        BestIndex = Index;
                        BestDistance = Distance;
                        BestDirection = Direction;
                    }
                }
            }
        }
        if (BestIndex != INDEX_NONE)
        {
            FCourtyardLink Tunnel;
            Tunnel.From = Route.From;
            Tunnel.To = Route.To;
            Tunnel.Cells.Add(Route.Cells[BestIndex]);
            Tunnel.bAlley = false;
            Tunnel.Direction = BestDirection;
            Tunnel.Axis = BestDirection;
            Result.AddedTunnels.Add(Tunnel.Cells[0], MoveTemp(Tunnel));
        }
    }

    // 「部屋IDが違う」ではなく「トンネルなしで歩いてつながる」を同じ空間の基準にする。
    // 部屋から細道へ進んでも、開けた空間の途中で壁の高さが切り替わらない。
    TMap<FIntPoint, int32> RegionByCell;
    TArray<TArray<FIntPoint>> Regions;
    TArray<int32> Owners;
    for (const auto& Cell : Plan.Cells)
    {
        if (Cell.Kind == ECourtyardCellKind::Tunnel ||
            Result.AddedTunnels.Contains(Cell.Position) || RegionByCell.Contains(Cell.Position))
        {
            continue;
        }
        const int32 RegionId = Regions.Num();
        TArray<FIntPoint> Queue{Cell.Position};
        RegionByCell.Add(Cell.Position, RegionId);
        int32 Owner = INDEX_NONE;
        for (int32 Head = 0; Head < Queue.Num(); ++Head)
        {
            const FIntPoint P = Queue[Head];
            const int32 NodeId = Floors.FindChecked(P)->NodeId;
            if (NodeId >= 0 && (Owner == INDEX_NONE || NodeId < Owner))
            {
                Owner = NodeId;
            }
            for (const auto Step : WallRegionSteps)
            {
                const FIntPoint Next = P + Step;
                const auto Neighbor = Floors.Find(Next);
                if (Neighbor && (*Neighbor)->Kind != ECourtyardCellKind::Tunnel &&
                    !Result.AddedTunnels.Contains(Next) && !RegionByCell.Contains(Next))
                {
                    RegionByCell.Add(Next, RegionId);
                    Queue.Add(Next);
                }
            }
        }
        Regions.Add(MoveTemp(Queue));
        Owners.Add(Owner);
    }

    // トンネルを挟んだ細道だけの空間にも、最寄りの部屋の有効なIDを割り当てる。
    // -1をBPのMapへ渡して、勝手にNormalへ戻る従来の経路をなくす。
    TMap<FIntPoint, int32> NearestOwner;
    TArray<FIntPoint> Queue;
    for (const auto& Cell : Plan.Cells)
    {
        const auto Region = RegionByCell.Find(Cell.Position);
        if (Region && Owners[*Region] != INDEX_NONE)
        {
            NearestOwner.Add(Cell.Position, Owners[*Region]);
            Queue.Add(Cell.Position);
        }
    }
    for (int32 Head = 0; Head < Queue.Num(); ++Head)
    {
        const FIntPoint P = Queue[Head];
        const int32 Owner = NearestOwner.FindChecked(P);
        for (const auto Step : WallRegionSteps)
        {
            const FIntPoint Next = P + Step;
            if (Floors.Contains(Next) && !NearestOwner.Contains(Next))
            {
                NearestOwner.Add(Next, Owner);
                Queue.Add(Next);
            }
        }
    }
    for (int32 RegionId = 0; RegionId < Regions.Num(); ++RegionId)
    {
        const auto& Region = Regions[RegionId];
        const int32* Nearest = NearestOwner.Find(Region[0]);
        const int32 Owner = Owners[RegionId] != INDEX_NONE ? Owners[RegionId] :
                            (Nearest ? *Nearest : INDEX_NONE);
        for (const auto P : Region)
        {
            Result.StyleOwnerByCell.Add(P, Owner);
        }
    }
    Result.RegionCount = Regions.Num();

    // 別ルートで同じ空間につながるだけの仕切りは不要なので、屋外の道へ戻す。
    for (auto It = Result.AddedTunnels.CreateIterator(); It; ++It)
    {
        const FIntPoint P = It.Key();
        const FIntPoint Step = WallRegionSteps[It.Value().Direction];
        const int32 A = RegionByCell.FindChecked(P - Step);
        const int32 B = RegionByCell.FindChecked(P + Step);
        if (A == B)
        {
            Result.StyleOwnerByCell.Add(P, Result.StyleOwnerByCell.FindChecked(P - Step));
            It.RemoveCurrent();
        }
    }
    return Result;
}
