#pragma once

#include "CoreMinimal.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardRoomShapes.generated.h"

/** 部屋の床形状を補助する。配置・接続・壁タイプの判断は既存BPが担当する。 */
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UCourtyardRoomShapes : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 幅2セルの帯の一部だけを1セルに絞る。曲がり角と外接範囲を保ち、抽選用Streamは消費しない。 */
    UFUNCTION(BlueprintPure, Category = "Courtyard BP|Room Shape")
    static TArray<FIntPoint> MakeHybridRoomFloor(const TArray<FIntPoint>& OriginalFloor,
        int32 LayoutSeed, int32 RoomId, int32 NarrowingLength = 2);

    /** 外側から空白を探索し、部屋の外接範囲内にある閉じた穴だけを床へ変える。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard BP|Room Shape")
    static int32 FillEnclosedRoomHoles(UPARAM(ref) FCourtyardPlan& Plan);
    /** 大きな四角い床には、入口を壊さない外向きの切り欠きを作る。 */
    static int32 RemovePlainRectangularRooms(FCourtyardPlan& Plan,
        const TArray<FIntPoint>& ProtectedCells = {});
};
