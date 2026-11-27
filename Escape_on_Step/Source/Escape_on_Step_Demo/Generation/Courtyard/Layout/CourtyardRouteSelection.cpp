#include "Generation/Courtyard/Layout/CourtyardLayout.h"

namespace CourtyardRouteSelection
{
    // const FIntPoint Steps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    struct FEvaluation
    {
        bool bValid = false;
        // 候補の評価値（Score）を符号付き32ビット整数として用意し、MAX_int32で初期化する。
        int32 Score = MAX_int32;
        // 部屋ごとの接続数（Degrees）を配列として宣言する。
        TArray<int32> Degrees;
    };

    // この関数の役割：接続口が覆う自由端をビットで求める。ここから本体を定義する。
    uint8 FreeEnds(const FCourtyardNode& Node, FIntPoint P)
    {
        uint8 Mask = 0;
        // 「Nodeの長方形区画一覧で、要素数を得るが2より小さい」に当てはまる場合だけ、続く文を実行する。
        if (Node.Sections.Num() < 2)
            // Maskを呼び出し元へ返す。
            return Mask;
        for (int32 End = 0; End < 2; ++End)
        {
            // I（I）に「End」ならNodeの長方形区画一覧で、要素数を得るから1を引いた値、そうでなければ0を格納する。J（J）に「End」ならIから1を引いた値、そうでなければ1を格納する。
            const int32 I = End ? Node.Sections.Num() - 1 : 0, J = End ? I - 1 : 1;
            // R（R）を右辺から自動判定した型として用意し、Nodeの長方形区画一覧の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& R = Node.Sections[I];
            // 隣とつながる側の区画（Join）を右辺から自動判定した型として用意し、Nodeの長方形区画一覧の「J」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& Join = Node.Sections[J];
            // 「「「PのX成分がRの最小端のX成分より小さい」または「PのY成分がRの最小端のY成分より小さい」」または「PのX成分がRの最小端のX成分とRの大きさのX成分の和以上」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (P.X < R.Min.X || P.Y < R.Min.Y || P.X >= R.Min.X + R.Size.X ||
                // この行ではPのY成分がRの最小端のY成分とRの大きさのY成分の和以上を指定し、前後の行の計算・呼び出しへつなげる。
                P.Y >= R.Min.Y + R.Size.Y)
                continue;
            // X（X）を真偽値として用意し、Rの大きさのX成分がRの大きさのY成分より大きいで初期化する。
            const bool X = R.Size.X > R.Size.Y;
            // T（T）に「X」ならPのX成分からRの最小端のX成分を引いた値、そうでなければPのY成分からRの最小端のY成分を引いた値を格納する。長さ（Length）に「X」ならRの大きさのX成分、そうでなければRの大きさのY成分を格納する。
            const int32 T = X ? P.X - R.Min.X : P.Y - R.Min.Y, Length = X ? R.Size.X : R.Size.Y,
                        // 幅（Width）を、「X」ならRの大きさのY成分、そうでなければRの大きさのX成分に設定する。
                        Width = X ? R.Size.Y : R.Size.X;
            // 区画の長手方向の手前側で接続しているか（JoinedAtStart）を真偽値として用意し、「X」なら隣とつながる側の区画（Join）の最小端のX成分がRの最小端のX成分以下、そうでなければ隣とつながる側の区画（Join）の最小端のY成分がRの最小端のY成分以下で初期化する。
            const bool JoinedAtStart = X ? Join.Min.X <= R.Min.X : Join.Min.Y <= R.Min.Y;
            // 「「区画の長手方向の手前側で接続しているか（JoinedAtStart）」ならTが長さ（Length）から幅（Width）を引いた値以上、そうでなければTが幅（Width）より小さい」に当てはまる場合だけ、続く文を実行する。
            if (JoinedAtStart ? T >= Length - Width : T < Width)
                // Maskへ、1 <がEndより小さいとのビットORで該当するビットを残す。
                Mask |= 1 << End;
        }
        // Maskを呼び出し元へ返す。
        return Mask;
    }

    // この関数の役割：床の上だけを通る直線で見通せるか調べる。ここから本体を定義する。
    bool Visible(FIntPoint A, FIntPoint B, const TSet<FIntPoint>& Floor)
    {
        // Samples（Samples）を符号付き32ビット整数として用意し、大きい方の値を選ぶ（引数：符号を取り除いた絶対値を求める（引数：AのX成分からBのX成分を引いた値）、符号を取り除いた絶対値を求める（引数：AのY成分からBのY成分を引いた値））と4の積で初期化する。
        const int32 Samples = FMath::Max(FMath::Abs(A.X - B.X), FMath::Abs(A.Y - B.Y)) * 4;
        for (int32 I = 0; I <= Samples; ++I)
        {
            // T（T）を単精度の実数として用意し、「Samples」なら単精度の実数へ変換する（引数：I）をSamplesで割った値、そうでなければ0で初期化する。
            const float T = Samples ? float(I) / Samples : 0;
            // P（P）を右辺から自動判定した型として用意し、で初期化する。
            const auto P =
                // 2地点のX座標を割合Tで線形補間し、0.5を足して切り捨てることで最寄りの整数セルへ直す。
                FIntPoint(FMath::FloorToInt(FMath::Lerp(float(A.X), float(B.X), T) + .5f),
                    // 同じ補間と丸めをY座標にも行い、直線の途中が歩ける床上にあるか調べるセルを求める。
                    FMath::FloorToInt(FMath::Lerp(float(A.Y), float(B.Y), T) + .5f));
            // 「「歩ける床の集合（Floor）で、指定値が登録されているか調べる（引数：P）」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!Floor.Contains(P))
                return false;
        }
        return true;
    }
}

// この関数の役割：見通しのよい短い周回を減らし、戻り道を残して接続を調整する。ここから本体を定義する。
void UCourtyardLayoutLibrary::ImproveMazeConnections(const TArray<FCourtyardNode>& Nodes,
    // 接続や入口の候補（Candidates）を配列として引数で受け取る。元の値を参照し、コピーしない。
    const TArray<FCourtyardLink>& Candidates,
    // 採用済み候補の添字一覧（Selected）を配列として引数で受け取る。元の値を参照し、コピーしない。
    TArray<int32>& Selected,
    // 部屋ごとの接続数（Degrees）を配列として引数で受け取る。元の値を参照し、コピーしない。
    TArray<int32>& Degrees,
    // 生成設定（Settings）を迷路の生成設定として引数で受け取る。元の値を参照し、コピーしない。
    const FCourtyardSettings& Settings,
    // 再現用Seed（Seed）を符号付き32ビット整数として引数で受け取る。
    int32 Seed)
{
    // CourtyardRouteSelection内の名前を、この範囲では名前空間名を省略して使えるようにする。
    using namespace CourtyardRouteSelection;
    // 「「「「生成設定（Settings）の旧方式で見通しのよすぎる接続を減らす設定」が成立しない」または「広い空間の一覧（Nodes）で、要素数を得るが4より小さい」」または「接続や入口の候補（Candidates）で、空かを調べる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (!Settings.bDisorientingRoutes || Nodes.Num() < 4 || Candidates.IsEmpty() ||
        // この行では採用済み候補の添字一覧（Selected）で、要素数を得るが広い空間の一覧（Nodes）で、要素数を得ると2の和より小さいを指定し、前後の行の計算・呼び出しへつなげる。
        Selected.Num() < Nodes.Num() + 2)
        return;
    // 対象の件数（Count）を符号付き32ビット整数として用意し、広い空間の一覧（Nodes）で、要素数を得るで初期化する。
    const int32 Count = Nodes.Num();
    // Floors（Floors）を配列として宣言する。
    TArray<TSet<FIntPoint>> Floors;
    // Floorsで、要素数を指定件数にそろえる（引数：対象の件数（Count））。
    Floors.SetNum(Count);
    for (int32 I = 0; I < Count; ++I)
        for (auto P : Nodes[I].FloorCells)
            // Floorsの「I」に対応する要素で、要素または対応関係を末尾へ追加する（引数：P）。
            Floors[I].Add(P);
    // 維持する必要がある出口のビット情報（Required）を配列として宣言する。
    TArray<uint8> Required;
    // 維持する必要がある出口のビット情報（Required）で、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
    Required.Init(0, Count);
    for (int32 I : Selected)
    {
        // 「「接続や入口の候補（Candidates）で、指定値が有効な添字か調べる（引数：I）」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!Candidates.IsValidIndex(I))
            return;
        // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& C = Candidates[I];
        // 「「「「広い空間の一覧（Nodes）で、指定値が有効な添字か調べる（引数：Cの接続元ID）」が成立しない」または「「広い空間の一覧（Nodes）で、指定値が有効な添字か調べる（引数：Cの接続先ID）」が成立しない」」または「Cのセル列で、空かを調べる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (!Nodes.IsValidIndex(C.From) || !Nodes.IsValidIndex(C.To) || C.Cells.IsEmpty() ||
            // この行では「Cの方向番号が0より小さい」または「Cの方向番号が3より大きい」を指定し、前後の行の計算・呼び出しへつなげる。
            C.Direction < 0 || C.Direction > 3)
            return;
        // 維持する必要がある出口のビット情報（Required）の「Cの接続元ID」に対応する要素へ、接続口が覆う自由端をビットで求める（引数：広い空間の一覧（Nodes）の「Cの接続元ID」に対応する要素、Cのセル列の「0」に対応する要素から東・北・西・南の移動差分（Steps）の「Cの方向番号」に対応する要素を引いた値）とのビットORで該当するビットを残す。
        Required[C.From] |= FreeEnds(Nodes[C.From], C.Cells[0] - Steps[C.Direction]);
        // 維持する必要がある出口のビット情報（Required）の「Cの接続先ID」に対応する要素へ、接続口が覆う自由端をビットで求める（引数：広い空間の一覧（Nodes）の「Cの接続先ID」に対応する要素、Cのセル列で、末尾の要素を取得すると東・北・西・南の移動差分（Steps）の「Cの方向番号」に対応する要素の和）とのビットORで該当するビットを残す。
        Required[C.To] |= FreeEnds(Nodes[C.To], C.Cells.Last() + Steps[C.Direction]);
    }
    // Evaluate（Evaluate）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Evaluate = [&](const TArray<int32>& Edges)
    {
        // この検査や計算の結果（Result）をFEvaluationとして宣言する。
        FEvaluation Result;
        // 「接続辺の件数（Edges）で、要素数を得るから対象の件数（Count）を引いた値と1の和が3より小さい」に当てはまる場合だけ、続く文を実行する。
        if (Edges.Num() - Count + 1 < 3)
            // この検査や計算の結果（Result）を呼び出し元へ返す。
            return Result;
        // 接続する辺の一覧（Adjacency）を配列として宣言する。
        TArray<TArray<int32>> Adjacency;
        // 接続する辺の一覧（Adjacency）で、要素数を指定件数にそろえる（引数：対象の件数（Count））。
        Adjacency.SetNum(Count);
        // 部屋の出入口（Ports）を配列として宣言する。
        TArray<TArray<FIntPoint>> Ports;
        // 部屋の出入口（Ports）で、要素数を指定件数にそろえる（引数：対象の件数（Count））。
        Ports.SetNum(Count);
        // 今回の接続で覆えた自由端のビット情報（Covered）を配列として宣言する。
        TArray<uint8> Covered;
        // 今回の接続で覆えた自由端のビット情報（Covered）で、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
        Covered.Init(0, Count);
        // この検査や計算の結果（Result）の部屋ごとの接続数で、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
        Result.Degrees.Init(0, Count);
        for (int32 I : Edges)
        {
            // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& C = Candidates[I];
            // 接続する辺の一覧（Adjacency）の「Cの接続元ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：I）。
            Adjacency[C.From].Add(I);
            // 接続する辺の一覧（Adjacency）の「Cの接続先ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：I）。
            Adjacency[C.To].Add(I);
            // この検査や計算の結果（Result）の部屋ごとの接続数の「Cの接続元ID」に対応する要素を1増やす。
            ++Result.Degrees[C.From];
            // この検査や計算の結果（Result）の部屋ごとの接続数の「Cの接続先ID」に対応する要素を1増やす。
            ++Result.Degrees[C.To];
            // A（A）にCのセル列の「0」に対応する要素から東・北・西・南の移動差分（Steps）の「Cの方向番号」に対応する要素を引いた値を格納する。B（B）にCのセル列で、末尾の要素を取得すると東・北・西・南の移動差分（Steps）の「Cの方向番号」に対応する要素の和を格納する。
            const auto A = C.Cells[0] - Steps[C.Direction], B = C.Cells.Last() + Steps[C.Direction];
            // 部屋の出入口（Ports）の「Cの接続元ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：A）。
            Ports[C.From].Add(A);
            // 部屋の出入口（Ports）の「Cの接続先ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：B）。
            Ports[C.To].Add(B);
            // 今回の接続で覆えた自由端のビット情報（Covered）の「Cの接続元ID」に対応する要素へ、接続口が覆う自由端をビットで求める（引数：広い空間の一覧（Nodes）の「Cの接続元ID」に対応する要素、A）とのビットORで該当するビットを残す。
            Covered[C.From] |= FreeEnds(Nodes[C.From], A);
            // 今回の接続で覆えた自由端のビット情報（Covered）の「Cの接続先ID」に対応する要素へ、接続口が覆う自由端をビットで求める（引数：広い空間の一覧（Nodes）の「Cの接続先ID」に対応する要素、B）とのビットORで該当するビットを残す。
            Covered[C.To] |= FreeEnds(Nodes[C.To], B);
        }
        int32 Branches = 0;
        for (int32 I = 0; I < Count; ++I)
        {
            // 「「この検査や計算の結果（Result）の部屋ごとの接続数の「I」に対応する要素が2より小さい」または「今回の接続で覆えた自由端のビット情報（Covered）の「I」に対応する要素と維持する必要がある出口のビット情報（Required）の「I」に対応する要素のビットANDが維持する必要がある出口のビット情報（Required）の「I」に対応する要素と異なる」」に当てはまる場合だけ、続く文を実行する。
            if (Result.Degrees[I] < 2 || (Covered[I] & Required[I]) != Required[I])
                // この検査や計算の結果（Result）を呼び出し元へ返す。
                return Result;
            // この検査や計算の結果（Result）の部屋ごとの接続数の「I」に対応する要素が3以上という判定を、trueなら1・falseなら0として分岐の件数（Branches）へ加算する。
            Branches += Result.Degrees[I] >= 3;
        }
        // 「分岐の件数（Branches）が4より小さい」に当てはまる場合だけ、続く文を実行する。
        if (Branches < 4)
            // この検査や計算の結果（Result）を呼び出し元へ返す。
            return Result;
        // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する。
        TSet<int32> Seen{0};
        // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
        TArray<int32> Queue{0};
        for (int32 At = 0; At < Queue.Num(); ++At)
            for (int32 I : Adjacency[Queue[At]])
            {
                // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
                const auto& C = Candidates[I];
                // 次に処理する位置または添字（Next）を符号付き32ビット整数として用意し、「Cの接続元IDが幅優先探索の待ち行列（Queue）の「At」に対応する要素と等しい」ならCの接続先ID、そうでなければCの接続元IDで初期化する。
                const int32 Next = C.From == Queue[At] ? C.To : C.From;
                // 「「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」が成立しない」に当てはまる場合だけ、続く文を実行する。
                if (!Seen.Contains(Next))
                {
                    // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                    Seen.Add(Next);
                    // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                    Queue.Add(Next);
                }
            }
        // 「探索済みの集合（Seen）で、要素数を得るが対象の件数（Count）と異なる」に当てはまる場合だけ、続く文を実行する。
        if (Seen.Num() != Count)
            // この検査や計算の結果（Result）を呼び出し元へ返す。
            return Result;
        // A two-space parallel loop and a four-space square are easier to recognize.
        // Count edges belonging to those short loops without traversing long paths.
        // 短い周回に属する接続の件数（ShortLoopEdges）に0を格納する。互いに見通せる入口の組数（VisiblePairs）に0を格納する。
        int32 ShortLoopEdges = 0, VisiblePairs = 0;
        for (int32 Omitted : Edges)
        {
            // Edge（Edge）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「Omitted」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& Edge = Candidates[Omitted];
            // 歩行距離（Distance）を配列として宣言する。
            TArray<int32> Distance;
            // 歩行距離（Distance）で、指定件数の要素を同じ初期値で埋める（引数：-1、対象の件数（Count））。
            Distance.Init(-1, Count);
            // 歩行距離（Distance）の「Edgeの接続元ID」に対応する要素を、0に設定する。
            Distance[Edge.From] = 0;
            // Search（Search）を配列として作り、波括弧内の値で初期化する。
            TArray<int32> Search{Edge.From};
            for (int32 At = 0; At < Search.Num() && Distance[Edge.To] < 0; ++At)
            {
                // Here（Here）を符号付き32ビット整数として用意し、Searchの「At」に対応する要素で初期化する。
                const int32 Here = Search[At];
                // 「歩行距離（Distance）の「Here」に対応する要素が3以上」に当てはまる場合だけ、続く文を実行する。
                if (Distance[Here] >= 3)
                    continue;
                for (int32 I : Adjacency[Here])
                {
                    // 「IがOmittedと等しい」に当てはまる場合だけ、続く文を実行する。
                    if (I == Omitted)
                        continue;
                    // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
                    const auto& C = Candidates[I];
                    // 次に処理する位置または添字（Next）を符号付き32ビット整数として用意し、「Cの接続元IDがHereと等しい」ならCの接続先ID、そうでなければCの接続元IDで初期化する。
                    const int32 Next = C.From == Here ? C.To : C.From;
                    // 「歩行距離（Distance）の「次に処理する位置または添字（Next）」に対応する要素が0より小さい」に当てはまる場合だけ、続く文を実行する。
                    if (Distance[Next] < 0)
                    {
                        // 歩行距離（Distance）の「次に処理する位置または添字（Next）」に対応する要素を、歩行距離（Distance）の「Here」に対応する要素と1の和に設定する。
                        Distance[Next] = Distance[Here] + 1;
                        // Searchで、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                        Search.Add(Next);
                    }
                }
            }
            // 「歩行距離（Distance）の「Edgeの接続先ID」に対応する要素が0以上」かつ「歩行距離（Distance）の「Edgeの接続先ID」に対応する要素が3以下」という判定を、trueなら1・falseなら0として短い周回に属する接続の件数（ShortLoopEdges）へ加算する。
            ShortLoopEdges += Distance[Edge.To] >= 0 && Distance[Edge.To] <= 3;
        }
        for (int32 N = 0; N < Count; ++N)
            for (int32 A = 0; A < Ports[N].Num(); ++A)
                for (int32 B = A + 1; B < Ports[N].Num(); ++B)
                    // 互いに見通せる入口の組数（VisiblePairs）へ、床の上だけを通る直線で見通せるか調べる（引数：部屋の出入口（Ports）の「N」に対応する要素の「A」に対応する要素、部屋の出入口（Ports）の「N」に対応する要素の「B」に対応する要素、Floorsの「N」に対応する要素）を加算する。
                    VisiblePairs += Visible(Ports[N][A], Ports[N][B], Floors[N]);
        // Resultの各項目を、以下で計算・取得した値へまとめて設定する。
        Result.bValid = true;
        Result.Score = ShortLoopEdges * 1000 + VisiblePairs * 30 + Edges.Num() * 2;
        // この検査や計算の結果（Result）を呼び出し元へ返す。
        return Result;
    };
    // 今回処理する値または位置（Current）を右辺から自動判定した型として用意し、Evaluate(Selected)で初期化する。
    auto Current = Evaluate(Selected);
    // 「「今回処理する値または位置（Current）の候補全体が条件を満たしたか」が成立しない」に当てはまる場合だけ、続く文を実行する。
    if (!Current.bValid)
        return;
    // Remove redundant doors only if every existing long arm retains an exit.
    for (int32 Pass = 0; Pass < Candidates.Num(); ++Pass)
    {
        // 最良候補の評価値（BestScore）を符号付き32ビット整数として用意し、今回処理する値または位置（Current）の候補の評価値で初期化する。
        int32 BestScore = Current.Score;
        // 現在の最良候補（Best）を配列として宣言する。
        TArray<int32> Best;
        for (int32 I = 0; I < Selected.Num(); ++I)
        {
            // 接続を入れ替えて試す仮の一覧（Trial）を右辺から自動判定した型として用意し、採用済み候補の添字一覧（Selected）で初期化する。
            auto Trial = Selected;
            // 接続を入れ替えて試す仮の一覧（Trial）で、指定添字の要素を取り除く（引数：I）。
            Trial.RemoveAt(I);
            // この検査や計算の結果（Result）を右辺から自動判定した型として用意し、Evaluate(Trial)で初期化する。
            const auto Result = Evaluate(Trial);
            // 「「この検査や計算の結果（Result）の候補全体が条件を満たしたか」かつ「この検査や計算の結果（Result）の候補の評価値が最良候補の評価値（BestScore）より小さい」」に当てはまる場合だけ、続く文を実行する。
            if (Result.bValid && Result.Score < BestScore)
            {
                // 最良候補の評価値（BestScore）を、この検査や計算の結果（Result）の候補の評価値に設定する。
                BestScore = Result.Score;
                // 現在の最良候補（Best）を、コピーせず移動できる形で値を渡す（引数：接続を入れ替えて試す仮の一覧（Trial））に設定する。
                Best = MoveTemp(Trial);
            }
        }
        // 「現在の最良候補（Best）で、空かを調べる」に当てはまる場合だけ、続く文を実行する。
        if (Best.IsEmpty())
            break;
        // 採用済み候補の添字一覧（Selected）を、コピーせず移動できる形で値を渡す（引数：現在の最良候補（Best））に設定する。
        Selected = MoveTemp(Best);
        // 今回処理する値または位置（Current）を、Evaluate(Selected)に設定する。
        Current = Evaluate(Selected);
    }
    // Exchange doors as well as removing them: this can replace a small local loop
    // with a return route through several spaces while preserving footprint/arms.
    // 再現可能な乱数生成器（Random）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
    FRandomStream Random(Seed ^ 0x27c4a91d);
    for (int32 Pass = 0; Pass < 4; ++Pass)
    {
        // まだ採用していない候補の一覧（Available）を配列として宣言する。
        TArray<int32> Available;
        for (int32 I = 0; I < Candidates.Num(); ++I)
            // 「「採用済み候補の添字一覧（Selected）で、指定値が登録されているか調べる（引数：I）」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!Selected.Contains(I))
                // まだ採用していない候補の一覧（Available）で、要素または対応関係を末尾へ追加する（引数：I）。
                Available.Add(I);
        for (int32 I = Available.Num() - 1; I > 0; --I)
            // まだ採用していない候補の一覧（Available）で、指定した2つの添字の要素を交換する（引数：I、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
            Available.Swap(I, Random.RandRange(0, I));
        // 処理する添字の順番（Order）を配列として宣言する。
        TArray<int32> Order;
        for (int32 I = 0; I < Selected.Num(); ++I)
            // 処理する添字の順番（Order）で、要素または対応関係を末尾へ追加する（引数：I）。
            Order.Add(I);
        for (int32 I = Order.Num() - 1; I > 0; --I)
            // 処理する添字の順番（Order）で、指定した2つの添字の要素を交換する（引数：I、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
            Order.Swap(I, Random.RandRange(0, I));
        // 最良候補の評価値（BestScore）に今回処理する値または位置（Current）の候補の評価値を格納する。試した接続の入れ替え件数（Evaluations）に0を格納する。
        int32 BestScore = Current.Score, Evaluations = 0;
        // 現在の最良候補（Best）を配列として宣言する。
        TArray<int32> Best;
        for (int32 Slot : Order)
        {
            for (int32 Add : Available)
            {
                // 「++Evaluationsが256より大きい」に当てはまる場合だけ、続く文を実行する。
                if (++Evaluations > 256)
                    break;
                // 接続を入れ替えて試す仮の一覧（Trial）を右辺から自動判定した型として用意し、採用済み候補の添字一覧（Selected）で初期化する。
                auto Trial = Selected;
                // 接続を入れ替えて試す仮の一覧（Trial）の「Slot」に対応する要素を、Addに設定する。
                Trial[Slot] = Add;
                // この検査や計算の結果（Result）を右辺から自動判定した型として用意し、Evaluate(Trial)で初期化する。
                const auto Result = Evaluate(Trial);
                // 「「この検査や計算の結果（Result）の候補全体が条件を満たしたか」かつ「この検査や計算の結果（Result）の候補の評価値が最良候補の評価値（BestScore）より小さい」」に当てはまる場合だけ、続く文を実行する。
                if (Result.bValid && Result.Score < BestScore)
                {
                    // 最良候補の評価値（BestScore）を、この検査や計算の結果（Result）の候補の評価値に設定する。
                    BestScore = Result.Score;
                    // 現在の最良候補（Best）を、コピーせず移動できる形で値を渡す（引数：接続を入れ替えて試す仮の一覧（Trial））に設定する。
                    Best = MoveTemp(Trial);
                }
            }
            // 「試した接続の入れ替え件数（Evaluations）が256より大きい」に当てはまる場合だけ、続く文を実行する。
            if (Evaluations > 256)
                break;
        }
        // 「現在の最良候補（Best）で、空かを調べる」に当てはまる場合だけ、続く文を実行する。
        if (Best.IsEmpty())
            break;
        // 採用済み候補の添字一覧（Selected）を、コピーせず移動できる形で値を渡す（引数：現在の最良候補（Best））に設定する。
        Selected = MoveTemp(Best);
        // 今回処理する値または位置（Current）を、Evaluate(Selected)に設定する。
        Current = Evaluate(Selected);
    }
    // 部屋ごとの接続数（Degrees）を、コピーせず移動できる形で値を渡す（引数：今回処理する値または位置（Current）の部屋ごとの接続数）に設定する。
    Degrees = MoveTemp(Current.Degrees);
}
