/**
 * BP生成とISM描画の連携、および共有する重い探索の呼び出し口。
 * 部屋の形・処理順・接続の採否はBPで決め、C++側は指定された計算だけを行う。
 */

#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"
#include "Generation/Courtyard/Layout/CourtyardOrganicSearch.h"

/** 生成開始時に前回の表示を消し、床ができるまでプレイヤーの落下を防ぐ。 */
bool ACourtyardISMGenerator::BeginBlueprintLayout()
{
    bBlueprintEntranceReady = false;
    // 前回生成したISMや小物と、描画待ちの作業を消し、今回の生成状態を初期化する。
    ClearCourtyardISM();
    // まだ床がない生成中にプレイヤーが落ちないよう、移動を一時停止する。
    FreezePrototypePlayer(true);
    return true;
}

/** BPが再試行上限に達した場合の終了処理。エラー記録と暗転・移動制限の解除をまとめる。 */
// BPから受け取った失敗理由を使い、生成失敗時の終了処理を行う。
bool ACourtyardISMGenerator::FailBlueprintLayout(const FString& Reason)
{
    // エラーを記録し、タイマー・暗転・移動制限を解除する共通処理へ理由を渡す。
    return PrototypeFail(Reason);
}

/** セルとSeedから0〜6の固定コストを返す。乱数列を進めないため、抽選順には影響しない。 */
// セル座標とSeedを入力に、経路コストへ加える小さな固定値を返す。
int32 ACourtyardISMGenerator::RouteCellNoise(FIntPoint Cell, int32 LayoutSeed)
{
    // 座標のハッシュとSeedを組み合わせ、7で割った余りを0〜6のコストとして返す。乱数列は進めない。
    return int32(HashCombineFast(GetTypeHash(Cell), uint32(LayoutSeed)) % 7);
}

/** 部屋の輪郭を変えず、位置だけを詰める。探索の内訳はCourtyardOrganicLayout.cppを参照。 */
// 部屋配列の位置を詰める共有探索を呼ぶ。戻り値ではなくRoomsを書き換えて返す。
void ACourtyardISMGenerator::CompactBlueprintRooms(
    // 入力兼出力の部屋配列、配置可能な一辺のセル数、再現用Seedを受け取る。
    TArray<FCourtyardNode>& Rooms, int32 Limit, int32 LayoutSeed)
{
    // 移動相手になる部屋があり、配置範囲も正の場合だけ位置の圧縮を行う。
    if (Rooms.Num() > 1 && Limit > 0)
    {
        // 部屋の輪郭や面積を変えず、位置だけを詰める元のC++探索を実行する。
        CourtyardOrganic::CompactRooms(Rooms, Limit, LayoutSeed);
    }
}

/** 2室間の経路候補を返す。接続する室の選択や、入口・周回の最低条件はBPの責務。 */
bool ACourtyardISMGenerator::FindBlueprintRoomRoute(int32 FromRoom,
    // 接続先の部屋IDを受け取る。部屋配列の有効な添字である必要がある。
    int32 ToRoom,
    // 各部屋の形と床セルを入力として読み、通路が部屋内を横切らないようにする。
    const TArray<FCourtyardNode>& Rooms,
    // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
    const TMap<FIntPoint, int32>& Owners,
    // 既存の細道セルを読み、使える道は共有する経路探索の入力にする。
    const TMap<FIntPoint, bool>& RoadCells,
    // 使用済みの入口セルと所属部屋を受け取り、近すぎる入口を避ける。
    const TMap<FIntPoint, int32>& UsedEntrances,
    // 部屋を配置した盤面の一辺のセル数を受け取り、探索範囲の基準にする。
    int32 Limit,
    // この経路のコストを再現するためのSeedを受け取る。
    int32 LayoutSeed,
    // 出力：接続元・接続先と、見つかった経路セルを既存のBP向け構造体で返す。
    FCourtyardLink& Route)
{
    // 無効な入力でも前回の経路を返さないよう、出力を最初に空へ戻す。
    // 出力を空の経路へ初期化し、入力不正や探索失敗時に前回の経路を残さない。
    Route = FCourtyardLink();

    // 開始・接続先が配列内に存在するか、同じ部屋同士を指定していないかを確認する。
    if (!Rooms.IsValidIndex(FromRoom) || !Rooms.IsValidIndex(ToRoom) || FromRoom == ToRoom ||
        // 配置範囲が正でない場合も不正な入力として探索を中止する。
        Limit <= 0)
    {
        return false;
    }

    // BPのMap形式を元の経路探索の入力形式へ変換する。
    // 細道セルの集合（Roads）を重複しない集合として宣言する。
    TSet<FIntPoint> Roads;
    // 既存細道の対応表から、座標のキーを1件ずつ取り出す。
    for (const auto& Entry : RoadCells)
    {
        // 座標を集合に登録し、元のC++探索が細道の存在を素早く判定できるようにする。
        Roads.Add(Entry.Key);
    }

    // 部屋ごとの使用済み入口。近すぎる入口が同じ壁へ集中するのを避ける。
    // 部屋IDごとに、使用済み入口の座標配列を持つ二重配列を用意する。
    TArray<TArray<FIntPoint>> Used;
    // 部屋の数だけ内側の配列を用意し、部屋IDを添字として使えるようにする。
    Used.SetNum(Rooms.Num());
    // BP形式の「使用済み入口セル→部屋ID」の対応表を1件ずつ読む。
    for (const auto& Entry : UsedEntrances)
    {
        // 記録された部屋IDが有効な配列添字か確認し、範囲外アクセスを避ける。
        if (Used.IsValidIndex(Entry.Value))
        {
            // この入口セルを、その所属部屋の使用済み入口一覧へ追加する。
            Used[Entry.Value].Add(Entry.Key);
        }
    }

    // 重い探索は共有実装へ任せる。失敗時は空のRouteとfalseを返す。
    // 共有するC++探索の出力形式で、経路候補を受け取る変数を作る。
    CourtyardOrganic::FRoute Candidate;
    // 2室間をつなぐ探索を実行し、有効な長さの経路が見つからなければ失敗を返す。
    if (!CourtyardOrganic::FindRoomConnectionRoute(
            // 部屋・床の所有者・既存細道・使用済み入口・探索範囲・Seedを渡し、経路を受け取る。
            FromRoom, ToRoom, Rooms, Owners, Roads, Used, Limit, LayoutSeed, Candidate))
    {
        return false;
    }

    // 候補だけ返す。RoadCellsや部屋同士の接続状態をここで書き換えない。
    // Routeの各項目を、以下で計算・取得した値へまとめて設定する。
    Route.From = FromRoom;
    Route.To = ToRoom;
    Route.Cells = MoveTemp(Candidate.Cells);
    Route.bAlley = true;
    return true;
}
