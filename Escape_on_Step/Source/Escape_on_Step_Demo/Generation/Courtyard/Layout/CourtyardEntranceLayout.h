#pragma once

#include "CoreMinimal.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"

/** 入口として開けるセル列と、その外向き方向・境界中央（迷路ローカルcm）。 */
// 開口セル列、向き、位置、内側かどうかをまとめたC++用の入口データ型を宣言する。
struct FCourtyardEntranceSelection
{
    // 壁を開ける連続セル。すべて屋外床で、外側に床がないことを検査済み。
    // この入口で壁を開ける、連続したセル座標を順番に格納する。
    TArray<FIntPoint> Cells;

    // 0=東、1=北、2=西、3=南。通路から迷路へ進む向きは、この逆方向。
    // 外向き方向を0=東・1=北・2=西・3=南で保持する。既定値は西向き。
    int32 OutwardDirection = 2;

    // 生成Actorのローカル座標、cm単位。セル番号そのものではない。
    // 開口の境界中央を、生成Actorのローカルcm座標で保持する。未計算時は原点。
    FVector BoundaryCenter = FVector::ZeroVector;

    // 全床の外接範囲の内側にある入口か。内側のくぼみも候補に含める。
    // 全床の外接範囲の内側にある入口ならtrue。未判定の既定値はfalse。
    bool bInterior = false;
};

namespace CourtyardEntranceLayout
{
    /** 寸法を満たす入口を列挙する。抽選しないので、候補配列をBPへ返しても乱数順を変えない。 */
    bool FindCandidates(const TArray<FCourtyardCell>& FloorCells,
        // 1セルの実寸（cm）（CellSize）を単精度の実数として引数で受け取る。
        float CellSize,
        // 幅（Width）を単精度の実数として引数で受け取る。
        float Width,
        // ApproachDepth（ApproachDepth）を単精度の実数として引数で受け取る。
        float ApproachDepth,
        // 壁を含む導入通路の外幅（cm）（ApproachWidth）を単精度の実数として引数で受け取る。
        float ApproachWidth,
        // 接続や入口の候補（Candidates）を配列として引数で受け取る。元の値を参照し、コピーしない。
        TArray<FCourtyardEntranceSelection>& Candidates,
        // 失敗理由を文字列で返す出力引数。成功した場合は空文字列になる。
        FString& Error);

    /**
     * 四方向の外周・くぼみから、通路幅と導入エリアの奥行きを確保できる入口を抽選する。
     * 部屋や細道の形は変更しない。選択後は入口からの歩行距離でゴールを再計算する。
     */
    bool SelectEntrance(FCourtyardPlan& Plan,
        // 1セルの実寸（cm）（CellSize）を単精度の実数として引数で受け取る。
        float CellSize,
        // 幅（Width）を単精度の実数として引数で受け取る。
        float Width,
        // ApproachDepth（ApproachDepth）を単精度の実数として引数で受け取る。
        float ApproachDepth,
        // 再現用Seed（Seed）を符号付き32ビット整数として引数で受け取る。
        int32 Seed,
        // Selection（Selection）を入口候補の情報として引数で受け取る。元の値を参照し、コピーしない。
        FCourtyardEntranceSelection& Selection,
        // 失敗理由（Error）を文字列として引数で受け取る。元の値を参照し、コピーしない。
        FString& Error,
        // 導入通路の外幅を受け取る。省略時の0では、内幅を必要空間の基準に使う。
        float ApproachWidth = 0);
}
