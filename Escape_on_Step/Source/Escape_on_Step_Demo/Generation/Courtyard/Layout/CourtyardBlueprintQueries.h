#pragma once

#include "CoreMinimal.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardBlueprintQueries.generated.h"

/**
 * BPが生成の順序・条件・抽選を担当し、C++が大量のセルをまとめて調べるための窓口。
 *
 * このクラスはActorやメッシュを生成しない。入力データを調べて結果だけを返すので、
 * 部屋の形・乱数の呼び出し順・接続方針を変えずにBPの繰り返し処理を減らせる。
 */
// このクラスをUEの型情報へ登録し、UFUNCTIONの関数をBPへ公開できるようにする。
UCLASS()

// モジュール外からも使えるBP向け関数ライブラリを宣言する。インスタンスのActor配置は不要。
class ESCAPE_ON_STEP_DEMO_API UCourtyardBlueprintQueries : public UBlueprintFunctionLibrary
{
    // UEの型登録や反射機能に必要な、自動生成コードの宣言を挿入する。
    GENERATED_BODY()

public:
    /** 指定した部屋に記録されている入口数を数える。最低何個必要かはBPが決める。 */
    // 値を計算して返すBPノードとして公開する。実行ピンがなく、接続先で必要なときに評価される。
    UFUNCTION(BlueprintPure, Category = "Courtyard BP|Bulk queries")
    // 部屋ごとの使用済み入口数を返す。staticなので、ライブラリのインスタンスは不要。
    static int32 CountRoomEntrances(const TMap<FIntPoint, int32>& UsedEntrances, int32 RoomId);

    /** 独立した周回数を集計する。必要な周回数との比較・接続の追加はBPが行う。 */
    // 値を計算して返すBPノードとして公開する。実行ピンがなく、接続先で必要なときに評価される。
    UFUNCTION(BlueprintPure, Category = "Courtyard BP|Bulk queries")
    // 部屋配列を受け取り、独立した周回数を整数で返す関数を宣言する。
    static int32 CountLayoutLoops(const TArray<FCourtyardNode>& Rooms,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // 入力：既存の細道セルの対応表。件数・隣接関係を使って周回数を求める。
        const TMap<FIntPoint, bool>& RoadCells);

    /**
     * 全床を幅優先探索し、最短歩行距離と、条件に合う最遠セルを返す。
     * bRequireRoomNode=trueなら、細道を除いた屋外の塊だけをゴール候補にする。
     * 戻り値は全床に到達できた場合だけtrue。開始セルがなければfalse。
     */
    // 実行ピンを持つBPノードとして公開する。配列やMapをまとめて計算する処理の実行順を指定できる。
    UFUNCTION(BlueprintCallable, Category = "Courtyard BP|Bulk queries")
    // 全床を入力に、最短歩行距離と到達セルを計算し、全床へ到達できたかを返す関数を宣言する。
    static bool ComputeWalkingDistances(const TArray<FCourtyardCell>& Cells,
        // 開始セル（Start）を2次元の整数セル座標として引数で受け取る。
        FIntPoint Start,
        // bRequireRoomNode（bRequireRoomNode）を真偽値として引数で受け取る。
        bool bRequireRoomNode,
        // 各セルまたは候補の距離情報（Distances）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        TMap<FIntPoint, int32>& Distances,
        // Reachable（Reachable）を配列として引数で受け取る。元の値を参照し、コピーしない。
        TArray<FIntPoint>& Reachable,
        // 出力：条件に合う最遠セルの座標を、参照先へ書き込んで返す。
        FIntPoint& Goal);

    /**
     * 導入通路が収まる入口候補を列挙する。抽選やゴールの再計算はここでは行わない。
     * Dimensionsはcm単位でX=内幅、Y=奥行、Z=外幅。候補のbAlleyは内側の入口かを表す。
     * 配列順を従来版と揃え、同じSeedでBPの抽選結果が変わらないようにする。
     */
    // 実行ピンを持つBPノードとして公開する。配列やMapをまとめて計算する処理の実行順を指定できる。
    UFUNCTION(BlueprintCallable, Category = "Courtyard BP|Bulk queries")
    // 導入通路が収まる入口候補を、BPで扱える構造体の配列で返す関数を宣言する。
    static TArray<FCourtyardLink> FindEntranceCandidates(
        // 入力は全床、セルの実寸、内幅X・奥行Y・外幅Zの寸法。寸法の単位はcm。
        const TArray<FCourtyardCell>& Cells, float CellSize, FVector Dimensions);

    /** フレーム内の処理時間を測る時計。生成の乱数や配置結果には影響しない。 */
    // 時間予算の判定に使う時計を、値を返すBPノードとして公開する。
    UFUNCTION(BlueprintPure, Category = "Courtyard BP|Budget")
    // 高精度な実時間の秒数をdoubleで返す。フレーム内の作業時間を測るために使う。
    static double WorkClockSeconds();
};
