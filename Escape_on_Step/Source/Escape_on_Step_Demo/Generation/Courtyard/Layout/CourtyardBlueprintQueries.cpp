#include "Generation/Courtyard/Layout/CourtyardBlueprintQueries.h"
#include "Generation/Courtyard/Layout/CourtyardEntranceLayout.h"
#include "Generation/Courtyard/Layout/CourtyardOrganicSearch.h"
#include "HAL/PlatformTime.h"

// 指定した部屋の入口数を整数で返す関数を定義する。
int32 UCourtyardBlueprintQueries::CountRoomEntrances(
    // 入力は「入口セル→所属部屋ID」の対応表と、数えたい部屋のID。表をコピーせず参照する。
    const TMap<FIntPoint, int32>& UsedEntrances, int32 RoomId)
{
    int32 Count = 0;

    // 対応表に記録された入口を1件ずつ取り出す。元の表は書き換えない。
    for (const auto& Entrance : UsedEntrances)
    {
        // この入口の所属部屋IDが、数えたい部屋のIDと一致するかを調べる。
        if (Entrance.Value == RoomId)
        {
            ++Count;
        }
    }

    // 集計した件数をBPへ返す。何個あれば十分かは呼び出し元BPで比較する。
    return Count;
}

// 部屋配列を受け取り、独立した周回路の数を返す関数を定義する。
int32 UCourtyardBlueprintQueries::CountLayoutLoops(const TArray<FCourtyardNode>& Rooms,
    // 入力：部屋の各床セルが、どの部屋に所属するかを引く表。
    const TMap<FIntPoint, int32>& Owners,
    // 入力：細道として確保済みのセルの表。ここでは値ではなくキーの座標を使う。
    const TMap<FIntPoint, bool>& RoadCells)
{
    // BPで扱いやすいMapを、元のC++版が使う集合へ変換する。
    // ループの数え方を別実装にせず、元から使っている計算を共有する。
    // 同じ座標を重複登録しない、C++探索用の細道セル集合を用意する。
    TSet<FIntPoint> Roads;
    Roads.Reserve(RoadCells.Num());

    // BPから受け取った細道の対応表を1件ずつ読む。
    for (const auto& Road : RoadCells)
    {
        // 表のキーであるセル座標を、細道の集合へ追加する。
        Roads.Add(Road.Key);
    }

    // 元の実装と同じ周回数計算を呼び、結果だけをBPへ返す。
    return CourtyardOrganic::CountNetworkLoops(Rooms, Owners, Roads);
}

// 全床セルを入力に、歩行距離を求める関数を定義する。成否はboolで返す。
bool UCourtyardBlueprintQueries::ComputeWalkingDistances(const TArray<FCourtyardCell>& Cells,
    // 入力：探索を始めるセルの整数座標。cm単位のワールド座標ではない。
    FIntPoint Start,
    // 入力：trueなら細道をゴール候補から外し、所属部屋のある屋外セルだけを候補にする。
    bool bRequireRoomNode,
    // 出力：各セルへ最短で何マス歩くかを格納する表。参照渡しで呼び出し元へ返す。
    TMap<FIntPoint, int32>& Distances,
    // 出力：到達したセルを探索順に並べる配列。探索待ち行列としても使う。
    TArray<FIntPoint>& Reachable,
    // 出力：ゴール条件に合うセルのうち、開始位置から最も遠かったセル。
    FIntPoint& Goal)
{
    // 前回の呼び出し結果を残さない。失敗時のGoalも必ず開始セルに戻す。
    Distances.Reset();
    Reachable.Reset();
    // 候補がまだない段階のゴールを開始セルにする。失敗時も未設定の座標を返さない。
    Goal = Start;

    // セル座標から種別・所属部屋を素早く参照できる索引を1回だけ作る。
    // セル座標からCells配列の添字を引くための索引を用意する。
    TMap<FIntPoint, int32> CellIndices;
    CellIndices.Reserve(Cells.Num());

    // 床セル配列を先頭から末尾まで、添字を付けて読む。
    for (int32 Index = 0; Index < Cells.Num(); ++Index)
    {
        // この床セルの座標と、その詳細が格納された配列添字を対応付ける。
        CellIndices.Add(Cells[Index].Position, Index);
    }

    // 開始セルが床の一覧に存在するかを確認する。存在しない位置からは探索できない。
    if (!CellIndices.Contains(Start))
    {
        return false;
    }

    // BP版と同じ東→北→西→南の順番で探索する。
    // 最遠距離が同じ候補が複数ある場合も、従来と同じセルが選ばれる。
    // 隣のセルへ進む差分を東・北・西・南の順に定義する。順番を保ち再現性を維持する。
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    Distances.Reserve(Cells.Num());
    Reachable.Reserve(Cells.Num());
    // 開始セル自身へ到達する距離は0マスとして登録する。
    Distances.Add(Start, 0);
    // 最初に調べるセルとして開始セルを待ち行列へ入れる。
    Reachable.Add(Start);
    int32 BestDistance = 0;

    // 待ち行列を先頭から処理する。探索中に後ろへ追加されたセルも順に処理する。
    for (int32 QueueIndex = 0; QueueIndex < Reachable.Num(); ++QueueIndex)
    {
        // 今回調べるセル座標を値として取り出す。配列追加で領域が移動しても参照が壊れない。
        const FIntPoint Current = Reachable[QueueIndex];
        // 今のセルまでの最短距離に1マス加え、隣のセルまでの距離を求める。
        const int32 NextDistance = Distances.FindChecked(Current) + 1;

        // 今のセルから、四方向それぞれの隣接セルを調べる。
        for (const FIntPoint Step : Steps)
        {
            // 現在位置に方向の差分を足し、調べる隣接セルの座標を作る。
            const FIntPoint Next = Current + Step;
            // 隣接座標に床があれば配列添字へのポインター、なければnullptrを受け取る。
            const int32* CellIndex = CellIndices.Find(Next);

            // 床がないセル、または既に最短距離を登録したセルは探索対象から外す。
            if (!CellIndex || Distances.Contains(Next))
            {
                continue;
            }

            // 初めて到達したセルに最短歩行距離を記録する。幅優先探索なので後から短くならない。
            Distances.Add(Next, NextDistance);
            // 隣接セルを行列の末尾へ入れ、後でその周囲も調べる。
            Reachable.Add(Next);

            // 添字のポインターを参照し、このセルの種別や所属部屋をコピーせずに読む。
            const FCourtyardCell& Cell = Cells[*CellIndex];
            // ゴール候補には屋外セルだけを許可する。次行の所属部屋条件も同時に満たす必要がある。
            const bool bGoalCandidate = Cell.Kind == ECourtyardCellKind::Street &&
                                        // 部屋所属を要求しないか、実際に所属部屋IDがある場合に、追加条件を満たす。
                                        (!bRequireRoomNode || Cell.NodeId != INDEX_NONE);

            // ゴール候補で、かつ現在の候補より遠いときだけ更新する。同距離なら先に見つけた候補を残す。
            if (bGoalCandidate && NextDistance > BestDistance)
            {
                // 今の隣接セルを、新しい最遠ゴール候補として記録する。
                Goal = Next;
                // 新しい候補までの距離を保存し、以後の候補と比較できるようにする。
                BestDistance = NextDistance;
            }
        }
    }

    // ゴールだけ見つかっても採用しない。孤立した床があればBPへ失敗を返す。
    // 到達件数が全床件数と一致するときだけ成功。孤立した床や重複セルがあれば失敗とする。
    return Reachable.Num() == Cells.Num();
}

// BPで扱える既存の接続構造体の配列として、入口候補を返す関数を定義する。
TArray<FCourtyardLink> UCourtyardBlueprintQueries::FindEntranceCandidates(
    // 入力：全床、1セルの実寸、導入通路の内幅X・奥行Y・外幅Z。寸法はcm単位。
    const TArray<FCourtyardCell>& Cells, float CellSize, FVector Dimensions)
{
    // 共有するC++の入口検査が返す、候補セル列・方向・内側判定の配列を用意する。
    TArray<FCourtyardEntranceSelection> Selections;
    // 候補を列挙できなかった理由を共有処理から受け取る文字列を用意する。
    FString Error;

    // 入口検査を実行し、寸法が不正、または収まる候補がない場合を判定する。
    if (!CourtyardEntranceLayout::FindCandidates(
            // 全床と通路の3寸法を渡し、候補一覧と失敗理由を受け取る。ここでは抽選しない。
            Cells, CellSize, Dimensions.X, Dimensions.Y, Dimensions.Z, Selections, Error))
    {
        // 候補を作れなかったので空配列を返す。再試行やエラー表示の判断はBPに任せる。
        return {};
    }

    // BP側の既存配列型を使い、構造体を追加しなくても候補の抽選・絞り込みを編集できる。
    // BPへ返す入口候補の配列を用意する。
    TArray<FCourtyardLink> Candidates;
    Candidates.Reserve(Selections.Num());

    // C++形式の候補を元の順番で読む。順番を変えると同じ乱数でも抽選結果が変わるため保持する。
    for (const FCourtyardEntranceSelection& Selection : Selections)
    {
        // 1候補分のBP向け構造体を既定値で作る。
        FCourtyardLink Candidate;
        // 候補の各項目をまとめて設定する。特殊な項目の扱いは個別の説明を参照。
        Candidate.Cells = Selection.Cells;
        Candidate.Direction = Selection.OutwardDirection;
        // 候補配列内ではbAlleyを「内側の入口か」の受け渡しに使う。通常の接続配列とは用途が異なる。
        Candidate.bAlley = Selection.bInterior;
        // 作った候補を配列へ移動して追加し、配列などの不要なコピーを減らす。
        Candidates.Add(MoveTemp(Candidate));
    }

    // すべての候補をBPへ返す。どれを選ぶかの抽選はBP側で行う。
    return Candidates;
}

// フレーム内の時間予算を調べるため、実時間の秒数を返す関数を定義する。
double UCourtyardBlueprintQueries::WorkClockSeconds()
{
    // ゲーム内時刻はフレーム途中で進まないため、時間予算の判定には実時間を使う。
    // エンジンの高精度時計を秒で返す。ゲームの一時停止やフレーム更新を待たずに経過時間を測れる。
    return FPlatformTime::Seconds();
}
