#include "Generation/Courtyard/Layout/CourtyardRoomShapes.h"
/**
 * 中庭ステージの配置データを生成する。ここではメッシュやActorを生成しない。
 * 読み始める場所: BuildOrganicCourtyardPlan の番号付きコメント。
 * 描画は CourtyardISMGenerator.cpp、導入通路との接続は CourtyardIntroCorridor.cpp が担当する。
 * 座標・距離・道幅は原則として「セル（マス）」単位。cmへの変換は描画側で行う。
 */
#include "Generation/Courtyard/Layout/CourtyardLayout.h"
#include "Algo/Reverse.h"
#include "Generation/Courtyard/Layout/CourtyardOrganicSearch.h"

namespace CourtyardOrganic
{
    // const FIntPoint Steps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    /** 部屋の外周から、出入口にできる「室内セル・隣接する室外セル・方向」を列挙する。
 * 既存の出入口からマンハッタン距離2未満の候補を避け、同じ場所への接続集中を抑える。
 */
    TArray<FPort> FindAvailableRoomPorts(const FCourtyardNode& Room,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // 部屋ごとの使用済み入口（Used）を配列として引数で受け取る。元の値を参照し、コピーしない。
        const TArray<FIntPoint>& Used)
    {
        // この検査や計算の結果（Result）を配列として宣言する。
        TArray<FPort> Result;
        for (auto P : Room.FloorCells)
        {
            bool Separate = true;
            for (auto U : Used)
            {
                // 「符号を取り除いた絶対値を求める（引数：PのX成分からUのX成分を引いた値）と符号を取り除いた絶対値を求める（引数：PのY成分からUのY成分を引いた値）の和が2より小さい」に当てはまる場合だけ、続く文を実行する。
                if (FMath::Abs(P.X - U.X) + FMath::Abs(P.Y - U.Y) < 2)
                {
                    Separate = false;
                    break;
                }
            }
            if (!Separate)
            {
                continue;
            }
            for (int32 D = 0; D < 4; ++D)
            {
                // 「「床セルから所属部屋を引く表（Owners）で、指定値が登録されているか調べる（引数：Pと東・北・西・南の移動差分（Steps）の「D」に対応する要素の和）」が成立しない」に当てはまる場合だけ、続く文を実行する。
                if (!Owners.Contains(P + Steps[D]))
                {
                    // この検査や計算の結果（Result）で、要素または対応関係を末尾へ追加する（引数：{P, P + Steps[D], D}）。
                    Result.Add({P, P + Steps[D], D});
                }
            }
        }
        // この検査や計算の結果（Result）を呼び出し元へ返す。
        return Result;
    }

    /** 2つの部屋の出入口候補間を、累積コストの小さい順に探索する。
 * Ownersは部屋の床セル→部屋ID、Roadsは既に確保した細道セルの集合。
 * 部屋の内部を横切らず、既存の細道を共有しやすい経路を選ぶ。
 * Out.Cellsには部屋内部を含めず、部屋の間の細道セルだけを順番に格納する。
 */
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
        FRoute& Out)
    {
        // 開始側の入口候補（Starts）を右辺から自動判定した型として用意し、既存入口から離れた外周の入口候補を集める（引数：部屋の一覧（Rooms）の「A」に対応する要素、床セルから所属部屋を引く表（Owners）、部屋ごとの使用済み入口（Used）の「A」に対応する要素）で初期化する。
        const auto Starts = FindAvailableRoomPorts(Rooms[A], Owners, Used[A]),
                   // 到達側の候補または末端数（Ends）を、既存入口から離れた外周の入口候補を集める（引数：部屋の一覧（Rooms）の「B」に対応する要素、床セルから所属部屋を引く表（Owners）、部屋ごとの使用済み入口（Used）の「B」に対応する要素）に設定する。
                   Ends = FindAvailableRoomPorts(Rooms[B], Owners, Used[B]);
        // 「「開始側の入口候補（Starts）で、空かを調べる」または「到達側の候補または末端数（Ends）で、空かを調べる」」に当てはまる場合だけ、続く文を実行する。
        if (Starts.IsEmpty() || Ends.IsEmpty())
        {
            return false;
        }
        // 到達先に使える室外セル（Targets）を重複しない集合として宣言する。
        TSet<FIntPoint> Targets;
        for (const auto& P : Ends)
        {
            // 到達先に使える室外セル（Targets）で、要素または対応関係を末尾へ追加する（引数：Pの室外側の隣接セル）。
            Targets.Add(P.Outside);
        }
        // 外周を迂回できる余白を含む有限の探索盤面を用意する。
        // 横方向または盤面の一辺（Side）に配置盤面の一辺のセル数（Limit）と9の和を格納する。探索可能な最小セル座標（Low）に-3を格納する。対象の件数（Count）に横方向または盤面の一辺（Side）と横方向または盤面の一辺（Side）の積を格納する。
        const int32 Side = Limit + 9, Low = -3, Count = Side * Side;
        // Cost: 累積コスト。Previous: 経路を逆向きにたどるための直前セルの添字。
        // セルまでの累積コスト（Cost）、経路を逆にたどる直前セル（Previous）を、それぞれ配列として用意する。
        TArray<int32> Cost, Previous;
        // セルまでの累積コスト（Cost）で、指定件数の要素を同じ初期値で埋める（引数：MAX_int32、対象の件数（Count））。
        Cost.Init(MAX_int32, Count);
        // 経路を逆にたどる直前セル（Previous）で、指定件数の要素を同じ初期値で埋める（引数：INDEX_NONE（未指定を表す-1）、対象の件数（Count））。
        Previous.Init(INDEX_NONE, Count);
        // 探索が確定したセル（Closed）を配列として宣言する。
        TArray<uint8> Closed;
        // 探索が確定したセル（Closed）で、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
        Closed.Init(0, Count);
        // 配列の添字（Index）を、周囲の値を使って判定・計算する小さな関数として定義する。
        const auto Index = [&](FIntPoint P)
        {
            // PのY成分から探索可能な最小セル座標（Low）を引いた値と横方向または盤面の一辺（Side）の積とPのX成分の和から探索可能な最小セル座標（Low）を引いた値を呼び出し元へ返す。
            return (P.Y - Low) * Side + P.X - Low;
        };
        // Point（Point）を、周囲の値を使って判定・計算する小さな関数として定義する。
        const auto Point = [&](int32 I)
        {
            // 2次元の整数セル座標を組み立てる（引数：Iを横方向または盤面の一辺（Side）で割った余りと探索可能な最小セル座標（Low）の和、Iを横方向または盤面の一辺（Side）で割った値と探索可能な最小セル座標（Low）の和）を呼び出し元へ返す。
            return FIntPoint(I % Side + Low, I / Side + Low);
        };
        // 室内側のセル（Inside）を、周囲の値を使って判定・計算する小さな関数として定義する。
        const auto Inside = [&](FIntPoint P)
        {
            // 「「「PのX成分が探索可能な最小セル座標（Low）以上」かつ「PのY成分が探索可能な最小セル座標（Low）以上」」かつ「PのX成分が探索可能な最小セル座標（Low）と横方向または盤面の一辺（Side）の和より小さい」」かつ「PのY成分が探索可能な最小セル座標（Low）と横方向または盤面の一辺（Side）の和より小さい」を呼び出し元へ返す。
            return P.X >= Low && P.Y >= Low && P.X < Low + Side && P.Y < Low + Side;
        };
        // 未確定の探索候補（Open）を配列として宣言する。
        TArray<int32> Open;
        for (const auto& P : Starts)
        {
            // 「Inside(P.Outside)」に当てはまる場合だけ、続く文を実行する。
            if (Inside(P.Outside))
            {
                // I（I）を符号付き32ビット整数として用意し、Index(P.Outside)で初期化する。
                const int32 I = Index(P.Outside);
                // 「セルまでの累積コスト（Cost）の「I」に対応する要素が0と異なる」に当てはまる場合だけ、続く文を実行する。
                if (Cost[I] != 0)
                {
                    // セルまでの累積コスト（Cost）の「I」に対応する要素を、0に設定する。
                    Cost[I] = 0;
                    // 未確定の探索候補（Open）で、要素または対応関係を末尾へ追加する（引数：I）。
                    Open.Add(I);
                }
            }
        }
        // 探索の到達先（Goal）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
        int32 Goal = INDEX_NONE;
        // 「「未確定の探索候補（Open）で、空かを調べる」が成立しない」が成り立つ間、続く文を繰り返す。
        while (!Open.IsEmpty())
        {
            int32 Best = 0;
            for (int32 I = 1; I < Open.Num(); ++I)
            {
                // 「Cost[Open[I]]がCost[Open[Best]]より小さい」に当てはまる場合だけ、続く文を実行する。
                if (Cost[Open[I]] < Cost[Open[Best]])
                {
                    // 現在の最良候補（Best）を、Iに設定する。
                    Best = I;
                }
            }
            // 今回処理する値または位置（Current）を符号付き32ビット整数として用意し、未確定の探索候補（Open）の「現在の最良候補（Best）」に対応する要素で初期化する。
            const int32 Current = Open[Best];
            // 未確定の探索候補（Open）で、指定添字を末尾要素と入れ替えて取り除く。順序は維持しない（引数：現在の最良候補（Best）、EAllowShrinking::No）。
            Open.RemoveAtSwap(Best, EAllowShrinking::No);
            // 「探索が確定したセル（Closed）の「今回処理する値または位置（Current）」に対応する要素」に当てはまる場合だけ、続く文を実行する。
            if (Closed[Current])
            {
                continue;
            }
            // 探索が確定したセル（Closed）の「今回処理する値または位置（Current）」に対応する要素を、1に設定する。
            Closed[Current] = 1;
            // P（P）を右辺から自動判定した型として用意し、Point(Current)で初期化する。
            const auto P = Point(Current);
            // 「到達先に使える室外セル（Targets）で、指定値が登録されているか調べる（引数：P）」に当てはまる場合だけ、続く文を実行する。
            if (Targets.Contains(P))
            {
                // 探索の到達先（Goal）を、今回処理する値または位置（Current）に設定する。
                Goal = Current;
                break;
            }
            for (auto Step : Steps)
            {
                // Q（Q）を右辺から自動判定した型として用意し、Pと隣接セルへ進む差分（Step）の和で初期化する。
                const auto Q = P + Step;
                // 「「「Inside(Q)」が成立しない」または「床セルから所属部屋を引く表（Owners）で、指定値が登録されているか調べる（引数：Q）」」に当てはまる場合だけ、続く文を実行する。
                if (!Inside(Q) || Owners.Contains(Q))
                {
                    continue;
                }
                // J（J）を符号付き32ビット整数として用意し、Index(Q)で初期化する。
                const int32 J = Index(Q);
                // 「探索が確定したセル（Closed）の「J」に対応する要素」に当てはまる場合だけ、続く文を実行する。
                if (Closed[J])
                {
                    continue;
                }
                // 座標とSeedから求める経路コストの変化（Noise）を符号なし32ビット整数として用意し、2つのハッシュ値を組み合わせる（引数：値に対応するハッシュを求める（引数：Q）、符号なし32ビット整数へ変換する（引数：再現用Seed（Seed）））で初期化する。
                const uint32 Noise = HashCombineFast(GetTypeHash(Q), uint32(Seed));
                // 同方向へ連続したセル数（Run）に1を格納する。直進の長さを逆に調べるセル添字（Back）に今回処理する値または位置（Current）を格納する。
                int32 Run = 1, Back = Current;
                // 「経路を逆にたどる直前セル（Previous）の「直進の長さを逆に調べるセル添字（Back）」に対応する要素がINDEX_NONE（未指定を表す-1）と異なる」、かつ次行の条件が成り立つ間、続く文を繰り返す。
                while (Previous[Back] != INDEX_NONE &&
                       // この行では「Point(Back)からPoint(Previous[Back])を引いた値が隣接セルへ進む差分（Step）と等しい」かつ「同方向へ連続したセル数（Run）が6より小さい」を指定し、前後の行の計算・呼び出しへつなげる。
                       Point(Back) - Point(Previous[Back]) == Step && Run < 6)
                {
                    ++Run;
                    // 直進の長さを逆に調べるセル添字（Back）を、経路を逆にたどる直前セル（Previous）の「直進の長さを逆に調べるセル添字（Back）」に対応する要素に設定する。
                    Back = Previous[Back];
                }
                // 既存の道は10、新しい道は12～18。6セル以上の直進には45を追加して曲がりやすくする。
                // 隣接セルを通る場合の累積コスト（NextCost）を符号付き32ビット整数として用意し、で初期化する。
                const int32 NextCost =
                    // セルまでの累積コスト（Cost）の「今回処理する値または位置（Current）」に対応する要素と「細道セルの集合（Roads）で、指定値が登録されているか調べる（引数：Q）」なら10、そうでなければ12と座標とSeedから求める経路コストの変化（Noise）を7で割った余りの和の和と「同方向へ連続したセル数（Run）が6以上」なら45、そうでなければ0の和。
                    Cost[Current] + (Roads.Contains(Q) ? 10 : 12 + Noise % 7) + (Run >= 6 ? 45 : 0);
                // 「隣接セルを通る場合の累積コスト（NextCost）がセルまでの累積コスト（Cost）の「J」に対応する要素より小さい」に当てはまる場合だけ、続く文を実行する。
                if (NextCost < Cost[J])
                {
                    // 「セルまでの累積コスト（Cost）の「J」に対応する要素がMAX_int32と等しい」に当てはまる場合だけ、続く文を実行する。
                    if (Cost[J] == MAX_int32)
                    {
                        // 未確定の探索候補（Open）で、要素または対応関係を末尾へ追加する（引数：J）。
                        Open.Add(J);
                    }
                    // セルまでの累積コスト（Cost）の「J」に対応する要素を、隣接セルを通る場合の累積コスト（NextCost）に設定する。
                    Cost[J] = NextCost;
                    // 経路を逆にたどる直前セル（Previous）の「J」に対応する要素を、今回処理する値または位置（Current）に設定する。
                    Previous[J] = Current;
                }
            }
        }
        // 「探索の到達先（Goal）がINDEX_NONE（未指定を表す-1）と等しい」に当てはまる場合だけ、続く文を実行する。
        if (Goal == INDEX_NONE)
        {
            return false;
        }
        // 到達セルから出発側まで逆にたどり、最後に配列を出発→到達の順へ反転する。
        // 呼び出し元へ返す結果（Out）を、{A, B, {}}に設定する。
        Out = {A, B, {}};
        for (int32 I = Goal; I != INDEX_NONE; I = Previous[I])
        {
            // 呼び出し元へ返す結果（Out）のセル列で、要素または対応関係を末尾へ追加する（引数：Point(I)）。
            Out.Cells.Add(Point(I));
        }
        // 経路配列の順番を逆転する（引数：呼び出し元へ返す結果（Out）のセル列）。
        Algo::Reverse(Out.Cells);
        // 部屋間の1接続が長い移動だけの区間にならないよう、16セルを上限とする。
        // 呼び出し元へ返す結果（Out）のセル列で、要素数を得るが16以下を呼び出し元へ返す。
        return Out.Cells.Num() <= 16;
    }

    /** 道を作る前に、部屋を平行移動して配置全体の空白を減らす。
 * 形・向き・床面積は変えず、外接矩形同士に最低1セルの間隔を残す。
 * 部屋0は基準位置として固定し、残りの部屋を最大8巡調整する。
 */
    void CompactRooms(TArray<FCourtyardNode>& Rooms, int32 Limit, int32 Seed)
    {
        // 配置圧縮専用の乱数生成器（PackingRandom）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
        FRandomStream PackingRandom(uint32(Seed) ^ 0x29d175abu);
        // 処理する添字の順番（Order）を配列として宣言する。
        TArray<int32> Order;
        for (int32 I = 1; I < Rooms.Num(); ++I)
        {
            // 処理する添字の順番（Order）で、要素または対応関係を末尾へ追加する（引数：I）。
            Order.Add(I);
        }
        for (int32 Pass = 0; Pass < 8; ++Pass)
        {
            for (int32 I = Order.Num() - 1; I > 0; --I)
            {
                // 処理する添字の順番（Order）で、指定した2つの添字の要素を交換する（引数：I、配置圧縮専用の乱数生成器（PackingRandom）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
                Order.Swap(I, PackingRandom.RandRange(0, I));
            }
            bool Moved = false;
            for (int32 Id : Order)
            {
                // 対象の部屋（Room）を右辺から自動判定した型として用意し、部屋の一覧（Rooms）の「空間の識別番号（Id）」に対応する要素で初期化する。元の値を参照し、コピーしない。
                auto& Room = Rooms[Id];
                // 候補の評価値（Score）を、周囲の値を使って判定・計算する小さな関数として定義する。
                const auto Score = [&](FIntPoint Position)
                {
                    // 全体の最大X端（MaxX）にセルまたは配置位置（Position）のX成分と対象の部屋（Room）の大きさのX成分の和を格納する。全体の最大Y端（MaxY）にセルまたは配置位置（Position）のY成分と対象の部屋（Room）の大きさのY成分の和を格納する。
                    int32 MaxX = Position.X + Room.Size.X, MaxY = Position.Y + Room.Size.Y,
                          Distances = 0;
                    // 中央位置（Center）を2次元の整数セル座標として用意し、セルまたは配置位置（Position）と2の積と対象の部屋（Room）の大きさの和で初期化する。
                    const FIntPoint Center = Position * 2 + Room.Size;
                    for (const auto& Other : Rooms)
                    {
                        // 「Otherの識別番号が空間の識別番号（Id）と異なる」に当てはまる場合だけ、続く文を実行する。
                        if (Other.Id != Id)
                        {
                            // 全体の最大X端（MaxX）を、大きい方の値を選ぶ（引数：全体の最大X端（MaxX）、Otherの最小端のX成分とOtherの大きさのX成分の和）に設定する。
                            MaxX = FMath::Max(MaxX, Other.Min.X + Other.Size.X);
                            // 全体の最大Y端（MaxY）を、大きい方の値を選ぶ（引数：全体の最大Y端（MaxY）、Otherの最小端のY成分とOtherの大きさのY成分の和）に設定する。
                            MaxY = FMath::Max(MaxY, Other.Min.Y + Other.Size.Y);
                            // 相対位置の差分（Delta）を右辺から自動判定した型として用意し、中央位置（Center）からOtherの最小端と2の積とOtherの大きさの和を引いた値で初期化する。
                            const auto Delta = Center - (Other.Min * 2 + Other.Size);
                            // 各セルまたは候補の距離情報（Distances）へ、符号を取り除いた絶対値を求める（引数：相対位置の差分（Delta）のX成分）と符号を取り除いた絶対値を求める（引数：相対位置の差分（Delta）のY成分）の和を加算する。
                            Distances += FMath::Abs(Delta.X) + FMath::Abs(Delta.Y);
                        }
                    }
                    // 配置範囲の面積を最優先し、細長さと部屋間距離も小さくなる候補を選ぶ。
                    // 全体の最大X端（MaxX）と全体の最大Y端（MaxY）の積と64の積と符号を取り除いた絶対値を求める（引数：全体の最大X端（MaxX）から全体の最大Y端（MaxY）を引いた値）と4の積の和と各セルまたは候補の距離情報（Distances）の和を呼び出し元へ返す。
                    return MaxX * MaxY * 64 + FMath::Abs(MaxX - MaxY) * 4 + Distances;
                };
                // 現在の最良候補（Best）を2次元の整数セル座標として用意し、対象の部屋（Room）の最小端で初期化する。
                FIntPoint Best = Room.Min;
                // 最良候補の評価値（BestScore）を符号付き32ビット整数として用意し、Score(Best)で初期化する。
                int32 BestScore = Score(Best);
                for (int32 X = 0; X <= Limit - Room.Size.X; ++X)
                {
                    for (int32 Y = 0; Y <= Limit - Room.Size.Y; ++Y)
                    {
                        // Candidate（Candidate）を、括弧内の値を使って2次元の整数セル座標として初期化する。
                        const FIntPoint Candidate(X, Y);
                        bool Fits = true;
                        for (const auto& Other : Rooms)
                        {
                            // 「「Otherの識別番号が空間の識別番号（Id）と異なる」かつ「XがOtherの最小端のX成分とOtherの大きさのX成分の和と1の和より小さい」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
                            if (Other.Id != Id && X < Other.Min.X + Other.Size.X + 1 &&
                                // この行ではXと対象の部屋（Room）の大きさのX成分の和と1の和がOtherの最小端のX成分より大きいを指定し、前後の行の計算・呼び出しへつなげる。
                                X + Room.Size.X + 1 > Other.Min.X &&
                                // この行ではYがOtherの最小端のY成分とOtherの大きさのY成分の和と1の和より小さいを指定し、前後の行の計算・呼び出しへつなげる。
                                Y < Other.Min.Y + Other.Size.Y + 1 &&
                                // この行ではYと対象の部屋（Room）の大きさのY成分の和と1の和がOtherの最小端のY成分より大きいを指定し、前後の行の計算・呼び出しへつなげる。
                                Y + Room.Size.Y + 1 > Other.Min.Y)
                            {
                                Fits = false;
                                break;
                            }
                        }
                        if (!Fits)
                        {
                            continue;
                        }
                        // CandidateScore（CandidateScore）を符号付き32ビット整数として用意し、Score(Candidate)で初期化する。
                        const int32 CandidateScore = Score(Candidate);
                        // 「CandidateScoreが最良候補の評価値（BestScore）より小さい」に当てはまる場合だけ、続く文を実行する。
                        if (CandidateScore < BestScore)
                        {
                            // 現在の最良候補（Best）を、Candidateに設定する。
                            Best = Candidate;
                            // 最良候補の評価値（BestScore）を、CandidateScoreに設定する。
                            BestScore = CandidateScore;
                        }
                    }
                }
                // 移動差分（Offset）を右辺から自動判定した型として用意し、現在の最良候補（Best）から対象の部屋（Room）の最小端を引いた値で初期化する。
                const auto Offset = Best - Room.Min;
                // 「移動差分（Offset）がFIntPoint::ZeroValueと異なる」に当てはまる場合だけ、続く文を実行する。
                if (Offset != FIntPoint::ZeroValue)
                {
                    // 対象の部屋（Room）の最小端を、現在の最良候補（Best）に設定する。
                    Room.Min = Best;
                    for (auto& Cell : Room.FloorCells)
                    {
                        // Cellへ、移動差分（Offset）を加算する。
                        Cell += Offset;
                    }
                    Moved = true;
                }
            }
            if (!Moved)
            {
                break;
            }
        }
    }

    /** 部屋を1頂点として扱い、接続ネットワークの独立した周回数を数える。
 * 接続済みグラフに対する「辺数 - 頂点数 + 1」を使用する。
 */
    int32 CountNetworkLoops(const TArray<FCourtyardNode>& Rooms,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // 細道セルの集合（Roads）を重複しない集合として引数で受け取る。元の値を参照し、コピーしない。
        const TSet<FIntPoint>& Roads)
    {
        int32 Edges = 0;
        // 細道セルと部屋の接触を重複なく数える集合（RoomEdges）を重複しない集合として宣言する。
        TSet<uint64> RoomEdges;
        for (auto P : Roads)
        {
            for (int32 D = 0; D < 4; ++D)
            {
                // Q（Q）を右辺から自動判定した型として用意し、Pと東・北・西・南の移動差分（Steps）の「D」に対応する要素の和で初期化する。
                const auto Q = P + Steps[D];
                // 「細道セルの集合（Roads）で、指定値が登録されているか調べる（引数：Q）」に当てはまる場合だけ、続く文を実行する。
                if (Roads.Contains(Q))
                {
                    // 「Dが2より小さい」に当てはまる場合だけ、続く文を実行する。
                    if (D < 2)
                    {
                        ++Edges;
                    }
                }
                // 前の条件が不成立で、さらに「const int32とId = Ownersで、キーに対応する値の参照を探す。未登録ならnullptr（引数：Q）の積」に当てはまる場合だけ、続く文を実行する。
                else if (const int32* Id = Owners.Find(Q))
                {
                    // 同じ細道セルと同じ部屋の接触は、複数の室内セルに接していても1本と数える。
                    // 同一設定を探すためのキー（Key）を符号なし64ビット整数として用意し、uint64(GetTypeHash(P)) <が32より小さいと符号なし32ビット整数へ変換する（引数：空間の識別番号（Id）の参照先の値）のビットORで初期化する。
                    const uint64 Key = (uint64(GetTypeHash(P)) << 32) | uint32(*Id);
                    // 細道セルと部屋の接触を重複なく数える集合（RoomEdges）で、要素または対応関係を末尾へ追加する（引数：同一設定を探すためのキー（Key））。
                    RoomEdges.Add(Key);
                }
            }
        }
        // 接続辺の件数（Edges）と細道セルと部屋の接触を重複なく数える集合（RoomEdges）で、要素数を得るの和から細道セルの集合（Roads）で、要素数を得るを引いた値から部屋の一覧（Rooms）で、要素数を得るを引いた値と1の和を呼び出し元へ返す。
        return Edges + RoomEdges.Num() - Roads.Num() - Rooms.Num() + 1;
    }
}

/** 生成の入口。部屋配置→密集化→全室接続→周回追加→屋内化→床/接続データ化→検査の順。
 * Randomの抽選順は同じSeedの再現性に関わるため、段階の順序を変えないこと。
 */
bool UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(
    // 生成設定（Settings）、int32 Seed（int32 Seed）、FCourtyardPlan& Plan（FCourtyardPlan& Plan）、FString& Error)（FString& Error)）を、それぞれ迷路の生成設定として用意する。元の値を参照し、コピーしない。
    const FCourtyardSettings& Settings, int32 Seed, FCourtyardPlan& Plan, FString& Error)
{
    // CourtyardOrganic内の名前を、この範囲では名前空間名を省略して使えるようにする。
    using namespace CourtyardOrganic;
    // ① 設定から塊の個数を決める。標準12個、許容範囲は9～24個。
    // 必要な広い空間の数（Wanted）を符号付き32ビット整数として用意し、指定した最小値と最大値の範囲へ収める（引数：生成設定（Settings）の広い空間を何個作るかの基準、9、24）で初期化する。
    const int32 Wanted = FMath::Clamp(Settings.StreetCount, 9, 24);
    // 接続条件を緩めずに作り直す。無限ループを防ぐため、全体の再試行は最大24回。
    for (int32 Attempt = 0; Attempt < 24; ++Attempt)
    {
        // 再現可能な乱数生成器（Random）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
        FRandomStream Random(Seed + Attempt * 7919);
        // 生成結果（Plan）を、生成結果を既定状態で用意するに設定する。
        Plan = FCourtyardPlan();
        // Planの各項目を、以下で計算・取得した値へまとめて設定する。
        Plan.Seed = Seed;
        Plan.bOrganicLayout = true;
        Plan.TunnelCellLimit = FMath::Clamp(Settings.MaxTunnelCells, 1, 3);
        // 配置盤面の一辺のセル数（Limit）を符号付き32ビット整数として用意し、端数を切り上げて整数にする（引数：平方根を求める（引数：単精度の実数へ変換する（引数：必要な広い空間の数（Wanted）））と9.fの積）で初期化する。
        const int32 Limit = FMath::CeilToInt(FMath::Sqrt(float(Wanted)) * 9.f);
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として宣言する。
        TMap<FIntPoint, int32> Owners;
        // 細道セルの集合（Roads）を重複しない集合として宣言する。
        TSet<FIntPoint> Roads;
        // ② 重なり・間隔・最寄りの部屋までの距離を検査して、塊をランダム配置する。
        for (int32 Try = 0; Try < 2500 && Plan.Nodes.Num() < Wanted; ++Try)
        {
            // W（W）に再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：5、6）を格納する。H（H）にWを格納する。
            const int32 W = Random.RandRange(5, 6), H = W;
            // 範囲の最小端（Min）を2次元の整数セル座標として用意し、生成結果（Plan）の広い空間の一覧のIsEmpty() ? FIntPoint(0, 0)で初期化する。
            const FIntPoint Min = Plan.Nodes.IsEmpty() ? FIntPoint(0, 0)
                                                       // この関数の役割：2次元の整数セル座標を組み立てる。ここから本体を定義する。
                                                       : FIntPoint(Random.RandRange(0, Limit - W),
                                                             // 再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、配置盤面の一辺のセル数（Limit）からHを引いた値）。
                                                             Random.RandRange(0, Limit - H));
            bool Fits = true;
            // 部屋同士の間隔（セル）（Gap）を符号付き32ビット整数として用意し、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：1、2）で初期化する。
            const int32 Gap = Random.RandRange(1, 2);
            // Nearest（Nearest）を符号付き32ビット整数として用意し、MAX_int32で初期化する。
            int32 Nearest = MAX_int32;
            for (const auto& N : Plan.Nodes)
            {
                // 「「範囲の最小端（Min）のX成分がNの最小端のX成分とNの大きさのX成分の和と部屋同士の間隔（セル）（Gap）の和より小さい」かつ「範囲の最小端（Min）のX成分とWの和と部屋同士の間隔（セル）（Gap）の和がNの最小端のX成分より大きい」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
                if (Min.X < N.Min.X + N.Size.X + Gap && Min.X + W + Gap > N.Min.X &&
                    // この行では「範囲の最小端（Min）のY成分がNの最小端のY成分とNの大きさのY成分の和と部屋同士の間隔（セル）（Gap）の和より小さい」かつ「範囲の最小端（Min）のY成分とHの和と部屋同士の間隔（セル）（Gap）の和がNの最小端のY成分より大きい」を指定し、前後の行の計算・呼び出しへつなげる。
                    Min.Y < N.Min.Y + N.Size.Y + Gap && Min.Y + H + Gap > N.Min.Y)
                {
                    Fits = false;
                    break;
                }
                // DX（DX）を符号付き32ビット整数として用意し、で初期化する。
                const int32 DX =
                    // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Nの最小端のX成分から範囲の最小端（Min）のX成分とWの和を引いた値、範囲の最小端（Min）のX成分からNの最小端のX成分とNの大きさのX成分の和を引いた値））。
                    FMath::Max(0, FMath::Max(N.Min.X - (Min.X + W), Min.X - (N.Min.X + N.Size.X)));
                // DY（DY）を符号付き32ビット整数として用意し、で初期化する。
                const int32 DY =
                    // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Nの最小端のY成分から範囲の最小端（Min）のY成分とHの和を引いた値、範囲の最小端（Min）のY成分からNの最小端のY成分とNの大きさのY成分の和を引いた値））。
                    FMath::Max(0, FMath::Max(N.Min.Y - (Min.Y + H), Min.Y - (N.Min.Y + N.Size.Y)));
                // Nearestを、小さい方の値を選ぶ（引数：Nearest、DXとDYの和）に設定する。
                Nearest = FMath::Min(Nearest, DX + DY);
            }
            // 「「「生成結果（Plan）の広い空間の一覧で、空かを調べる」が成立しない」かつ「Nearestが4より大きい」」に当てはまる場合だけ、続く文を実行する。
            if (!Plan.Nodes.IsEmpty() && Nearest > 4)
            {
                Fits = false;
            }
            if (!Fits)
            {
                continue;
            }
            // N（N）を広い空間1個の情報として宣言する。
            FCourtyardNode N;
            // Nの各項目を、以下で計算・取得した値へまとめて設定する。
            N.Id = Plan.Nodes.Num();
            N.Min = Min;
            N.Size = {W, H};
            N.Kind = ECourtyardCellKind::Street;
            // 外接範囲を全て埋めず、幅2セルの帯を重ねてL字・U字の塊を作る。
            // 回転（Rotation）を符号付き32ビット整数として用意し、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、3）で初期化する。
            const int32 Rotation = Random.RandRange(0, 3);
            // UShape（UShape）を真偽値として用意し、再現可能な乱数生成器（Random）で、0以上1未満の小数をSeedに従って抽選するがの55fより小さいで初期化する。
            const bool UShape = Random.FRand() < .55f;
            for (int32 X = 0; X < W; ++X)
            {
                for (int32 Y = 0; Y < H; ++Y)
                {
                    // 歩ける床の集合（Floor）を真偽値として用意し、「「Yが2より小さい」または「XがWから2を引いた値以上」」または「「UShape」かつ「YがHから2を引いた値以上」」で初期化する。
                    const bool Floor = Y < 2 || X >= W - 2 || (UShape && Y >= H - 2);
                    if (!Floor)
                    {
                        continue;
                    }
                    // ローカル座標またはローカル姿勢（Local）を、括弧内の値を使って2次元の整数セル座標として初期化する。
                    FIntPoint Local(X, Y);
                    for (int32 R = 0; R < Rotation; ++R)
                    {
                        // ローカル座標またはローカル姿勢（Local）を、2次元の整数セル座標を組み立てる（引数：Wから1を引いた値からローカル座標またはローカル姿勢（Local）のY成分を引いた値、ローカル座標またはローカル姿勢（Local）のX成分）に設定する。
                        Local = FIntPoint(W - 1 - Local.Y, Local.X);
                    }
                    // P（P）を右辺から自動判定した型として用意し、範囲の最小端（Min）とローカル座標またはローカル姿勢（Local）の和で初期化する。
                    const auto P = Min + Local;
                    // Nの実床セル一覧で、要素または対応関係を末尾へ追加する（引数：P）。
                    N.FloorCells.Add(P);
                    // 床セルから所属部屋を引く表（Owners）で、要素または対応関係を末尾へ追加する（引数：P、Nの識別番号）。
                    // 所属表は太細を混ぜた床形状が確定してから登録する。
                }
            }
            // 生成結果（Plan）の広い空間の一覧で、要素または対応関係を末尾へ追加する（引数：コピーせず移動できる形で値を渡す（引数：N））。
            N.FloorCells = UCourtyardRoomShapes::MakeHybridRoomFloor(N.FloorCells, Seed, N.Id);
            for (const auto P : N.FloorCells)
            {
                Owners.Add(P, N.Id);
            }
            Plan.Nodes.Add(MoveTemp(N));
        }
        // 「生成結果（Plan）の広い空間の一覧で、要素数を得るが必要な広い空間の数（Wanted）と異なる」に当てはまる場合だけ、続く文を実行する。
        if (Plan.Nodes.Num() != Wanted)
        {
            continue;
        }
        // ③ 部屋の形を保って内側へ寄せ、移動後の床セル→部屋ID対応を作り直す。
        // 部屋の形を保って位置を詰める（引数：生成結果（Plan）の広い空間の一覧、配置盤面の一辺のセル数（Limit）、再現用Seed（Seed））。
        CompactRooms(Plan.Nodes, Limit, Seed);
        Owners.Reset();
        for (const auto& Node : Plan.Nodes)
        {
            for (auto Cell : Node.FloorCells)
            {
                // 床セルから所属部屋を引く表（Owners）で、要素または対応関係を末尾へ追加する（引数：Cell、Nodeの識別番号）。
                Owners.Add(Cell, Node.Id);
            }
        }

        // ④ 全ての部屋ペアを距離＋少量の乱数で並べ、近い接続候補から試す。
        struct FPair
        {
            // A（A）、B（B）、歩行距離（Distance）を、それぞれ符号付き32ビット整数として用意する。
            int32 A, B, Distance;
        };

        // 部屋同士の接続ペア候補（Pairs）を配列として宣言する。
        TArray<FPair> Pairs;
        for (int32 A = 0; A < Wanted; ++A)
        {
            for (int32 B = A + 1; B < Wanted; ++B)
            {
                // D（D）を右辺から自動判定した型として用意し、生成結果（Plan）の広い空間の一覧の「A」に対応する要素の最小端から生成結果（Plan）の広い空間の一覧の「B」に対応する要素の最小端を引いた値で初期化する。
                const auto D = Plan.Nodes[A].Min - Plan.Nodes[B].Min;
                // 部屋同士の接続ペア候補（Pairs）で、要素または対応関係を末尾へ追加する（引数：{A, B, FMath::Abs(D.X) + FMath::Abs(D.Y) + Random.RandRange(0, 3)}）。
                Pairs.Add({A, B, FMath::Abs(D.X) + FMath::Abs(D.Y) + Random.RandRange(0, 3)});
            }
        }
        // 部屋ペアを距離と小さな乱数の評価順へ整列する。同点では元の並び順を保つ。
        Pairs.StableSort(
            // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
            [](const FPair& A, const FPair& B)
            {
                // Aの歩行距離がBの歩行距離より小さいを呼び出し元へ返す。
                return A.Distance < B.Distance;
            });
        // 部屋ごとの接続済みグループID（Groups）を配列として宣言する。
        TArray<int32> Groups;
        for (int32 I = 0; I < Wanted; ++I)
        {
            // 部屋ごとの接続済みグループID（Groups）で、要素または対応関係を末尾へ追加する（引数：I）。
            Groups.Add(I);
        }
        // 部屋ごとの使用済み入口（Used）を配列として宣言する。
        TArray<TArray<FIntPoint>> Used;
        // 部屋ごとの使用済み入口（Used）で、要素数を指定件数にそろえる（引数：必要な広い空間の数（Wanted））。
        Used.SetNum(Wanted);
        // 採用した細道経路（Routes）を配列として宣言する。
        TArray<FRoute> Routes;
        // 採用済み候補の添字一覧（Selected）を重複しない集合として宣言する。
        TSet<int32> Selected;
        // Add（Add）を、周囲の値を使って判定・計算する小さな関数として定義する。
        const auto Add = [&](int32 PairIndex)
        {
            // Pair（Pair）を右辺から自動判定した型として用意し、部屋同士の接続ペア候補（Pairs）の「PairIndex」に対応する要素で初期化する。
            const auto Pair = Pairs[PairIndex];
            // R（R）を細道経路の候補として宣言する。
            FRoute R;
            // 「「FindRoomConnectionRoute(Pair.A,」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!FindRoomConnectionRoute(Pair.A,
                    // この行ではPairのBを指定し、前後の行の計算・呼び出しへつなげる。
                    Pair.B,
                    // この行では生成結果（Plan）の広い空間の一覧を指定し、前後の行の計算・呼び出しへつなげる。
                    Plan.Nodes,
                    // この行では床セルから所属部屋を引く表（Owners）を指定し、前後の行の計算・呼び出しへつなげる。
                    Owners,
                    // この行では細道セルの集合（Roads）を指定し、前後の行の計算・呼び出しへつなげる。
                    Roads,
                    // この行では部屋ごとの使用済み入口（Used）を指定し、前後の行の計算・呼び出しへつなげる。
                    Used,
                    // この行では配置盤面の一辺のセル数（Limit）を指定し、前後の行の計算・呼び出しへつなげる。
                    Limit,
                    // この行では再現可能な乱数生成器（Random）で、0以上、指定上限未満の整数を抽選する（引数：MAX_int32）を指定し、前後の行の計算・呼び出しへつなげる。
                    Random.RandHelper(MAX_int32),
                    // 探索結果をRへ受け取り、2室間の有効な経路が見つからなければ候補の採用を中止する。
                    R))
            {
                return false;
            }
            for (auto P : R.Cells)
            {
                // 細道セルの集合（Roads）で、要素または対応関係を末尾へ追加する（引数：P）。
                Roads.Add(P);
            }
            for (int32 Id : {Pair.A, Pair.B})
            {
                // P（P）を右辺から自動判定した型として用意し、「空間の識別番号（Id）がPairのAと等しい」ならRのセル列の「0」に対応する要素、そうでなければRのセル列で、末尾の要素を取得するで初期化する。
                const auto P = Id == Pair.A ? R.Cells[0] : R.Cells.Last();
                for (auto Step : Steps)
                {
                    // 「「const int32とOwner = OwnersのFind(P + Step); Ownerの積」かつ「Ownerの参照先の値が空間の識別番号（Id）と等しい」」に当てはまる場合だけ、続く文を実行する。
                    if (const int32* Owner = Owners.Find(P + Step); Owner && *Owner == Id)
                    {
                        // 部屋ごとの使用済み入口（Used）の「空間の識別番号（Id）」に対応する要素で、要素または対応関係を末尾へ追加する（引数：Pと隣接セルへ進む差分（Step）の和）。
                        Used[Id].Add(P + Step);
                        break;
                    }
                }
            }
            // 採用済み候補の添字一覧（Selected）で、要素または対応関係を末尾へ追加する（引数：PairIndex）。
            Selected.Add(PairIndex);
            // 採用した細道経路（Routes）で、要素または対応関係を末尾へ追加する（引数：コピーせず移動できる形で値を渡す（引数：R））。
            Routes.Add(MoveTemp(R));
            return true;
        };
        // ⑤ まず未接続グループ同士をつなぎ、全室が到達可能な骨格を作る。
        for (int32 I = 0; I < Pairs.Num(); ++I)
        {
            // P（P）を右辺から自動判定した型として用意し、部屋同士の接続ペア候補（Pairs）の「I」に対応する要素で初期化する。
            const auto P = Pairs[I];
            // 「「部屋ごとの接続済みグループID（Groups）の「PのA」に対応する要素が部屋ごとの接続済みグループID（Groups）の「PのB」に対応する要素と等しい」または「「要素または対応関係を末尾へ追加する（引数：I）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
            if (Groups[P.A] == Groups[P.B] || !Add(I))
            {
                continue;
            }
            // 置き換え前の値（Old）に部屋ごとの接続済みグループID（Groups）の「PのB」に対応する要素を格納する。置き換え先の値（New）に部屋ごとの接続済みグループID（Groups）の「PのA」に対応する要素を格納する。
            const int32 Old = Groups[P.B], New = Groups[P.A];
            for (auto& G : Groups)
            {
                // 「Gが置き換え前の値（Old）と等しい」に当てはまる場合だけ、続く文を実行する。
                if (G == Old)
                {
                    // Gを、置き換え先の値（New）に設定する。
                    G = New;
                }
            }
        }
        bool Connected = true;
        for (auto G : Groups)
        {
            // 全体が接続済みかの判定（Connected）へ、Gが部屋ごとの接続済みグループID（Groups）の「0」に対応する要素と等しいとのビットANDで両方にあるビットだけ残す。
            Connected &= G == Groups[0];
        }
        if (!Connected)
        {
            continue;
        }
        // ⑥ 離れた出入口と戻れる経路を追加する。条件が満たされたペアには余分な道を足さない。
        for (int32 Pass = 0; Pass < 3; ++Pass)
        {
            for (int32 I = 0; I < Pairs.Num(); ++I)
            {
                // 「採用済み候補の添字一覧（Selected）で、指定値が登録されているか調べる（引数：I）」に当てはまる場合だけ、続く文を実行する。
                if (Selected.Contains(I))
                {
                    continue;
                }
                // P（P）を右辺から自動判定した型として用意し、部屋同士の接続ペア候補（Pairs）の「I」に対応する要素で初期化する。
                const auto P = Pairs[I];
                // 「「「部屋ごとの使用済み入口（Used）の「PのA」に対応する要素で、要素数を得るが2以上」かつ「部屋ごとの使用済み入口（Used）の「PのB」に対応する要素で、要素数を得るが2以上」」かつ「採用した細道経路（Routes）で、要素数を得るが必要な広い空間の数（Wanted）と1の和以上」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
                if (Used[P.A].Num() >= 2 && Used[P.B].Num() >= 2 && Routes.Num() >= Wanted + 1 &&
                    // この行では部屋と細道のネットワークの独立周回数を計算する（引数：生成結果（Plan）の広い空間の一覧、床セルから所属部屋を引く表（Owners）、細道セルの集合（Roads））が3以上を指定し、前後の行の計算・呼び出しへつなげる。
                    CountNetworkLoops(Plan.Nodes, Owners, Roads) >= 3)
                {
                    continue;
                }
                // 要素または対応関係を末尾へ追加する（引数：I）。
                Add(I);
            }
        }
        bool AllTwo = true;
        for (const auto& P : Used)
        {
            // 全室に入口が2個以上あるかの判定（AllTwo）へ、Pで、要素数を得るが2以上とのビットANDで両方にあるビットだけ残す。
            AllTwo &= P.Num() >= 2;
        }
        if (!AllTwo)
        {
            continue;
        }
        // 生成結果（Plan）の独立した周回路の件数を、部屋と細道のネットワークの独立周回数を計算する（引数：生成結果（Plan）の広い空間の一覧、床セルから所属部屋を引く表（Owners）、細道セルの集合（Roads））に設定する。
        Plan.LoopCount = CountNetworkLoops(Plan.Nodes, Owners, Roads);
        // 「生成結果（Plan）の独立した周回路の件数が3より小さい」に当てはまる場合だけ、続く文を実行する。
        if (Plan.LoopCount < 3)
        {
            continue;
        }
        // ⑦ 一部の塊を屋内室に変更する。屋内は専用の壁・扉・天井、残りは屋外として描画する。
        int32 IndoorCount = 0;
        for (int32 I = 1; I < Wanted; ++I)
        {
            // 再現可能な乱数生成器（Random）で、0以上1未満の小数をSeedに従って抽選するが指定した最小値と最大値の範囲へ収める（引数：生成設定（Settings）の屋内部屋を追加・選択する確率、0.f、1.f）より小さいという判定を、trueなら1・falseなら0として屋内にする空間の件数（IndoorCount）へ加算する。
            IndoorCount += Random.FRand() < FMath::Clamp(Settings.RoomChance, 0.f, 1.f);
        }
        // 屋内にする空間の件数（IndoorCount）を、FMath::Clamp(に設定する。
        IndoorCount = FMath::Clamp(
            // 屋内化する数を0から設定上限へ収める。上限は最大4室かつ最初の基準空間を除いた件数とする。
            IndoorCount, 0, FMath::Clamp(Settings.MaxRooms, 0, FMath::Min(4, Wanted - 1)));
        // 「「生成設定（Settings）の屋内部屋を追加・選択する確率が0より大きい」かつ「生成設定（Settings）の屋内部屋の上限数が0より大きい」」に当てはまる場合だけ、続く文を実行する。
        if (Settings.RoomChance > 0 && Settings.MaxRooms > 0)
        {
            // 屋内にする空間の件数（IndoorCount）を、大きい方の値を選ぶ（引数：1、屋内にする空間の件数（IndoorCount））に設定する。
            IndoorCount = FMath::Max(1, IndoorCount);
        }
        // 屋内化する候補の部屋ID一覧（IndoorIds）を配列として宣言する。
        TArray<int32> IndoorIds;
        for (int32 I = 1; I < Wanted; ++I)
        {
            // 屋内化する候補の部屋ID一覧（IndoorIds）で、要素または対応関係を末尾へ追加する（引数：I）。
            IndoorIds.Add(I);
        }
        for (int32 I = IndoorIds.Num() - 1; I > 0; --I)
        {
            // 屋内化する候補の部屋ID一覧（IndoorIds）で、指定した2つの添字の要素を交換する（引数：I、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
            IndoorIds.Swap(I, Random.RandRange(0, I));
        }
        // 「生成設定（Settings）の屋内部屋を追加・選択する確率が0より大きい」に当てはまる場合だけ、続く文を実行する。
        if (Settings.RoomChance > 0)
        {
            for (int32 I = 0; I < IndoorCount; ++I)
            {
                // N（N）を右辺から自動判定した型として用意し、生成結果（Plan）のNodes[IndoorIds[I]]で初期化する。元の値を参照し、コピーしない。
                auto& N = Plan.Nodes[IndoorIds[I]];
                // Nの床の種別を、ECourtyardCellKind::Roomに設定する。
                N.Kind = ECourtyardCellKind::Room;
                // R（R）を屋内部屋の床と扉の情報として宣言する。
                FCourtyardRoomArea R;
                // Rの各項目を、以下で計算・取得した値へまとめて設定する。
                R.Min = N.Min;
                R.Size = N.Size;
                R.FloorCells = N.FloorCells;
                for (auto P : N.FloorCells)
                {
                    for (int32 D = 0; D < 4; ++D)
                    {
                        // 「細道セルの集合（Roads）で、指定値が登録されているか調べる（引数：Pと東・北・西・南の移動差分（Steps）の「D」に対応する要素の和）」に当てはまる場合だけ、続く文を実行する。
                        if (Roads.Contains(P + Steps[D]))
                        {
                            // Rの扉を開ける室内セルで、要素または対応関係を末尾へ追加する（引数：P）。
                            R.DoorCells.Add(P);
                            // Rの各扉の外向き方向で、要素または対応関係を末尾へ追加する（引数：D）。
                            R.DoorDirections.Add(D);
                        }
                    }
                }
                // 生成結果（Plan）のRoomAreasで、要素または対応関係を末尾へ追加する（引数：コピーせず移動できる形で値を渡す（引数：R））。
                Plan.RoomAreas.Add(MoveTemp(R));
            }
        }
        // ⑧ 部屋と細道を、描画側が扱う共通のセル配列へ変換する。細道のNodeIdはINDEX_NONE。
        // 座標から床配列の添字を引く索引（CellIndex）をキーで値を引く表として宣言する。
        TMap<FIntPoint, int32> CellIndex;
        for (const auto& N : Plan.Nodes)
        {
            for (auto P : N.FloorCells)
            {
                // C（C）を床セル1個の情報として宣言する。
                FCourtyardCell C;
                // Cの各項目を、以下で計算・取得した値へまとめて設定する。
                C.Position = P;
                C.Kind = N.Kind;
                C.NodeId = N.Id;
                // 座標から床配列の添字を引く索引（CellIndex）で、要素または対応関係を末尾へ追加する（引数：P、生成結果（Plan）のセル列で、要素または対応関係を末尾へ追加する（引数：C））。
                CellIndex.Add(P, Plan.Cells.Add(C));
            }
        }
        // 集合の列挙順に依存しないよう整列し、同じSeedで床・壁の選択順も再現する。
        // 座標順に並べる細道セル配列（RoadArray）を右辺から自動判定した型として用意し、細道セルの集合（Roads）で、集合の内容を配列へ取り出すで初期化する。
        auto RoadArray = Roads.Array();
        // 集合から取り出した細道を座標順に並べ、集合の列挙順に描画結果が左右されないようにする。
        RoadArray.Sort(
            // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
            [](FIntPoint A, FIntPoint B)
            {
                // 「AのX成分がBのX成分と等しい」ならAのY成分がBのY成分より小さい、そうでなければAのX成分がBのX成分より小さいを呼び出し元へ返す。
                return A.X == B.X ? A.Y < B.Y : A.X < B.X;
            });
        for (auto P : RoadArray)
        {
            // C（C）を床セル1個の情報として宣言する。
            FCourtyardCell C;
            // Cのセル座標を、Pに設定する。
            C.Position = P;
            // 座標から床配列の添字を引く索引（CellIndex）で、要素または対応関係を末尾へ追加する（引数：P、生成結果（Plan）のセル列で、要素または対応関係を末尾へ追加する（引数：C））。
            CellIndex.Add(P, Plan.Cells.Add(C));
        }
        for (const auto& R : Routes)
        {
            // L（L）を2室間の接続情報として宣言する。
            FCourtyardLink L;
            // Lの各項目を、以下で計算・取得した値へまとめて設定する。
            L.From = R.From;
            L.To = R.To;
            L.Cells = R.Cells;
            L.bAlley = true;
            // ⑨ 短い・直線・分岐なし・両端が対象の部屋、の全条件を満たす接続だけトンネル化する。
            // 直線トンネルの条件を満たすかの判定（Straight）を真偽値として用意し、Lのセル列で、要素数を得るが生成結果（Plan）の採用する直線トンネルの最長セル数以下で初期化する。
            bool Straight = L.Cells.Num() <= Plan.TunnelCellLimit;
            // 隣接セルへ進む差分（Step）を2次元の整数セル座標として用意し、「Lのセル列で、要素数を得るが1より大きい」ならLのセル列の「1」に対応する要素からLのセル列の「0」に対応する要素を引いた値、そうでなければFIntPoint::ZeroValueで初期化する。
            FIntPoint Step = L.Cells.Num() > 1 ? L.Cells[1] - L.Cells[0] : FIntPoint::ZeroValue;
            // 「隣接セルへ進む差分（Step）がFIntPoint::ZeroValueと等しい」に当てはまる場合だけ、続く文を実行する。
            if (Step == FIntPoint::ZeroValue)
            {
                for (auto D : Steps)
                {
                    // 「「const int32とId = OwnersのFind(L.Cells[0] - D); Idの積」かつ「空間の識別番号（Id）の参照先の値がLの接続元IDと等しい」」に当てはまる場合だけ、続く文を実行する。
                    if (const int32* Id = Owners.Find(L.Cells[0] - D); Id && *Id == L.From)
                    {
                        // 隣接セルへ進む差分（Step）を、Dに設定する。
                        Step = D;
                        break;
                    }
                }
            }
            int32 D = 0;
            for (; D < 4 && Steps[D] != Step; ++D)
            {
            }
            // 直線トンネルの条件を満たすかの判定（Straight）へ、Dが4より小さいとのビットANDで両方にあるビットだけ残す。
            Straight &= D < 4;
            for (int32 I = 0; I < L.Cells.Num() && Straight; ++I)
            {
                // P（P）を右辺から自動判定した型として用意し、Lのセル列の「I」に対応する要素で初期化する。
                const auto P = L.Cells[I];
                // 直線トンネルの条件を満たすかの判定（Straight）へ、「Iが0と等しい」または「PがLのセル列の「Iから1を引いた値」に対応する要素と隣接セルへ進む差分（Step）の和と等しい」とのビットANDで両方にあるビットだけ残す。
                Straight &= I == 0 || P == L.Cells[I - 1] + Step;
                for (int32 J = 0; J < 4; ++J)
                {
                    // 「「JがDと異なる」かつ「JがDと2の和を4で割った余りと異なる」」に当てはまる場合だけ、続く文を実行する。
                    if (J != D && J != (D + 2) % 4)
                    {
                        // 直線トンネルの条件を満たすかの判定（Straight）へ、「座標から床配列の添字を引く索引（CellIndex）で、指定値が登録されているか調べる（引数：Pと東・北・西・南の移動差分（Steps）の「J」に対応する要素の和）」が成立しないとのビットANDで両方にあるビットだけ残す。
                        Straight &= !CellIndex.Contains(P + Steps[J]);
                    }
                }
                // 直線トンネルの条件を満たすかの判定（Straight）へ、生成結果（Plan）のセル列の「座標から床配列の添字を引く索引（CellIndex）で、登録済みであることを前提にキーの値を取得する（引数：P）」に対応する要素の床の種別がECourtyardCellKind::Streetと等しいとのビットANDで両方にあるビットだけ残す。
                Straight &= Plan.Cells[CellIndex.FindChecked(P)].Kind == ECourtyardCellKind::Street;
            }
            // 接続元の部屋ID（From）を符号付き32ビット整数として用意し、床セルから所属部屋を引く表（Owners）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：Lのセル列の「0」に対応する要素から隣接セルへ進む差分（Step）を引いた値）で初期化する。参照先がない場合もあるポインターとして扱う。
            const int32* From = Owners.Find(L.Cells[0] - Step);
            // 接続先の部屋ID（To）を符号付き32ビット整数として用意し、床セルから所属部屋を引く表（Owners）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：Lのセル列で、末尾の要素を取得すると隣接セルへ進む差分（Step）の和）で初期化する。参照先がない場合もあるポインターとして扱う。
            const int32* To = Owners.Find(L.Cells.Last() + Step);
            // 直線トンネルの条件を満たすかの判定（Straight）へ、「「「接続元の部屋ID（From）」かつ「接続先の部屋ID（To）」」かつ「接続元の部屋ID（From）の参照先の値がLの接続元IDと等しい」」かつ「接続先の部屋ID（To）の参照先の値がLの接続先IDと等しい」とのビットANDで両方にあるビットだけ残す。
            Straight &= From && To && *From == L.From && *To == L.To;
            if (Straight)
            {
                // Lの各項目を、以下で計算・取得した値へまとめて設定する。
                L.bAlley = false;
                L.Direction = D;
                L.Axis = D % 2;
                for (auto P : L.Cells)
                {
                    // C（C）を右辺から自動判定した型として用意し、生成結果（Plan）のセル列の「座標から床配列の添字を引く索引（CellIndex）で、登録済みであることを前提にキーの値を取得する（引数：P）」に対応する要素で初期化する。元の値を参照し、コピーしない。
                    auto& C = Plan.Cells[CellIndex.FindChecked(P)];
                    // Cの各項目を、以下で計算・取得した値へまとめて設定する。
                    C.Kind = ECourtyardCellKind::Tunnel;
                    C.LinkId = Plan.Links.Num();
                    C.Axis = L.Axis;
                }
            }
            // 生成結果（Plan）のLinksで、要素または対応関係を末尾へ追加する（引数：コピーせず移動できる形で値を渡す（引数：L））。
            Plan.Links.Add(MoveTemp(L));
        }
        // ⑩ 幅優先探索で歩行距離を求め、遠い屋外の塊の床を仮ゴールにする。
        // 導入通路がある場合は、その入口に合わせて描画前に再計算される。
        // 生成結果（Plan）のStartCellを、生成結果（Plan）の広い空間の一覧の「0」に対応する要素の実床セル一覧の「0」に対応する要素に設定する。
        Plan.StartCell = Plan.Nodes[0].FloorCells[0];
        // 歩行距離（Distance）をキーで値を引く表として宣言する。
        TMap<FIntPoint, int32> Distance;
        // 歩行距離（Distance）で、要素または対応関係を末尾へ追加する（引数：生成結果（Plan）のStartCell、0）。
        Distance.Add(Plan.StartCell, 0);
        // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
        TArray<FIntPoint> Queue{Plan.StartCell};
        // 生成結果（Plan）のGoalCellを、生成結果（Plan）のStartCellに設定する。
        Plan.GoalCell = Plan.StartCell;
        for (int32 I = 0; I < Queue.Num(); ++I)
        {
            for (auto Step : Steps)
            {
                // Q（Q）を右辺から自動判定した型として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素と隣接セルへ進む差分（Step）の和で初期化する。
                const auto Q = Queue[I] + Step;
                // 「「「座標から床配列の添字を引く索引（CellIndex）で、指定値が登録されているか調べる（引数：Q）」が成立しない」または「歩行距離（Distance）で、指定値が登録されているか調べる（引数：Q）」」に当てはまる場合だけ、続く文を実行する。
                if (!CellIndex.Contains(Q) || Distance.Contains(Q))
                {
                    continue;
                }
                // Dist（Dist）を符号付き32ビット整数として用意し、歩行距離（Distance）で、登録済みであることを前提にキーの値を取得する（引数：幅優先探索の待ち行列（Queue）の「I」に対応する要素）と1の和で初期化する。
                const int32 Dist = Distance.FindChecked(Queue[I]) + 1;
                // 歩行距離（Distance）で、要素または対応関係を末尾へ追加する（引数：Q、Dist）。
                Distance.Add(Q, Dist);
                // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：Q）。
                Queue.Add(Q);
                // C（C）を右辺から自動判定した型として用意し、生成結果（Plan）のセル列の「座標から床配列の添字を引く索引（CellIndex）で、登録済みであることを前提にキーの値を取得する（引数：Q）」に対応する要素で初期化する。元の値を参照し、コピーしない。
                const auto& C = Plan.Cells[CellIndex.FindChecked(Q)];
                // 「「Cの床の種別がECourtyardCellKind::Streetと等しい」かつ「Cの所属部屋IDがINDEX_NONE（未指定を表す-1）と異なる」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
                if (C.Kind == ECourtyardCellKind::Street && C.NodeId != INDEX_NONE &&
                    // この行ではDistが歩行距離（Distance）で、登録済みであることを前提にキーの値を取得する（引数：生成結果（Plan）のGoalCell）より大きいを指定し、前後の行の計算・呼び出しへつなげる。
                    Dist > Distance.FindChecked(Plan.GoalCell))
                {
                    // 生成結果（Plan）のGoalCellを、Qに設定する。
                    Plan.GoalCell = Q;
                }
            }
        }
        // 全床の接続・離れた出入口・細道の末端・分岐・周回を検査してから採用する。
        // 「部屋と細道の接続・入口・分岐・周回条件を検査する（引数：生成結果（Plan）、失敗理由（Error））」に当てはまる場合だけ、続く文を実行する。
        if (ValidateOrganicCourtyardPlan(Plan, Error))
        {
            return true;
        }
    }
    // 失敗理由（Error）を、"Could not build a compact connected room/alley network within the retry limit"というUE文字列に設定する。
    Error = TEXT("Could not build a compact connected room/alley network within the retry limit");
    return false;
}

/** 配置データだけで判定できる条件を検査する。
 * 実際のメッシュの衝突・床の支持は描画後にISM生成側で別途検査する。
 * ここでの末端検査は細道が対象であり、L/U字の部屋内部の張り出しは許容する。
 */
bool UCourtyardLayoutLibrary::ValidateOrganicCourtyardPlan(
    // 生成結果（Plan）、FString& Error)（FString& Error)）を、それぞれ迷路全体の生成結果として用意する。元の値を参照し、コピーしない。
    const FCourtyardPlan& Plan, FString& Error)
{
    // CourtyardOrganic内の名前を、この範囲では名前空間名を省略して使えるようにする。
    using namespace CourtyardOrganic;
    // Fail（Fail）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Fail = [&](const TCHAR* Text)
    {
        // 失敗理由（Error）を、Textに設定する。
        Error = Text;
        return false;
    };
    Error.Reset();
    // 「「生成結果（Plan）の広い空間の一覧で、要素数を得るが3より小さい」または「生成結果（Plan）のセル列で、空かを調べる」」に当てはまる場合だけ、続く文を実行する。
    if (Plan.Nodes.Num() < 3 || Plan.Cells.IsEmpty())
    {
        // エラーを記録して失敗を返す（引数："Organic layout has no rooms"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Organic layout has no rooms"));
    }
    // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として宣言する。
    TMap<FIntPoint, int32> Owners;
    // 全床セルの集合（All）、細道セルの集合（Roads）を、それぞれ重複しない集合として用意する。
    TSet<FIntPoint> All, Roads;
    for (const auto& C : Plan.Cells)
    {
        // 「全床セルの集合（All）で、指定値が登録されているか調べる（引数：Cのセル座標）」に当てはまる場合だけ、続く文を実行する。
        if (All.Contains(C.Position))
        {
            // エラーを記録して失敗を返す（引数："Duplicate organic floor"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Duplicate organic floor"));
        }
        // 全床セルの集合（All）で、要素または対応関係を末尾へ追加する（引数：Cのセル座標）。
        All.Add(C.Position);
        // 「Cの所属部屋IDがINDEX_NONE（未指定を表す-1）と等しい」に当てはまる場合だけ、続く文を実行する。
        if (C.NodeId == INDEX_NONE)
        {
            // 細道セルの集合（Roads）で、要素または対応関係を末尾へ追加する（引数：Cのセル座標）。
            Roads.Add(C.Position);
        }
    }
    for (int32 I = 0; I < Plan.Nodes.Num(); ++I)
    {
        // N（N）を右辺から自動判定した型として用意し、生成結果（Plan）の広い空間の一覧の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& N = Plan.Nodes[I];
        // 「「Nの識別番号がIと異なる」または「Nの実床セル一覧で、空かを調べる」」に当てはまる場合だけ、続く文を実行する。
        if (N.Id != I || N.FloorCells.IsEmpty())
        {
            // エラーを記録して失敗を返す（引数："Invalid organic room"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Invalid organic room"));
        }
        // Exits（Exits）を重複しない集合として宣言する。
        TSet<FIntPoint> Exits;
        for (auto P : N.FloorCells)
        {
            // 「「「全床セルの集合（All）で、指定値が登録されているか調べる（引数：P）」が成立しない」または「床セルから所属部屋を引く表（Owners）で、指定値が登録されているか調べる（引数：P）」」に当てはまる場合だけ、続く文を実行する。
            if (!All.Contains(P) || Owners.Contains(P))
            {
                // エラーを記録して失敗を返す（引数："Overlapping or missing organic room floor"というUE文字列）を呼び出し元へ返す。
                return Fail(TEXT("Overlapping or missing organic room floor"));
            }
            // 床セルから所属部屋を引く表（Owners）で、要素または対応関係を末尾へ追加する（引数：P、I）。
            Owners.Add(P, I);
            for (auto Step : Steps)
            {
                // 「細道セルの集合（Roads）で、指定値が登録されているか調べる（引数：Pと隣接セルへ進む差分（Step）の和）」に当てはまる場合だけ、続く文を実行する。
                if (Roads.Contains(P + Step))
                {
                    // Exitsで、要素または対応関係を末尾へ追加する（引数：P）。
                    Exits.Add(P);
                }
            }
        }
        bool Separate = false;
        for (auto A : Exits)
        {
            for (auto B : Exits)
            {
                // 入口の間隔が確保できるかの判定（Separate）へ、符号を取り除いた絶対値を求める（引数：AのX成分からBのX成分を引いた値）と符号を取り除いた絶対値を求める（引数：AのY成分からBのY成分を引いた値）の和が2以上とのビットORで該当するビットを残す。
                Separate |= FMath::Abs(A.X - B.X) + FMath::Abs(A.Y - B.Y) >= 2;
            }
        }
        if (!Separate)
        {
            // エラーを記録して失敗を返す（引数："Organic room needs two separate entrances"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Organic room needs two separate entrances"));
        }
    }
    // 「「「全床セルの集合（All）で、指定値が登録されているか調べる（引数：生成結果（Plan）のStartCell）」が成立しない」または「「全床セルの集合（All）で、指定値が登録されているか調べる（引数：生成結果（Plan）のGoalCell）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
    if (!All.Contains(Plan.StartCell) || !All.Contains(Plan.GoalCell))
    {
        // エラーを記録して失敗を返す（引数："Missing organic start/goal"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Missing organic start/goal"));
    }
    // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する。
    TSet<FIntPoint> Seen{Plan.StartCell};
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
    TArray<FIntPoint> Queue{Plan.StartCell};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (auto Step : Steps)
        {
            // Q（Q）を右辺から自動判定した型として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素と隣接セルへ進む差分（Step）の和で初期化する。
            const auto Q = Queue[I] + Step;
            // 「「全床セルの集合（All）で、指定値が登録されているか調べる（引数：Q）」かつ「「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：Q）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
            if (All.Contains(Q) && !Seen.Contains(Q))
            {
                // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する（引数：Q）。
                Seen.Add(Q);
                // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：Q）。
                Queue.Add(Q);
            }
        }
    }
    // 「探索済みの集合（Seen）で、要素数を得るが全床セルの集合（All）で、要素数を得ると異なる」に当てはまる場合だけ、続く文を実行する。
    if (Seen.Num() != All.Num())
    {
        // エラーを記録して失敗を返す（引数："Disconnected organic floor"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Disconnected organic floor"));
    }
    int32 Branches = 0;
    for (auto P : Roads)
    {
        int32 Degree = 0;
        for (auto Step : Steps)
        {
            // 全床セルの集合（All）で、指定値が登録されているか調べる（引数：Pと隣接セルへ進む差分（Step）の和）という判定を、trueなら1・falseなら0としてDegreeへ加算する。
            Degree += All.Contains(P + Step);
        }
        // 「Degreeが2より小さい」に当てはまる場合だけ、続く文を実行する。
        if (Degree < 2)
        {
            // エラーを記録して失敗を返す（引数："Dead-end organic alley"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Dead-end organic alley"));
        }
        // Degreeが3以上という判定を、trueなら1・falseなら0として分岐の件数（Branches）へ加算する。
        Branches += Degree >= 3;
    }
    // 「分岐の件数（Branches）が0と等しい」に当てはまる場合だけ、続く文を実行する。
    if (Branches == 0)
    {
        // エラーを記録して失敗を返す（引数："Organic layout needs an alley branch"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Organic layout needs an alley branch"));
    }
    // 「部屋と細道のネットワークの独立周回数を計算する（引数：生成結果（Plan）の広い空間の一覧、床セルから所属部屋を引く表（Owners）、細道セルの集合（Roads））が3より小さい」に当てはまる場合だけ、続く文を実行する。
    if (CountNetworkLoops(Plan.Nodes, Owners, Roads) < 3)
    {
        // エラーを記録して失敗を返す（引数："Organic layout needs three return loops"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Organic layout needs three return loops"));
    }
    for (int32 I = 0; I < Plan.Links.Num(); ++I)
    {
        // L（L）を右辺から自動判定した型として用意し、生成結果（Plan）のLinksの「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& L = Plan.Links[I];
        // 「「「「生成結果（Plan）の広い空間の一覧で、指定値が有効な添字か調べる（引数：Lの接続元ID）」が成立しない」または「「生成結果（Plan）の広い空間の一覧で、指定値が有効な添字か調べる（引数：Lの接続先ID）」が成立しない」」または「Lのセル列で、空かを調べる」」に当てはまる場合だけ、続く文を実行する。
        if (!Plan.Nodes.IsValidIndex(L.From) || !Plan.Nodes.IsValidIndex(L.To) || L.Cells.IsEmpty())
        {
            // エラーを記録して失敗を返す（引数："Invalid organic connection"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Invalid organic connection"));
        }
        for (int32 J = 0; J < L.Cells.Num(); ++J)
        {
            // 「「細道セルの集合（Roads）で、指定値が登録されているか調べる（引数：Lのセル列の「J」に対応する要素）」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!Roads.Contains(L.Cells[J]))
            {
                // エラーを記録して失敗を返す（引数："Connection leaves alley floor"というUE文字列）を呼び出し元へ返す。
                return Fail(TEXT("Connection leaves alley floor"));
            }
            // 「「J」かつ「FMath::Abs(L.Cells[J].X - L.Cells[J - 1].X) +」」に当てはまる場合だけ、続く文を実行する。
            if (J && FMath::Abs(L.Cells[J].X - L.Cells[J - 1].X) +
                             // 経路上の連続セル間のY差の絶対値をX差へ足し、上下左右の隣接距離を調べる。
                             FMath::Abs(L.Cells[J].Y - L.Cells[J - 1].Y) !=
                         // 連続セル間は1マス移動である必要がある。合計距離が1でなければ経路の不連続と判定する。
                         1)
            {
                // エラーを記録して失敗を返す（引数："Discontinuous organic alley"というUE文字列）を呼び出し元へ返す。
                return Fail(TEXT("Discontinuous organic alley"));
            }
        }
        // 「「Lの曲がりや分岐を許す細道かの判定」が成立しない」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
        if (!L.bAlley &&
            // 直線トンネルが設定より長い、または方向番号が0〜3の範囲外なら不正な接続とする。
            (L.Cells.Num() > Plan.TunnelCellLimit || L.Direction < 0 || L.Direction > 3))
        {
            // エラーを記録して失敗を返す（引数："Invalid short tunnel"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Invalid short tunnel"));
        }
    }
    return true;
}
