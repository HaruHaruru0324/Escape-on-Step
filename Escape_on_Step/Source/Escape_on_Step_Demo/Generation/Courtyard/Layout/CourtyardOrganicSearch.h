#pragma once
#include "Generation/Courtyard/Layout/CourtyardLayout.h"

namespace CourtyardOrganic
{
    // 部屋の出入口候補を、室内セル・室外セル・方向の組として表す型を宣言する。
    struct FPort
    {
        // Insideに部屋内の床セル、Outsideにその外側へ1マス進んだセルを保持する。
        FIntPoint Inside, Outside;
        // 室内セルから室外セルへ向かう方向番号を、0=東・1=北・2=西・3=南で保持する。
        int32 Direction;
    };

    // 2室間の経路探索の結果を、接続元・接続先・セル列で表す型を宣言する。
    struct FRoute
    {
        // 経路の接続元と接続先の部屋IDを保持する。
        int32 From, To;
        // 開始側から到達側へ順番に並べた細道のセル列。部屋内部の床セルは含めない。
        TArray<FIntPoint> Cells;
    };

    // Roomsの形を保ち、Limitの盤面内へ位置を詰める探索を宣言する。Seedで探索順を再現する。
    void CompactRooms(TArray<FCourtyardNode>& Rooms, int32 Limit, int32 Seed);
    // この関数の役割：2室間をつなぐ細道の最小コスト経路を探す。ここでは呼び出し方を宣言する。
    bool FindRoomConnectionRoute(int32 A,
        // B（B）を符号付き32ビット整数として引数で受け取る。
        int32 B,
        // 部屋の一覧（Rooms）を配列として引数で受け取る。元の値を参照し、コピーしない。
        const TArray<FCourtyardNode>& Rooms,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // 細道セルの集合（Roads）を重複しない集合として引数で受け取る。元の値を参照し、コピーしない。
        const TSet<FIntPoint>& Roads,
        // 部屋ごとの使用済み入口（Used）を配列として引数で受け取る。元の値を参照し、コピーしない。
        const TArray<TArray<FIntPoint>>& Used,
        // 配置盤面の一辺のセル数（Limit）を符号付き32ビット整数として引数で受け取る。
        int32 Limit,
        // 再現用Seed（Seed）を符号付き32ビット整数として引数で受け取る。
        int32 Seed,
        // 呼び出し元へ返す結果（Out）を細道経路の候補として引数で受け取る。元の値を参照し、コピーしない。
        FRoute& Out);
    /** 細道と部屋の接触を重複計上せず、独立した周回数を返す。 */
    int32 CountNetworkLoops(const TArray<FCourtyardNode>& Rooms,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // 細道セルの集合（Roads）を重複しない集合として引数で受け取る。元の値を参照し、コピーしない。
        const TSet<FIntPoint>& Roads);
}
