#pragma once

#include "CoreMinimal.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"

/** 床形状を変更せず、トンネルで区切られた空間ごとに壁選択の代表部屋IDを求める。 */
struct FCourtyardWallRegions
{
    TMap<FIntPoint, int32> StyleOwnerByCell;
    TMap<FIntPoint, FCourtyardLink> AddedTunnels;
    int32 RegionCount = 0;
};

namespace CourtyardWallRegions
{
    /** 曲がり角・分岐・部屋内部を避け、細道の直線1セルを短い仕切りトンネルにする。 */
    FCourtyardWallRegions Build(const FCourtyardPlan& Plan, const TSet<FIntPoint>& ProtectedCells = {});
}
