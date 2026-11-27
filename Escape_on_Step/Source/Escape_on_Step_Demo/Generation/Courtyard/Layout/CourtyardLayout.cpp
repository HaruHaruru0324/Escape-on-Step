#include "Generation/Courtyard/Layout/CourtyardLayout.h"

namespace
{
    // const FIntPoint CourtyardLayoutSteps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint CourtyardLayoutSteps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    // この関数の役割：エラーを記録して失敗を返す。ここから本体を定義する。
    bool Fail(FString& Error, const TCHAR* Message)
    {
        // 失敗理由（Error）を、エラーメッセージ（Message）に設定する。
        Error = Message;
        return false;
    }

    // この関数の役割：セルが長方形区画の内側か調べる。ここから本体を定義する。
    bool InRect(FIntPoint P, const FCourtyardRoomSection& R)
    {
        // 「「PのX成分がRの最小端のX成分以上」かつ「PのY成分がRの最小端のY成分以上」」かつ「PのX成分」がRの最小端のX成分とRの大きさのX &&の和より小さいを呼び出し元へ返す。
        return P.X >= R.Min.X && P.Y >= R.Min.Y && P.X < R.Min.X + R.Size.X &&
               // PのY成分がRの最小端のY成分とRの大きさのY成分の和より小さい。
               P.Y < R.Min.Y + R.Size.Y;
    }

    // この関数の役割：セルがその部屋の実床に含まれるか調べる。ここから本体を定義する。
    bool Contains(const FCourtyardNode& N, FIntPoint P)
    {
        // Nの実床セル一覧で、指定値が登録されているか調べる（引数：P）を呼び出し元へ返す。
        return N.FloorCells.Contains(P);
    }

    // この関数の役割：区画が横長か縦長かを求める。ここから本体を定義する。
    int32 SectionAxis(const FCourtyardRoomSection& R)
    {
        // 「Rの大きさのX成分がRの大きさのY成分より大きい」なら0、そうでなければ1を呼び出し元へ返す。
        return R.Size.X > R.Size.Y ? 0 : 1;
    }

    // この関数の役割：指定した軸の座標成分を取り出す。ここから本体を定義する。
    int32 Along(FIntPoint P, int32 Axis)
    {
        // 「長手方向の軸（Axis）が0と等しい」ならPのX成分、そうでなければPのY成分を呼び出し元へ返す。
        return Axis == 0 ? P.X : P.Y;
    }

    // 2本の長方形が、それぞれの「端」の角だけでL字に重なることを確認する。
    // 片方でも中央に重なればT字/十字の継ぎ足しなので不採用。
    // この関数の役割：長方形区画が端同士でL字の鎖としてつながるか検査する。ここから本体を定義する。
    bool BuildSectionChain(
        // 区画同士の隣接一覧（Adj）、FString& Error)（FString& Error)）を、それぞれ配列として用意する。元の値を参照し、コピーしない。
        const TArray<FCourtyardRoomSection>& Sections, TArray<TArray<int32>>& Adj, FString& Error)
    {
        // 「「曲がる空間を構成する長方形区画（Sections）で、要素数を得るが2より小さい」または「曲がる空間を構成する長方形区画（Sections）で、要素数を得るが4より大きい」」に当てはまる場合だけ、続く文を実行する。
        if (Sections.Num() < 2 || Sections.Num() > 4)
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"A wide space needs 2..4 turned sections"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("A wide space needs 2..4 turned sections"));
        }
        // 区画同士の隣接一覧（Adj）で、要素数を指定件数にそろえる（引数：曲がる空間を構成する長方形区画（Sections）で、要素数を得る）。
        Adj.SetNum(Sections.Num());
        int32 Edges = 0;
        for (const auto& R : Sections)
        {
            // 「「「「Rの大きさのX成分が2より小さい」または「Rの大きさのY成分が2より小さい」」または「Rの大きさのX成分が256より大きい」」または「Rの大きさのY成分が256より大きい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (R.Size.X < 2 || R.Size.Y < 2 || R.Size.X > 256 || R.Size.Y > 256 ||
                // この行ではRの大きさのX成分がRの大きさのY成分と等しいを指定し、前後の行の計算・呼び出しへつなげる。
                R.Size.X == R.Size.Y)
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid straight section dimensions"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Invalid straight section dimensions"));
            }
        }
        for (int32 I = 0; I < Sections.Num(); ++I)
        {
            for (int32 J = I + 1; J < Sections.Num(); ++J)
            {
                // A（A）を右辺から自動判定した型として用意し、曲がる空間を構成する長方形区画（Sections）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
                const auto& A = Sections[I];
                // B（B）を右辺から自動判定した型として用意し、曲がる空間を構成する長方形区画（Sections）の「J」に対応する要素で初期化する。元の値を参照し、コピーしない。
                const auto& B = Sections[J];
                // 範囲の最小端（Min）を、括弧内の値を使って2次元の整数セル座標として初期化する。
                const FIntPoint Min(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y));
                // 範囲の最大端（Max）を2次元の整数セル座標として作る。括弧内に並べる値を初期値として使う。
                const FIntPoint Max(FMath::Min(A.Min.X + A.Size.X, B.Min.X + B.Size.X),
                    // 2区画のY方向の上端のうち小さい方を選び、重なっている範囲の最大Y端を求める。
                    FMath::Min(A.Min.Y + A.Size.Y, B.Min.Y + B.Size.Y));
                // 「「範囲の最大端（Max）のX成分が範囲の最小端（Min）のX成分以下」または「範囲の最大端（Max）のY成分が範囲の最小端（Min）のY成分以下」」に当てはまる場合だけ、続く文を実行する。
                if (Max.X <= Min.X || Max.Y <= Min.Y)
                {
                    continue;
                }
                // 「区画が横長か縦長かを求める（引数：A）が区画が横長か縦長かを求める（引数：B）と等しい」に当てはまる場合だけ、続く文を実行する。
                if (SectionAxis(A) == SectionAxis(B))
                {
                    // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Parallel room sections overlap"というUE文字列）を呼び出し元へ返す。
                    return Fail(Error, TEXT("Parallel room sections overlap"));
                }
                // H（H）を右辺から自動判定した型として用意し、「区画が横長か縦長かを求める（引数：A）が0と等しい」ならA、そうでなければBで初期化する。元の値を参照し、コピーしない。
                const auto& H = SectionAxis(A) == 0 ? A : B;
                // V（V）を右辺から自動判定した型として用意し、「区画が横長か縦長かを求める（引数：A）が1と等しい」ならA、そうでなければBで初期化する。元の値を参照し、コピーしない。
                const auto& V = SectionAxis(A) == 1 ? A : B;
                // AtHEnd（AtHEnd）を真偽値として用意し、「範囲の最小端（Min）のX成分がHの最小端のX成分と等しい」または「範囲の最大端（Max）のX成分がHの最小端のX成分とHの大きさのX成分の和と等しい」で初期化する。
                const bool AtHEnd = Min.X == H.Min.X || Max.X == H.Min.X + H.Size.X;
                // AtVEnd（AtVEnd）を真偽値として用意し、「範囲の最小端（Min）のY成分がVの最小端のY成分と等しい」または「範囲の最大端（Max）のY成分がVの最小端のY成分とVの大きさのY成分の和と等しい」で初期化する。
                const bool AtVEnd = Min.Y == V.Min.Y || Max.Y == V.Min.Y + V.Size.Y;
                // 「「「「「AtHEnd」が成立しない」または「「AtVEnd」が成立しない」」または「範囲の最小端（Min）のX成分がVの最小端のX成分と異なる」」または「範囲の最大端（Max）のX成分がVの最小端のX成分とVの大きさのX成分の和と異なる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
                if (!AtHEnd || !AtVEnd || Min.X != V.Min.X || Max.X != V.Min.X + V.Size.X ||
                    // この行では「範囲の最小端（Min）のY成分がHの最小端のY成分と異なる」または「範囲の最大端（Max）のY成分がHの最小端のY成分とHの大きさのY成分の和と異なる」を指定し、前後の行の計算・呼び出しへつなげる。
                    Min.Y != H.Min.Y || Max.Y != H.Min.Y + H.Size.Y)
                {
                    // Fail(を呼び出し元へ返す。
                    return Fail(
                        // この関数の役割：UEが使う文字列リテラルを指定する。ここでは呼び出し方を宣言する。
                        Error, TEXT("Sections must meet at both ends, never at their middle"));
                }
                // 区画同士の隣接一覧（Adj）の「I」に対応する要素で、要素または対応関係を末尾へ追加する（引数：J）。
                Adj[I].Add(J);
                // 区画同士の隣接一覧（Adj）の「J」に対応する要素で、要素または対応関係を末尾へ追加する（引数：I）。
                Adj[J].Add(I);
                ++Edges;
            }
        }
        // 「接続辺の件数（Edges）が曲がる空間を構成する長方形区画（Sections）で、要素数を得るから1を引いた値と異なる」に当てはまる場合だけ、続く文を実行する。
        if (Edges != Sections.Num() - 1)
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Section chain is disconnected or branched"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Section chain is disconnected or branched"));
        }
        int32 Ends = 0;
        for (const auto& Neighbors : Adj)
        {
            // 「Neighborsで、要素数を得るが1と等しい」に当てはまる場合だけ、続く文を実行する。
            if (Neighbors.Num() == 1)
            {
                ++Ends;
            }
            // 前の条件が不成立で、さらに「Neighborsで、要素数を得るが2と異なる」に当てはまる場合だけ、続く文を実行する。
            else if (Neighbors.Num() != 2)
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Cross-shaped section junction"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Cross-shaped section junction"));
            }
        }
        // 「到達側の候補または末端数（Ends）が2と等しい」ならtrue（成立・有効）、そうでなければエラーを記録して失敗を返す（引数：失敗理由（Error）、"A section chain needs two ends"というUE文字列）を呼び出し元へ返す。
        return Ends == 2 ? true : Fail(Error, TEXT("A section chain needs two ends"));
    }

    // この関数の役割：連結する長方形区画を片端から順番に並べる。ここから本体を定義する。
    bool OrderSections(FCourtyardNode& N, FString& Error)
    {
        // 区画同士の隣接一覧（Adj）を配列として宣言する。
        TArray<TArray<int32>> Adj;
        // 「「長方形区画が端同士でL字の鎖としてつながるか検査する（引数：Nの長方形区画一覧、区画同士の隣接一覧（Adj）、失敗理由（Error））」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!BuildSectionChain(N.Sections, Adj, Error))
        {
            return false;
        }
        int32 Current = 0;
        // 「区画同士の隣接一覧（Adj）の「今回処理する値または位置（Current）」に対応する要素で、要素数を得るが1と異なる」が成り立つ間、続く文を繰り返す。
        while (Adj[Current].Num() != 1)
        {
            ++Current;
        }
        // 端から順に並べ直した区画一覧（Ordered）を配列として宣言する。
        TArray<FCourtyardRoomSection> Ordered;
        // 経路を逆にたどる直前セル（Previous）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
        int32 Previous = INDEX_NONE;
        for (int32 I = 0; I < N.Sections.Num(); ++I)
        {
            // 端から順に並べ直した区画一覧（Ordered）で、要素または対応関係を末尾へ追加する（引数：Nの長方形区画一覧の「今回処理する値または位置（Current）」に対応する要素）。
            Ordered.Add(N.Sections[Current]);
            // 次に処理する位置または添字（Next）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
            int32 Next = INDEX_NONE;
            for (int32 Candidate : Adj[Current])
            {
                // 「Candidateが経路を逆にたどる直前セル（Previous）と異なる」に当てはまる場合だけ、続く文を実行する。
                if (Candidate != Previous)
                {
                    // 次に処理する位置または添字（Next）を、Candidateに設定する。
                    Next = Candidate;
                }
            }
            // 「次に処理する位置または添字（Next）がINDEX_NONE（未指定を表す-1）と等しい」に当てはまる場合だけ、続く文を実行する。
            if (Next == INDEX_NONE)
            {
                break;
            }
            // 経路を逆にたどる直前セル（Previous）を、今回処理する値または位置（Current）に設定する。
            Previous = Current;
            // 今回処理する値または位置（Current）を、次に処理する位置または添字（Next）に設定する。
            Current = Next;
        }
        // 「端から順に並べ直した区画一覧（Ordered）で、要素数を得るがNの長方形区画一覧で、要素数を得ると異なる」に当てはまる場合だけ、続く文を実行する。
        if (Ordered.Num() != N.Sections.Num())
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Section chain cannot be ordered"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Section chain cannot be ordered"));
        }
        // Nの長方形区画一覧を、コピーせず移動できる形で値を渡す（引数：端から順に並べ直した区画一覧（Ordered））に設定する。
        N.Sections = MoveTemp(Ordered);
        return true;
    }

}

// 分岐口は各矩形の端の角に限る。広い区画の中央へ別の広い区画を継ぎ足さない。
// この関数の役割：セルが区画の端の帯に含まれるか調べる。ここから本体を定義する。
static bool AtSectionEnd(const FCourtyardNode& N, FIntPoint P)
{
    for (const auto& R : N.Sections)
    {
        // 「「セルが長方形区画の内側か調べる（引数：P、R）」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!InRect(P, R))
        {
            continue;
        }
        // 長手方向の軸（Axis）に区画が横長か縦長かを求める（引数：R）を格納する。T（T）に指定した軸の座標成分を取り出す（引数：PからRの最小端を引いた値、長手方向の軸（Axis））を格納する。
        const int32 Axis = SectionAxis(R), T = Along(P - R.Min, Axis);
        // 幅（Width）に指定した軸の座標成分を取り出す（引数：Rの大きさ、1から長手方向の軸（Axis）を引いた値）を格納する。長さ（Length）に指定した軸の座標成分を取り出す（引数：Rの大きさ、長手方向の軸（Axis））を格納する。
        const int32 Width = Along(R.Size, 1 - Axis), Length = Along(R.Size, Axis);
        // 「「Tが幅（Width）より小さい」または「Tが長さ（Length）から幅（Width）を引いた値以上」」に当てはまる場合だけ、続く文を実行する。
        if (T < Width || T >= Length - Width)
        {
            return true;
        }
    }
    return false;
}

// 両端の矩形で、接続口のない長い腕だけを検出・短縮する。
// 接続済みの腕を途中で切らず、曲がり角から最大1セルの出っ張りを残す。
// この関数の役割：出口のない長い腕を調べ、指定されていれば短縮する。ここから本体を定義する。
static bool FixBlindEnds(FCourtyardNode& N, const TArray<FIntPoint>& Ports, bool Trim)
{
    bool Valid = true;
    // 袋小路は複数の曲がり角にまたがる場合もある。出口のない末端区画を
    // 順に戻し、最初の接続口がある区画まで追う。最後の1本だけの短縮では不十分。
    bool Removed = true;
    // 「「不要な末端区画を削除したか（Removed）」かつ「Nの長方形区画一覧で、要素数を得るが2より大きい」」が成り立つ間、続く文を繰り返す。
    while (Removed && N.Sections.Num() > 2)
    {
        Removed = false;
        for (int32 End : {0, N.Sections.Num() - 1})
        {
            // R（R）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「End」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& R = N.Sections[End];
            // Neighbor（Neighbor）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「「Endが0と等しい」なら1、そうでなければEndから1を引いた値」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& Neighbor = N.Sections[End == 0 ? 1 : End - 1];
            bool CanRemove = true;
            for (auto P : Ports)
            {
                // 「「セルが長方形区画の内側か調べる（引数：P、R）」かつ「「セルが長方形区画の内側か調べる（引数：P、Neighbor）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
                if (InRect(P, R) && !InRect(P, Neighbor))
                {
                    CanRemove = false;
                    break;
                }
            }
            if (CanRemove)
            {
                if (!Trim)
                {
                    return false;
                }
                // Nの長方形区画一覧で、指定添字の要素を取り除く（引数：End）。
                N.Sections.RemoveAt(End);
                Removed = true;
                break;
            }
        }
    }
    for (int32 End : {0, N.Sections.Num() - 1})
    {
        // R（R）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「End」に対応する要素で初期化する。元の値を参照し、コピーしない。
        auto& R = N.Sections[End];
        // 隣とつながる側の区画（Join）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「「Endが0と等しい」なら1、そうでなければEndから1を引いた値」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& Join = N.Sections[End == 0 ? 1 : End - 1];
        // 長手方向の軸（Axis）に区画が横長か縦長かを求める（引数：R）を格納する。幅（Width）に指定した軸の座標成分を取り出す（引数：Rの大きさ、1から長手方向の軸（Axis）を引いた値）を格納する。
        const int32 Axis = SectionAxis(R), Width = Along(R.Size, 1 - Axis),
                    // 長さ（Length）を、指定した軸の座標成分を取り出す（引数：Rの大きさ、長手方向の軸（Axis））に設定する。
                    Length = Along(R.Size, Axis);
        // 区画の長手方向の手前側で接続しているか（JoinedAtStart）を真偽値として用意し、指定した軸の座標成分を取り出す（引数：隣とつながる側の区画（Join）の最小端、長手方向の軸（Axis））が指定した軸の座標成分を取り出す（引数：Rの最小端、長手方向の軸（Axis））以下で初期化する。
        const bool JoinedAtStart = Along(Join.Min, Axis) <= Along(R.Min, Axis);
        bool Exit = false;
        for (auto P : Ports)
        {
            // 「セルが長方形区画の内側か調べる（引数：P、R）」に当てはまる場合だけ、続く文を実行する。
            if (InRect(P, R))
            {
                // T（T）を符号付き32ビット整数として用意し、指定した軸の座標成分を取り出す（引数：PからRの最小端を引いた値、長手方向の軸（Axis））で初期化する。
                const int32 T = Along(P - R.Min, Axis);
                // 自由端に出口があるか（Exit）へ、「区画の長手方向の手前側で接続しているか（JoinedAtStart）」ならTが長さ（Length）から幅（Width）を引いた値以上、そうでなければTが幅（Width）より小さいとのビットORで該当するビットを残す。
                Exit |= JoinedAtStart ? T >= Length - Width : T < Width;
            }
        }
        // 「「自由端に出口があるか（Exit）」または「長さ（Length）が幅（Width）と1の和以下」」に当てはまる場合だけ、続く文を実行する。
        if (Exit || Length <= Width + 1)
        {
            continue;
        }
        Valid = false;
        if (Trim)
        {
            if (!JoinedAtStart)
            {
                // 「長手方向の軸（Axis）が0と等しい」に当てはまる場合だけ、続く文を実行する。
                if (Axis == 0)
                {
                    // Rの最小端のX成分へ、長さ（Length）から幅（Width）を引いた値から1を引いた値を加算する。
                    R.Min.X += Length - Width - 1;
                }
                // 直前の条件に当てはまらなかった場合の処理へ進む。
                else
                {
                    // Rの最小端のY成分へ、長さ（Length）から幅（Width）を引いた値から1を引いた値を加算する。
                    R.Min.Y += Length - Width - 1;
                }
            }
            // 「長手方向の軸（Axis）が0と等しい」に当てはまる場合だけ、続く文を実行する。
            if (Axis == 0)
            {
                // Rの大きさのX成分を、幅（Width）と1の和に設定する。
                R.Size.X = Width + 1;
            }
            // 直前の条件に当てはまらなかった場合の処理へ進む。
            else
            {
                // Rの大きさのY成分を、幅（Width）と1の和に設定する。
                R.Size.Y = Width + 1;
            }
        }
    }
    // 検査の成否（Valid）を呼び出し元へ返す。
    return Valid;
}

// この関数の役割：従来方式の折れた空間とトンネルの配置データを作る。ここから本体を定義する。
bool UCourtyardLayoutLibrary::BuildCourtyardPlan(
    // 入力設定（Input）、int32 Seed（int32 Seed）、FCourtyardPlan& Plan（FCourtyardPlan& Plan）、FString& Error)（FString& Error)）を、それぞれ迷路の生成設定として用意する。元の値を参照し、コピーしない。
    const FCourtyardSettings& Input, int32 Seed, FCourtyardPlan& Plan, FString& Error)
{
    // 生成結果（Plan）を、生成結果を既定状態で用意するに設定する。
    Plan = FCourtyardPlan();
    // 生成結果（Plan）の再現用Seedを、再現用Seed（Seed）に設定する。
    Plan.Seed = Seed;
    Error.Reset();
    // S（S）を迷路の生成設定として用意し、入力設定（Input）で初期化する。
    FCourtyardSettings S = Input;
    // 生成設定の各項目を、以下の許容範囲へ収める。
    S.StreetCount = FMath::Clamp(S.StreetCount, 9, 24);
    S.StreetWidth = FMath::Clamp(S.StreetWidth, 2, 3);
    S.MinStreetLength = FMath::Clamp(S.MinStreetLength, 4, 12);
    S.MaxStreetLength = FMath::Clamp(S.MaxStreetLength, S.MinStreetLength, 12);
    S.MinTunnelCells = FMath::Clamp(S.MinTunnelCells, 1, 3);
    S.MaxTunnelCells = FMath::Clamp(S.MaxTunnelCells, S.MinTunnelCells, 3);
    S.MinRoomSections = FMath::Clamp(S.MinRoomSections, 2, 4);
    S.MaxRoomSections = FMath::Clamp(S.MaxRoomSections, S.MinRoomSections, 4);
    S.MaxRooms = FMath::Clamp(S.MaxRooms, 0, 6);
    // 生成結果（Plan）の採用する直線トンネルの最長セル数を、Sのトンネルの最長セル数に設定する。
    Plan.TunnelCellLimit = S.MaxTunnelCells;
    // 再現可能な乱数生成器（Random）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
    FRandomStream Random(Seed);
    int32 RoomCount = 0;
    for (int32 I = 0; I < S.StreetCount && RoomCount < S.MaxRooms; ++I)
    {
        // 再現可能な乱数生成器（Random）で、0以上1未満の小数をSeedに従って抽選するが指定した最小値と最大値の範囲へ収める（引数：Sの屋内部屋を追加・選択する確率、0.f、1.f）より小さいという判定を、trueなら1・falseなら0としてRoomCountへ加算する。
        RoomCount += Random.FRand() < FMath::Clamp(S.RoomChance, 0.f, 1.f);
    }
    // 「Sの屋内部屋を追加・選択する確率が0.fより大きい」に当てはまる場合だけ、続く文を実行する。
    if (S.RoomChance > 0.f)
    {
        // RoomCountを、大きい方の値を選ぶ（引数：RoomCount、小さい方の値を選ぶ（引数：2、Sの屋内部屋の上限数））に設定する。
        RoomCount = FMath::Max(RoomCount, FMath::Min(2, S.MaxRooms));
    }
    // Requested（Requested）を符号付き32ビット整数として用意し、Sの広い空間を何個作るかの基準で初期化する。
    const int32 Requested = S.StreetCount;
    // 配置する列数（Columns）を符号付き32ビット整数として用意し、端数を切り上げて整数にする（引数：平方根を求める（引数：単精度の実数へ変換する（引数：Requested）））で初期化する。
    const int32 Columns = FMath::CeilToInt(FMath::Sqrt(float(Requested)));
    // 配置する行数または開口セル数（Rows）に割り算の端数を切り上げ、必要な個数を整数で求める（引数：Requested、配置する列数（Columns））を格納する。対象の件数（Count）に配置する列数（Columns）と配置する行数または開口セル数（Rows）の積を格納する。
    const int32 Rows = FMath::DivideAndRoundUp(Requested, Columns), Count = Columns * Rows;
    // 幅（Width）を符号付き32ビット整数として用意し、Sの旧方式の広い道の幅（セル）で初期化する。
    const int32 Width = S.StreetWidth;
    // 密集時はU字の内側を2セルまで詰める。セルや道幅の縮小ではなく、曲がり角間の距離を縮める。
    // 4区画の内巻き形は中央に別の折り返し用の空間が必要なので、密集時はL/U字を使う。
    // RequestedSpan（RequestedSpan）を符号付き32ビット整数として用意し、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：Sの旧方式の広い道の最短長（セル）、Sの旧方式の広い道の最長長（セル））で初期化する。
    const int32 RequestedSpan = Random.RandRange(S.MinStreetLength, S.MaxStreetLength);
    // 折れた空間の区画長（Span）を符号付き32ビット整数として用意し、「Sの旧方式で空白を詰める設定」なら幅（Width）と2の和、そうでなければ大きい方の値を選ぶ（引数：2と幅（Width）の積と2の和、RequestedSpan）で初期化する。
    const int32 Span = S.bDensePacking ? Width + 2 : FMath::Max(2 * Width + 2, RequestedSpan);
    // 1空間の外接範囲の辺長（Extent）に折れた空間の区画長（Span）と幅（Width）の和を格納する。区画長の中央位置（Middle）に折れた空間の区画長（Span）を2で割った値を格納する。
    const int32 Extent = Span + Width, Middle = Span / 2;
    // 各空間の基準X座標（TileX）を配列として作り、波括弧内の値で初期化する。
    TArray<int32> TileX{0}, TileY{0}, Parts, Kinds;
    for (int32 I = 1; I < Columns; ++I)
    {
        // 各空間の基準X座標（TileX）で、要素または対応関係を末尾へ追加する（引数：各空間の基準X座標（TileX）で、末尾の要素を取得すると1空間の外接範囲の辺長（Extent）の和と再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：Sのトンネルの最短セル数、Sのトンネルの最長セル数）の和）。
        TileX.Add(TileX.Last() + Extent + Random.RandRange(S.MinTunnelCells, S.MaxTunnelCells));
    }
    for (int32 I = 1; I < Rows; ++I)
    {
        // 各空間の基準Y座標（TileY）で、要素または対応関係を末尾へ追加する（引数：各空間の基準Y座標（TileY）で、末尾の要素を取得すると1空間の外接範囲の辺長（Extent）の和と再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：Sのトンネルの最短セル数、Sのトンネルの最長セル数）の和）。
        TileY.Add(TileY.Last() + Extent + Random.RandRange(S.MinTunnelCells, S.MaxTunnelCells));
    }
    // 開始ノードは屋外。残りのルームの配置をシャッフルする。
    // RoomIds（RoomIds）を配列として宣言する。
    TArray<int32> RoomIds;
    for (int32 I = 1; I < Count; ++I)
    {
        // RoomIdsで、要素または対応関係を末尾へ追加する（引数：I）。
        RoomIds.Add(I);
    }
    for (int32 I = RoomIds.Num() - 1; I > 0; --I)
    {
        // RoomIdsで、指定した2つの添字の要素を交換する（引数：I、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
        RoomIds.Swap(I, Random.RandRange(0, I));
    }
    // Kindsで、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
    Kinds.Init(0, Count);
    for (int32 I = 0; I < RoomCount; ++I)
    {
        // Kinds[RoomIds[I]]を、1に設定する。
        Kinds[RoomIds[I]] = 1;
    }
    for (int32 I = 0; I < Count; ++I)
    {
        // RequestedParts（RequestedParts）を符号付き32ビット整数として用意し、Kindsの「I」に対応する要素で初期化する。
        const int32 RequestedParts = Kinds[I]
                                         // この行では? Randomで、指定範囲の整数をSeedに従って抽選する（引数：Sの旧方式で1空間を作る最小区画数、Sの旧方式で1空間を作る最大区画数）を指定し、前後の行の計算・呼び出しへつなげる。
                                         ? Random.RandRange(S.MinRoomSections, S.MaxRoomSections)
                                         // : Randomで、指定範囲の整数をSeedに従って抽選する（引数：2、4）。
                                         : Random.RandRange(2, 4);
        // 各空間を構成する区画数（Parts）で、要素または対応関係を末尾へ追加する（引数：「Sの旧方式で空白を詰める設定」なら小さい方の値を選ぶ（引数：RequestedParts、3）、そうでなければRequestedParts）。
        Parts.Add(S.bDensePacking ? FMath::Min(RequestedParts, 3) : RequestedParts);
    }

    struct FCandidate
    {
        // A（A）、B（B）、四方向の番号（Direction）を、それぞれ符号付き32ビット整数として用意する。
        int32 A, B, Direction;
        // 床セルの一覧または索引（Cells）を配列として宣言する。
        TArray<FIntPoint> Cells;
    };

    // 接続や入口の候補（Candidates）を配列として宣言する。
    TArray<FCandidate> Candidates;
    // 向きを抽選し、接続候補が全体を結べない場合だけ再抽選する。最後は接続可能な向きへ戻す。
    for (int32 Attempt = 0; Attempt < 33; ++Attempt)
    {
        Plan.Nodes.Reset();
        Candidates.Reset();
        for (int32 Id = 0; Id < Count; ++Id)
        {
            // N（N）を広い空間1個の情報として宣言する。
            FCourtyardNode N;
            // Nの各項目を、以下で計算・取得した値へまとめて設定する。
            N.Id = Id;
            N.Kind = ECourtyardCellKind::Street;
            // 配置または計測の基準位置（Origin）を2次元の整数セル座標として用意し、GetStaggeredOrigin(で初期化する。
            const FIntPoint Origin = GetStaggeredOrigin(
                // 列番号と行番号から空間の元のタイル位置を取り出し、ID・列数・Seed・設定を位置ずらし計算へ渡す。
                FIntPoint(TileX[Id % Columns], TileY[Id / Columns]), Id, Columns, Seed, S);
            // 回転（Rotation）を符号付き32ビット整数として用意し、「全体の再試行番号（Attempt）が32と等しい」なら0、そうでなければ再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、3）で初期化する。
            const int32 Rotation = Attempt == 32 ? 0 : Random.RandRange(0, 3);
            // Rotate（Rotate）を、周囲の値を使って判定・計算する小さな関数として定義する。
            const auto Rotate = [&](FIntPoint P)
            {
                for (int32 I = 0; I < Rotation; ++I)
                {
                    // Pを、{Span - P.Y, P.X}に設定する。
                    P = {Span - P.Y, P.X};
                }
                // Pと配置または計測の基準位置（Origin）の和を呼び出し元へ返す。
                return P + Origin;
            };
            // Corners（Corners）を配列として作り、波括弧内の値で初期化する。
            TArray<FIntPoint> Corners{{0, 0}, {Span, 0}, {Span, Span}};
            // 「各空間を構成する区画数（Parts）の「空間の識別番号（Id）」に対応する要素が3以上」に当てはまる場合だけ、続く文を実行する。
            if (Parts[Id] >= 3)
            {
                // Cornersで、要素または対応関係を末尾へ追加する（引数：{Parts[Id] == 3 ? 0 : Middle, Span}）。
                Corners.Add({Parts[Id] == 3 ? 0 : Middle, Span});
            }
            // 「各空間を構成する区画数（Parts）の「空間の識別番号（Id）」に対応する要素が4と等しい」に当てはまる場合だけ、続く文を実行する。
            if (Parts[Id] == 4)
            {
                // Cornersで、要素または対応関係を末尾へ追加する（引数：{Middle, Middle}）。
                Corners.Add({Middle, Middle});
            }
            for (int32 I = 1; I < Corners.Num(); ++I)
            {
                // A（A）にRotate(Corners[I - 1])を格納する。B（B）にRotate(Corners[I])を格納する。
                const auto A = Rotate(Corners[I - 1]), B = Rotate(Corners[I]);
                // R（R）を長方形区画の位置と寸法として宣言する。
                FCourtyardRoomSection R;
                // Rの各項目を、以下で計算・取得した値へまとめて設定する。
                R.Min = {FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)};
                R.Size = {FMath::Abs(A.X - B.X) + Width, FMath::Abs(A.Y - B.Y) + Width};
                // Nの長方形区画一覧で、要素または対応関係を末尾へ追加する（引数：R）。
                N.Sections.Add(R);
            }
            // 「「連結する長方形区画を片端から順番に並べる（引数：N、失敗理由（Error））」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!OrderSections(N, Error))
            {
                return false;
            }
            // Nの各項目を、以下で計算・取得した値へまとめて設定する。
            N.Min = Origin;
            N.Size = {Extent, Extent};
            for (const auto& R : N.Sections)
            {
                for (int32 Dx = 0; Dx < R.Size.X; ++Dx)
                {
                    for (int32 Dy = 0; Dy < R.Size.Y; ++Dy)
                    {
                        // Nの実床セル一覧で、同じ要素がなければ追加する（引数：Rの最小端と2次元の整数セル座標を組み立てる（引数：Dx、Dy）の和）。
                        N.FloorCells.AddUnique(R.Min + FIntPoint(Dx, Dy));
                    }
                }
            }
            // 生成結果（Plan）の広い空間の一覧で、要素または対応関係を末尾へ追加する（引数：N）。
            Plan.Nodes.Add(N);
        }
        for (int32 Id = 0; Id < Count; ++Id)
        {
            for (int32 Direction = 0; Direction < 2; ++Direction)
            {
                // Other（Other）を符号付き32ビット整数として用意し、「四方向の番号（Direction）が0と等しい」なら空間の識別番号（Id）と1の和、そうでなければ空間の識別番号（Id）と配置する列数（Columns）の和で初期化する。
                const int32 Other = Direction == 0 ? Id + 1 : Id + Columns;
                // 「「Otherが対象の件数（Count）以上」または「「四方向の番号（Direction）が0と等しい」かつ「空間の識別番号（Id）を配置する列数（Columns）で割った余りが配置する列数（Columns）から1を引いた値と等しい」」」に当てはまる場合だけ、続く文を実行する。
                if (Other >= Count || (Direction == 0 && Id % Columns == Columns - 1))
                {
                    continue;
                }
                // 隣接セルへ進む差分（Step）を右辺から自動判定した型として用意し、CourtyardLayoutStepsの「四方向の番号（Direction）」に対応する要素で初期化する。
                const auto Step = CourtyardLayoutSteps[Direction];
                // 部屋同士の間隔（セル）（Gap）を符号付き32ビット整数として用意し、四方向の番号（Direction）が0と等しいで初期化する。
                const int32 Gap = Direction == 0
                                      // 横に隣接する空間なら、基準X座標同士の差から空間自体の大きさを引き、隙間のセル数を求める。
                                      ? TileX[Other % Columns] - TileX[Id % Columns] - Extent
                                      // : TileYの「Otherを配置する列数（Columns）で割った値」に対応する要素から各空間の基準Y座標（TileY）の「空間の識別番号（Id）を配置する列数（Columns）で割った値」に対応する要素を引いた値から1空間の外接範囲の辺長（Extent）を引いた値。
                                      : TileY[Other / Columns] - TileY[Id / Columns] - Extent;
                // Choices（Choices）を配列として宣言する。
                TArray<FCandidate> Choices;
                for (const auto P : Plan.Nodes[Id].FloorCells)
                {
                    // Q（Q）を右辺から自動判定した型として用意し、Pと隣接セルへ進む差分（Step）と部屋同士の間隔（セル）（Gap）と1の和の積の和で初期化する。
                    const auto Q = P + Step * (Gap + 1);
                    // 「「「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Other」に対応する要素、Q）」が成立しない」または「「セルが区画の端の帯に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「空間の識別番号（Id）」に対応する要素、P）」が成立しない」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
                    if (!Contains(Plan.Nodes[Other], Q) || !AtSectionEnd(Plan.Nodes[Id], P) ||
                        // この行では「セルが区画の端の帯に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Other」に対応する要素、Q）」が成立しないを指定し、前後の行の計算・呼び出しへつなげる。
                        !AtSectionEnd(Plan.Nodes[Other], Q))
                    {
                        continue;
                    }
                    // 両端の間はタイル間の空白だけ。側壁沿いの中途半端な入り口は作らない。
                    // 「「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「空間の識別番号（Id）」に対応する要素、Pと隣接セルへ進む差分（Step）の和）」または「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Other」に対応する要素、Qから隣接セルへ進む差分（Step）を引いた値）」」に当てはまる場合だけ、続く文を実行する。
                    if (Contains(Plan.Nodes[Id], P + Step) || Contains(Plan.Nodes[Other], Q - Step))
                    {
                        continue;
                    }
                    // C（C）をFCandidateとして宣言する。
                    FCandidate C;
                    // Cの各項目を、以下で計算・取得した値へまとめて設定する。
                    C.A = Id;
                    C.B = Other;
                    C.Direction = Direction;
                    for (int32 J = 1; J <= Gap; ++J)
                    {
                        // Cのセル列で、要素または対応関係を末尾へ追加する（引数：Pと隣接セルへ進む差分（Step）とJの積の和）。
                        C.Cells.Add(P + Step * J);
                    }
                    // Choicesで、要素または対応関係を末尾へ追加する（引数：C）。
                    Choices.Add(C);
                }
                // 「「Choicesで、空かを調べる」が成立しない」に当てはまる場合だけ、続く文を実行する。
                if (!Choices.IsEmpty())
                {
                    // First（First）を符号付き32ビット整数として用意し、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、Choicesで、要素数を得るから1を引いた値）で初期化する。
                    const int32 First = Random.RandRange(0, Choices.Num() - 1);
                    // 接続や入口の候補（Candidates）で、要素または対応関係を末尾へ追加する（引数：Choicesの「First」に対応する要素）。
                    Candidates.Add(Choices[First]);
                    for (const auto& Choice : Choices)
                    {
                        // 「FMath::Abs(Choice.Cells[0].X - Choices[First].Cells[0].X) +」に当てはまる場合だけ、続く文を実行する。
                        if (FMath::Abs(Choice.Cells[0].X - Choices[First].Cells[0].X) +
                                // 入口候補のY方向の離れ具合も絶対値で加え、近すぎる入口が集中しないか比較する。
                                FMath::Abs(Choice.Cells[0].Y - Choices[First].Cells[0].Y) >=
                            // この行では幅（Width）と2の和を指定し、前後の行の計算・呼び出しへつなげる。
                            Width + 2)
                        {
                            // 接続や入口の候補（Candidates）で、要素または対応関係を末尾へ追加する（引数：Choice）。
                            Candidates.Add(Choice);
                            break;
                        }
                    }
                }
            }
        }
        // 候補グラフ自体に複数の周回と選択肢がなければ、この向きは採用しない。
        // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する。
        TSet<int32> Seen{0};
        // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
        TArray<int32> Queue{0};
        // Degree（Degree）を配列として宣言する。
        TArray<int32> Degree;
        // Degreeで、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
        Degree.Init(0, Count);
        for (const auto& C : Candidates)
        {
            // Degreeの「CのA」に対応する要素を1増やす。
            ++Degree[C.A];
            // Degreeの「CのB」に対応する要素を1増やす。
            ++Degree[C.B];
        }
        for (int32 I = 0; I < Queue.Num(); ++I)
        {
            for (const auto& C : Candidates)
            {
                // 次に処理する位置または添字（Next）を符号付き32ビット整数として用意し、「CのAが幅優先探索の待ち行列（Queue）の「I」に対応する要素と等しい」ならCのB、そうでなければ「CのBが幅優先探索の待ち行列（Queue）の「I」に対応する要素と等しい」ならCのA、そうでなければINDEX_NONE（未指定を表す-1）で初期化する。
                const int32 Next = C.A == Queue[I] ? C.B : C.B == Queue[I] ? C.A : INDEX_NONE;
                // 「「次に処理する位置または添字（Next）がINDEX_NONE（未指定を表す-1）と異なる」かつ「「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」が成立しない」」に当てはまる場合だけ、続く文を実行する。
                if (Next != INDEX_NONE && !Seen.Contains(Next))
                {
                    // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                    Seen.Add(Next);
                    // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                    Queue.Add(Next);
                }
            }
        }
        bool AllTwo = true;
        int32 Branches = 0;
        for (int32 D : Degree)
        {
            // 全室に入口が2個以上あるかの判定（AllTwo）へ、Dが2以上とのビットANDで両方にあるビットだけ残す。
            AllTwo &= D >= 2;
            // Dが3以上という判定を、trueなら1・falseなら0として分岐の件数（Branches）へ加算する。
            Branches += D >= 3;
        }
        // 「「「「探索済みの集合（Seen）で、要素数を得るが対象の件数（Count）と等しい」かつ「全室に入口が2個以上あるかの判定（AllTwo）」」かつ「分岐の件数（Branches）が4以上」」かつ「接続や入口の候補（Candidates）で、要素数を得るから対象の件数（Count）を引いた値と1の和が3以上」」に当てはまる場合だけ、続く文を実行する。
        if (Seen.Num() == Count && AllTwo && Branches >= 4 && Candidates.Num() - Count + 1 >= 3)
        {
            break;
        }
        // 「全体の再試行番号（Attempt）が32と等しい」に当てはまる場合だけ、続く文を実行する。
        if (Attempt == 32)
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Cannot construct a branching maze"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Cannot construct a branching maze"));
        }
    }
    for (int32 I = Candidates.Num() - 1; I > 0; --I)
    {
        // 接続や入口の候補（Candidates）で、指定した2つの添字の要素を交換する（引数：I、再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、I））。
        Candidates.Swap(I, Random.RandRange(0, I));
    }
    // Parent（Parent）、Degree（Degree）を、それぞれ配列として用意する。
    TArray<int32> Parent, Degree;
    // Degreeで、指定件数の要素を同じ初期値で埋める（引数：0、対象の件数（Count））。
    Degree.Init(0, Count);
    for (int32 I = 0; I < Count; ++I)
    {
        // Parentで、要素または対応関係を末尾へ追加する（引数：I）。
        Parent.Add(I);
    }
    // Actorの基準となるルート部品（Root）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Root = [&](int32 V)
    {
        // 「Parentの「V」に対応する要素がVと異なる」が成り立つ間、続く文を繰り返す。
        while (Parent[V] != V)
        {
            // Vを、Parentの「V」に対応する要素に設定する。
            V = Parent[V];
        }
        // Vを呼び出し元へ返す。
        return V;
    };
    // 採用済み候補の添字一覧（Selected）を配列として宣言する。
    TArray<int32> Selected;
    // Select（Select）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Select = [&](int32 I)
    {
        // 採用済み候補の添字一覧（Selected）で、要素または対応関係を末尾へ追加する（引数：I）。
        Selected.Add(I);
        // Degree[Candidates[I].A]を1増やす。
        ++Degree[Candidates[I].A];
        // Degree[Candidates[I].B]を1増やす。
        ++Degree[Candidates[I].B];
    };
    // 全域木で孤立をなくし、その後で戻り道を足す。単なる一筆書きには戻さない。
    for (int32 I = 0; I < Candidates.Num(); ++I)
    {
        // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& C = Candidates[I];
        // A（A）にRoot(C.A)を格納する。B（B）にRoot(C.B)を格納する。
        const int32 A = Root(C.A), B = Root(C.B);
        // 「AがBと異なる」に当てはまる場合だけ、続く文を実行する。
        if (A != B)
        {
            // Parentの「A」に対応する要素を、Bに設定する。
            Parent[A] = B;
            // 候補Iを採用し、選択一覧・部屋ごとの接続数・接続済みグループを更新する。
            Select(I);
        }
    }
    for (int32 I = 0; I < Candidates.Num(); ++I)
    {
        // 「「採用済み候補の添字一覧（Selected）で、指定値が登録されているか調べる（引数：I）」が成立しない」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
        if (!Selected.Contains(I) &&
            // この行では再現可能な乱数生成器（Random）で、0以上1未満の小数をSeedに従って抽選するが指定した最小値と最大値の範囲へ収める（引数：Sの旧方式で余分な短い接続を追加する確率、0.f、1.f）より小さいを指定し、前後の行の計算・呼び出しへつなげる。
            Random.FRand() < FMath::Clamp(S.ExtraConnectionChance, 0.f, 1.f))
        {
            // 分岐や周回を満たすため、この短い接続候補Iを採用する。
            Select(I);
        }
    }
    for (int32 I = 0; I < Candidates.Num(); ++I)
    {
        // 「「「採用済み候補の添字一覧（Selected）で、指定値が登録されているか調べる（引数：I）」が成立しない」かつ「「Degree[Candidates[I].A]が2より小さい」または「Degree[Candidates[I].B]が2より小さい」」」に当てはまる場合だけ、続く文を実行する。
        if (!Selected.Contains(I) && (Degree[Candidates[I].A] < 2 || Degree[Candidates[I].B] < 2))
        {
            // 必要な接続条件を満たす候補Iを、迷路の接続へ追加する。
            Select(I);
        }
    }
    // BranchCount（BranchCount）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto BranchCount = [&]()
    {
        int32 N = 0;
        for (int32 D : Degree)
        {
            // Dが3以上という判定を、trueなら1・falseなら0としてNへ加算する。
            N += D >= 3;
        }
        // Nを呼び出し元へ返す。
        return N;
    };
    for (int32 I = 0; I < Candidates.Num() && (Selected.Num() - Count + 1 < 3 || BranchCount() < 4);
        // Iを1増やす。
        ++I)
    {
        // 「「採用済み候補の添字一覧（Selected）で、指定値が登録されているか調べる（引数：I）」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!Selected.Contains(I))
        {
            // 入口を増やす必要がある自由端を、この候補Iを使って別の空間へつなぐ。
            Select(I);
        }
    }
    // 候補がある自由端は必ず別の空間へつなぐ。全域木だけでは腕の袋小路は防げない。
    for (const auto& N : Plan.Nodes)
    {
        for (int32 End : {0, N.Sections.Num() - 1})
        {
            // R（R）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「End」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& R = N.Sections[End];
            // 隣とつながる側の区画（Join）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「「Endが0と等しい」なら1、そうでなければEndから1を引いた値」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& Join = N.Sections[End == 0 ? 1 : End - 1];
            // 長手方向の軸（Axis）に区画が横長か縦長かを求める（引数：R）を格納する。WidthR（WidthR）に指定した軸の座標成分を取り出す（引数：Rの大きさ、1から長手方向の軸（Axis）を引いた値）を格納する。
            const int32 Axis = SectionAxis(R), WidthR = Along(R.Size, 1 - Axis),
                        // 長さ（Length）を、指定した軸の座標成分を取り出す（引数：Rの大きさ、長手方向の軸（Axis））に設定する。
                        Length = Along(R.Size, Axis);
            // 区画の長手方向の手前側で接続しているか（JoinedAtStart）を真偽値として用意し、指定した軸の座標成分を取り出す（引数：隣とつながる側の区画（Join）の最小端、長手方向の軸（Axis））が指定した軸の座標成分を取り出す（引数：Rの最小端、長手方向の軸（Axis））以下で初期化する。
            const bool JoinedAtStart = Along(Join.Min, Axis) <= Along(R.Min, Axis);
            // Covers（Covers）を、周囲の値を使って判定・計算する小さな関数として定義する。
            const auto Covers = [&](int32 Index)
            {
                // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「配列の添字（Index）」に対応する要素で初期化する。元の値を参照し、コピーしない。
                const auto& C = Candidates[Index];
                // P（P）を2次元の整数セル座標として用意し、CのAがNのId ? Cのセル列の「0」に対応する要素からCourtyardLayoutStepsの「Cの方向番号」に対応する要素を引いた値と等しいで初期化する。
                const FIntPoint P = C.A == N.Id ? C.Cells[0] - CourtyardLayoutSteps[C.Direction]
                                    // 接続元でない場合は、この接続の到達側の部屋IDが対象空間と同じかを調べる。
                                    : C.B == N.Id
                                        // 対象が到達側なら、トンネル末尾から進行方向へ1セル進んだ室内側の入口を使う。
                                        ? C.Cells.Last() + CourtyardLayoutSteps[C.Direction]
                                        // この関数の役割：2次元の整数セル座標を組み立てる。ここでは呼び出し方を宣言する。
                                        : FIntPoint(MAX_int32, MAX_int32);
                // 「「セルが長方形区画の内側か調べる（引数：P、R）」が成立しない」に当てはまる場合だけ、続く文を実行する。
                if (!InRect(P, R))
                {
                    return false;
                }
                // T（T）を符号付き32ビット整数として用意し、指定した軸の座標成分を取り出す（引数：PからRの最小端を引いた値、長手方向の軸（Axis））で初期化する。
                const int32 T = Along(P - R.Min, Axis);
                // 「区画の長手方向の手前側で接続しているか（JoinedAtStart）」ならTが長さ（Length）からWidthRを引いた値以上、そうでなければTがWidthRより小さいを呼び出し元へ返す。
                return JoinedAtStart ? T >= Length - WidthR : T < WidthR;
            };
            bool Covered = false;
            for (int32 I : Selected)
            {
                // 今回の接続で覆えた自由端のビット情報（Covered）へ、Covers(I)とのビットORで該当するビットを残す。
                Covered |= Covers(I);
            }
            if (!Covered)
            {
                for (int32 I = 0; I < Candidates.Num(); ++I)
                {
                    // 「Covers(I)」に当てはまる場合だけ、続く文を実行する。
                    if (Covers(I))
                    {
                        // 不要な末端の短縮後も接続可能な短い経路候補Iを、必要に応じて追加する。
                        Select(I);
                        break;
                    }
                }
            }
        }
    }

    // RouteCandidates（RouteCandidates）を配列として宣言する。
    TArray<FCourtyardLink> RouteCandidates;
    for (const auto& C : Candidates)
    {
        // L（L）を2室間の接続情報として宣言する。
        FCourtyardLink L;
        // Lの各項目を、以下で計算・取得した値へまとめて設定する。
        L.From = C.A;
        L.To = C.B;
        L.Direction = C.Direction;
        L.Axis = C.Direction % 2;
        L.Cells = C.Cells;
        // RouteCandidatesで、要素または対応関係を末尾へ追加する（引数：コピーせず移動できる形で値を渡す（引数：L））。
        RouteCandidates.Add(MoveTemp(L));
    }
    // 見通しのよい短い周回を減らし、戻り道を残して接続を調整する（引数：生成結果（Plan）の広い空間の一覧、RouteCandidates、採用済み候補の添字一覧（Selected）、Degree、S、再現用Seed（Seed））。
    ImproveMazeConnections(Plan.Nodes, RouteCandidates, Selected, Degree, S, Seed);
    for (int32 I : Selected)
    {
        // C（C）を右辺から自動判定した型として用意し、接続や入口の候補（Candidates）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& C = Candidates[I];
        // L（L）を2室間の接続情報として宣言する。
        FCourtyardLink L;
        // Lの各項目を、以下で計算・取得した値へまとめて設定する。
        L.From = C.A;
        L.To = C.B;
        L.Direction = C.Direction;
        L.Axis = C.Direction % 2;
        L.Cells = C.Cells;
        // 生成結果（Plan）のLinksで、要素または対応関係を末尾へ追加する（引数：L）。
        Plan.Links.Add(L);
    }
    // 接続先が存在しない外周の長い腕は、短い出っ張りまで戻す。
    for (auto& N : Plan.Nodes)
    {
        // 部屋の出入口（Ports）を配列として宣言する。
        TArray<FIntPoint> Ports;
        for (const auto& L : Plan.Links)
        {
            // 「Lの接続元IDがNの識別番号と等しい」に当てはまる場合だけ、続く文を実行する。
            if (L.From == N.Id)
            {
                // 部屋の出入口（Ports）で、要素または対応関係を末尾へ追加する（引数：Lのセル列の「0」に対応する要素からCourtyardLayoutStepsの「Lの方向番号」に対応する要素を引いた値）。
                Ports.Add(L.Cells[0] - CourtyardLayoutSteps[L.Direction]);
            }
            // 「Lの接続先IDがNの識別番号と等しい」に当てはまる場合だけ、続く文を実行する。
            if (L.To == N.Id)
            {
                // 部屋の出入口（Ports）で、要素または対応関係を末尾へ追加する（引数：Lのセル列で、末尾の要素を取得するとCourtyardLayoutStepsの「Lの方向番号」に対応する要素の和）。
                Ports.Add(L.Cells.Last() + CourtyardLayoutSteps[L.Direction]);
            }
        }
        // 出口のない長い腕を調べ、指定されていれば短縮する（引数：N、部屋の出入口（Ports）、true（成立・有効））。
        FixBlindEnds(N, Ports, true);
        N.FloorCells.Reset();
        // 範囲の最小端（Min）を、括弧内の値を使って2次元の整数セル座標として初期化する。
        FIntPoint Min(MAX_int32, MAX_int32), Max(MIN_int32, MIN_int32);
        for (const auto& R : N.Sections)
        {
            for (int32 X = 0; X < R.Size.X; ++X)
            {
                for (int32 Y = 0; Y < R.Size.Y; ++Y)
                {
                    // P（P）を右辺から自動判定した型として用意し、Rの最小端と2次元の整数セル座標を組み立てる（引数：X、Y）の和で初期化する。
                    auto P = R.Min + FIntPoint(X, Y);
                    // Nの実床セル一覧で、同じ要素がなければ追加する（引数：P）。
                    N.FloorCells.AddUnique(P);
                    // Minの各項目を、以下で計算・取得した値へまとめて設定する。
                    Min.X = FMath::Min(Min.X, P.X);
                    Min.Y = FMath::Min(Min.Y, P.Y);
                    // Maxの各項目を、以下で計算・取得した値へまとめて設定する。
                    Max.X = FMath::Max(Max.X, P.X);
                    Max.Y = FMath::Max(Max.Y, P.Y);
                }
            }
        }
        // Nの各項目を、以下で計算・取得した値へまとめて設定する。
        N.Min = Min;
        N.Size = Max - Min + FIntPoint(1, 1);
    }
    // 床セルの一覧または索引（Cells）をキーで値を引く表として宣言する。
    TMap<FIntPoint, FCourtyardCell> Cells;
    for (const auto& N : Plan.Nodes)
    {
        for (const auto P : N.FloorCells)
        {
            // C（C）を床セル1個の情報として宣言する。
            FCourtyardCell C;
            // Cの各項目を、以下で計算・取得した値へまとめて設定する。
            C.Position = P;
            C.Kind = N.Kind;
            C.NodeId = N.Id;
            int32 Members = 0;
            for (int32 I = 0; I < N.Sections.Num(); ++I)
            {
                // 「セルが長方形区画の内側か調べる（引数：P、Nの長方形区画一覧の「I」に対応する要素）」に当てはまる場合だけ、続く文を実行する。
                if (InRect(P, N.Sections[I]))
                {
                    ++Members;
                    // Cの所属する長方形区画の添字を、Iに設定する。
                    C.SectionIndex = I;
                }
            }
            // 「Membersが1より大きい」に当てはまる場合だけ、続く文を実行する。
            if (Members > 1)
            {
                // Cの所属する長方形区画の添字を、INDEX_NONE（未指定を表す-1）に設定する。
                C.SectionIndex = INDEX_NONE;
            }
            // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：P、C）。
            Cells.Add(P, C);
        }
    }
    for (int32 Id = 0; Id < Plan.Links.Num(); ++Id)
    {
        for (const auto P : Plan.Links[Id].Cells)
        {
            // 「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：P）」に当てはまる場合だけ、続く文を実行する。
            if (Cells.Contains(P))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Maze tunnel overlaps a floor"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Maze tunnel overlaps a floor"));
            }
            // C（C）を床セル1個の情報として宣言する。
            FCourtyardCell C;
            // Cの各項目を、以下で計算・取得した値へまとめて設定する。
            C.Position = P;
            C.Kind = ECourtyardCellKind::Tunnel;
            C.LinkId = Id;
            C.Axis = Plan.Links[Id].Axis;
            // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：P、C）。
            Cells.Add(P, C);
        }
    }
    // 骨格と接続は完成済み。別タイルやトンネルに触れない内側だけを拡幅する。
    // 新しい長い枝を生まず、どの追加セルも元の道から1～2歩以内に収める。
    // CanAddDetail（CanAddDetail）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto CanAddDetail = [&](int32 Id, FIntPoint P, bool PreserveBend = false)
    {
        // 部屋候補も検証側と同じ距離制限で選ぶ。遠い角を採用してから失敗させない。
        bool NearSkeleton = false;
        for (const auto& R : Plan.Nodes[Id].Sections)
        {
            // DX（DX）を符号付き32ビット整数として用意し、で初期化する。
            const int32 DX =
                // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Rの最小端のX成分からPのX成分を引いた値、PのX成分からRの最小端のX成分とRの大きさのX成分の和から1を引いた値を引いた値））。
                FMath::Max(0, FMath::Max(R.Min.X - P.X, P.X - (R.Min.X + R.Size.X - 1)));
            // DY（DY）を符号付き32ビット整数として用意し、で初期化する。
            const int32 DY =
                // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Rの最小端のY成分からPのY成分を引いた値、PのY成分からRの最小端のY成分とRの大きさのY成分の和から1を引いた値を引いた値））。
                FMath::Max(0, FMath::Max(R.Min.Y - P.Y, P.Y - (R.Min.Y + R.Size.Y - 1)));
            // NearSkeletonへ、DXとDYの和が2以下とのビットORで該当するビットを残す。
            NearSkeleton |= DX + DY <= 2;
        }
        if (!NearSkeleton)
        {
            return false;
        }
        // 配置または計測の基準位置（Origin）を2次元の整数セル座標として用意し、GetStaggeredOrigin(で初期化する。
        const FIntPoint Origin = GetStaggeredOrigin(
            // 対象IDの列・行から元の配置座標を作り、同じSeedによるずらし量を再現して取得する。
            FIntPoint(TileX[Id % Columns], TileY[Id / Columns]), Id, Columns, Seed, S);
        // 「「「PのX成分が配置または計測の基準位置（Origin）のX成分以下」または「PのY成分が配置または計測の基準位置（Origin）のY成分以下」」または「PのX成分が配置または計測の基準位置（Origin）のX成分と1空間の外接範囲の辺長（Extent）の和から1を引いた値以上」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (P.X <= Origin.X || P.Y <= Origin.Y || P.X >= Origin.X + Extent - 1 ||
            // この行ではPのY成分が配置または計測の基準位置（Origin）のY成分と1空間の外接範囲の辺長（Extent）の和から1を引いた値以上を指定し、前後の行の計算・呼び出しへつなげる。
            P.Y >= Origin.Y + Extent - 1)
        {
            return false;
        }
        for (const auto D : CourtyardLayoutSteps)
        {
            // Other（Other）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：PとDの和）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* Other = Cells.Find(P + D);
            // 「「Other」かつ「「Otherの所属部屋IDが空間の識別番号（Id）と異なる」または「Otherの床の種別がECourtyardCellKind::Tunnelと等しい」」」に当てはまる場合だけ、続く文を実行する。
            if (Other && (Other->NodeId != Id || Other->Kind == ECourtyardCellKind::Tunnel))
            {
                return false;
            }
        }
        // 片側の張り出しで向かいの道まで埋めず、曲がって戻る経路を残す。
        for (int32 D = 0; D < 2; ++D)
        {
            // 「「PreserveBend」かつ「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：PとCourtyardLayoutStepsの「D」に対応する要素の和）」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
            if (PreserveBend && Cells.Contains(P + CourtyardLayoutSteps[D]) &&
                // この行では床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：PからCourtyardLayoutStepsの「D」に対応する要素を引いた値）を指定し、前後の行の計算・呼び出しへつなげる。
                Cells.Contains(P - CourtyardLayoutSteps[D]))
            {
                return false;
            }
        }
        // 「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：P）」が成立しないを呼び出し元へ返す。
        return !Cells.Contains(P);
    };
    // AddDetail（AddDetail）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto AddDetail = [&](int32 Id, FIntPoint P)
    {
        // N（N）を右辺から自動判定した型として用意し、生成結果（Plan）の広い空間の一覧の「空間の識別番号（Id）」に対応する要素で初期化する。元の値を参照し、コピーしない。
        auto& N = Plan.Nodes[Id];
        // Nの実床セル一覧で、要素または対応関係を末尾へ追加する（引数：P）。
        N.FloorCells.Add(P);
        // Nの短い張り出しとして追加した床で、要素または対応関係を末尾へ追加する（引数：P）。
        N.DetailCells.Add(P);
        // C（C）を床セル1個の情報として宣言する。
        FCourtyardCell C;
        // Cの各項目を、以下で計算・取得した値へまとめて設定する。
        C.Position = P;
        C.NodeId = Id;
        C.Kind = ECourtyardCellKind::Street;
        // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：P、C）。
        Cells.Add(P, C);
    };
    // DetailRandom（DetailRandom）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
    FRandomStream DetailRandom(Seed ^ 0x416C6C79);
    // 幅4セルの小部屋を曲がり角に置く。1角を欠いてL/凸形にし、長方形の大広間を避ける。
    for (int32 Id : RoomIds)
    {
        // 「生成結果（Plan）のRoomAreasで、要素数を得るがRoomCount以上」に当てはまる場合だけ、続く文を実行する。
        if (Plan.RoomAreas.Num() >= RoomCount)
        {
            break;
        }
        // N（N）を右辺から自動判定した型として用意し、生成結果（Plan）の広い空間の一覧の「空間の識別番号（Id）」に対応する要素で初期化する。元の値を参照し、コピーしない。
        auto& N = Plan.Nodes[Id];
        bool Added = false;
        for (int32 Part = 1; Part < N.Sections.Num() && !Added; ++Part)
        {
            // A（A）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「Partから1を引いた値」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& A = N.Sections[Part - 1];
            // B（B）を右辺から自動判定した型として用意し、Nの長方形区画一覧の「Part」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& B = N.Sections[Part];
            // Corner（Corner）を、括弧内の値を使って2次元の整数セル座標として初期化する。
            const FIntPoint Corner(FMath::Max(A.Min.X, B.Min.X), FMath::Max(A.Min.Y, B.Min.Y));
            for (int32 DX = -2; DX <= 0 && !Added; ++DX)
            {
                for (int32 DY = -2; DY <= 0 && !Added; ++DY)
                {
                    // Area（Area）を屋内部屋の床と扉の情報として宣言する。
                    FCourtyardRoomArea Area;
                    // Areaの各項目を、以下で計算・取得した値へまとめて設定する。
                    Area.Min = Corner + FIntPoint(DX, DY);
                    Area.Size = {4, 4};
                    bool Fits = true;
                    int32 Existing = 0;
                    for (int32 X = 0; X < 4; ++X)
                    {
                        for (int32 Y = 0; Y < 4; ++Y)
                        {
                            // P（P）を右辺から自動判定した型として用意し、Areaの最小端と2次元の整数セル座標を組み立てる（引数：X、Y）の和で初期化する。
                            const auto P = Area.Min + FIntPoint(X, Y);
                            // C（C）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：P）で初期化する。参照先がない場合もあるポインターとして扱う。
                            const auto* C = Cells.Find(P);
                            if (C)
                            {
                                // 配置可能かの判定（Fits）へ、「Cの所属部屋IDが空間の識別番号（Id）と等しい」かつ「Cの床の種別がECourtyardCellKind::Streetと等しい」とのビットANDで両方にあるビットだけ残す。
                                Fits &= C->NodeId == Id && C->Kind == ECourtyardCellKind::Street;
                                ++Existing;
                            }
                            // 直前の条件に当てはまらなかった場合の処理へ進む。
                            else
                            {
                                // 配置可能かの判定（Fits）へ、CanAddDetail(Id, P)とのビットANDで両方にあるビットだけ残す。
                                Fits &= CanAddDetail(Id, P);
                            }
                            // Areaの実床セル一覧で、要素または対応関係を末尾へ追加する（引数：P）。
                            Area.FloorCells.Add(P);
                        }
                    }
                    // 「「「配置可能かの判定（Fits）」が成立しない」または「Existingが6より小さい」」に当てはまる場合だけ、続く文を実行する。
                    if (!Fits || Existing < 6)
                    {
                        continue;
                    }
                    // 欠くのは元の道を削らない角。元の接続はすべて残す。
                    bool Cut = false;
                    // 4×4の部屋候補の隅が既存床と矛盾しないかを、4つの相対位置それぞれで確認する。
                    for (const FIntPoint Offset :
                        // 部屋候補の左下・右下・右上・左上に相当する、0または3セルの相対座標を列挙する。
                        {FIntPoint(0, 0), FIntPoint(3, 0), FIntPoint(3, 3), FIntPoint(0, 3)})
                    {
                        // P（P）を右辺から自動判定した型として用意し、Areaの最小端と移動差分（Offset）の和で初期化する。
                        const auto P = Area.Min + Offset;
                        // 「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：P）」に当てはまる場合だけ、続く文を実行する。
                        if (Cells.Contains(P))
                        {
                            continue;
                        }
                        // Areaの実床セル一覧で、指定要素を取り除く（引数：P）。
                        Area.FloorCells.Remove(P);
                        // Q（Q）を右辺から自動判定した型として用意し、Pと2次元の整数セル座標を組み立てる（引数：「移動差分（Offset）のX成分が0と等しい」なら1、そうでなければ-1、0）の和で初期化する。
                        const auto Q = P + FIntPoint(Offset.X == 0 ? 1 : -1, 0);
                        // 「「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：Q）」が成立しない」かつ「DetailRandomで、0以上1未満の小数をSeedに従って抽選するがの5fより小さい」」に当てはまる場合だけ、続く文を実行する。
                        if (!Cells.Contains(Q) && DetailRandom.FRand() < .5f)
                        {
                            // Areaの実床セル一覧で、指定要素を取り除く（引数：Q）。
                            Area.FloorCells.Remove(Q);
                        }
                        Cut = true;
                        break;
                    }
                    if (!Cut)
                    {
                        continue;
                    }
                    // 境界で道へ接する各辺に扉。隣接する別の出口を壁で塞がない。
                    for (auto P : Area.FloorCells)
                    {
                        for (int32 D = 0; D < 4; ++D)
                        {
                            // Q（Q）を右辺から自動判定した型として用意し、PとCourtyardLayoutStepsの「D」に対応する要素の和で初期化する。
                            const auto Q = P + CourtyardLayoutSteps[D];
                            // 「「「Areaの実床セル一覧で、指定値が登録されているか調べる（引数：Q）」が成立しない」かつ「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：Q）」」に当てはまる場合だけ、続く文を実行する。
                            if (!Area.FloorCells.Contains(Q) && Cells.Contains(Q))
                            {
                                // Areaの扉を開ける室内セルで、要素または対応関係を末尾へ追加する（引数：P）。
                                Area.DoorCells.Add(P);
                                // Areaの各扉の外向き方向で、要素または対応関係を末尾へ追加する（引数：D）。
                                Area.DoorDirections.Add(D);
                            }
                        }
                    }
                    // 「Areaの扉を開ける室内セルで、要素数を得るが2より小さい」に当てはまる場合だけ、続く文を実行する。
                    if (Area.DoorCells.Num() < 2)
                    {
                        continue;
                    }
                    for (auto P : Area.FloorCells)
                    {
                        // 「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：P）」が成立しない」に当てはまる場合だけ、続く文を実行する。
                        if (!Cells.Contains(P))
                        {
                            // 部屋IdへセルPを追加し、既存空間から短く広がる部分を床データへ反映する。
                            AddDetail(Id, P);
                        }
                        // 床セルの一覧または索引（Cells）で、登録済みであることを前提にキーの値を取得する（引数：P）の床の種別を、ECourtyardCellKind::Roomに設定する。
                        Cells.FindChecked(P).Kind = ECourtyardCellKind::Room;
                    }
                    // 生成結果（Plan）のRoomAreasで、要素または対応関係を末尾へ追加する（引数：Area）。
                    Plan.RoomAreas.Add(Area);
                    Added = true;
                }
            }
        }
    }
    // 「生成結果（Plan）のRoomAreasで、要素数を得るがRoomCountと異なる」に当てはまる場合だけ、続く文を実行する。
    if (Plan.RoomAreas.Num() != RoomCount)
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Could not place compact irregular rooms"というUE文字列）を呼び出し元へ返す。
        return Fail(Error, TEXT("Could not place compact irregular rooms"));
    }
    // 各タイルに最大2か所。奥行き1セル、長さ2～3セルの浅い凸形を作る。
    for (auto& N : Plan.Nodes)
    {
        // Skeleton（Skeleton）を右辺から自動判定した型として用意し、Nの実床セル一覧で初期化する。
        const auto Skeleton = N.FloorCells;
        int32 Added = 0;
        for (int32 Try = 0; Try < 40 && Added < 2; ++Try)
        {
            // Base（Base）を右辺から自動判定した型として用意し、Skeletonの「DetailRandomで、指定範囲の整数をSeedに従って抽選する（引数：0、Skeletonで、要素数を得るから1を引いた値）」に対応する要素で初期化する。
            const auto Base = Skeleton[DetailRandom.RandRange(0, Skeleton.Num() - 1)];
            // D（D）を符号付き32ビット整数として用意し、DetailRandomで、指定範囲の整数をSeedに従って抽選する（引数：0、3）で初期化する。
            const int32 D = DetailRandom.RandRange(0, 3);
            // 隣接セルへ進む差分（Step）にCourtyardLayoutStepsの「D」に対応する要素を格納する。入口の幅方向（Tangent）にCourtyardLayoutStepsの「Dと1の和を4で割った余り」に対応する要素を格納する。
            const auto Step = CourtyardLayoutSteps[D], Tangent = CourtyardLayoutSteps[(D + 1) % 4];
            // Patch（Patch）を配列として宣言する。
            TArray<FIntPoint> Patch;
            // 長さ（Length）を符号付き32ビット整数として用意し、DetailRandomで、指定範囲の整数をSeedに従って抽選する（引数：2、3）で初期化する。
            const int32 Length = DetailRandom.RandRange(2, 3);
            for (int32 I = 0; I < Length; ++I)
            {
                // P（P）を右辺から自動判定した型として用意し、Baseと入口の幅方向（Tangent）とIの積の和で初期化する。
                const auto P = Base + Tangent * I;
                // 「「「Skeletonで、指定値が登録されているか調べる（引数：P）」が成立しない」または「「CanAddDetail(N.Id, P + Step, true)」が成立しない」」に当てはまる場合だけ、続く文を実行する。
                if (!Skeleton.Contains(P) || !CanAddDetail(N.Id, P + Step, true))
                {
                    Patch.Reset();
                    break;
                }
                // Patchで、要素または対応関係を末尾へ追加する（引数：Pと隣接セルへ進む差分（Step）の和）。
                Patch.Add(P + Step);
            }
            // 「「Patchで、空かを調べる」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!Patch.IsEmpty())
            {
                for (auto P : Patch)
                {
                    // この空間のIDに属するセルPとして、短い張り出しを実床へ追加する。
                    AddDetail(N.Id, P);
                }
                ++Added;
            }
        }
    }
    for (auto& N : Plan.Nodes)
    {
        // 範囲の最小端（Min）にNの最小端を格納する。範囲の最大端（Max）にNの最小端とNの大きさの和から2次元の整数セル座標を組み立てる（引数：1、1）を引いた値を格納する。
        FIntPoint Min = N.Min, Max = N.Min + N.Size - FIntPoint(1, 1);
        for (auto P : N.FloorCells)
        {
            // Minの各項目を、以下で計算・取得した値へまとめて設定する。
            Min.X = FMath::Min(Min.X, P.X);
            Min.Y = FMath::Min(Min.Y, P.Y);
            // Maxの各項目を、以下で計算・取得した値へまとめて設定する。
            Max.X = FMath::Max(Max.X, P.X);
            Max.Y = FMath::Max(Max.Y, P.Y);
        }
        // Nの各項目を、以下で計算・取得した値へまとめて設定する。
        N.Min = Min;
        N.Size = Max - Min + FIntPoint(1, 1);
    }
    // 床セルの一覧または索引（Cells）で、Mapに格納された値を配列として取り出す（引数：生成結果（Plan）のセル列）。
    Cells.GenerateValueArray(Plan.Cells);
    // 生成結果の床セルを座標順に並べ、同じSeedで描画時の選択順も再現する。
    Plan.Cells.Sort(
        // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
        [](const auto& A, const auto& B)
        {
            // Aのセル座標のX成分がBのセル座標のX ? Aのセル座標のX成分がBのセル座標のX成分より小さいと異なるを呼び出し元へ返す。
            return A.Position.X != B.Position.X ? A.Position.X < B.Position.X
                                                // : Aのセル座標のY成分がBのセル座標のY成分より小さい。
                                                : A.Position.Y < B.Position.Y;
        });
    // Planの各項目を、以下で計算・取得した値へまとめて設定する。
    Plan.StartCell = Plan.Nodes[0].FloorCells[0];
    Plan.GoalCell = Plan.StartCell;
    // 歩行距離（Distance）をキーで値を引く表として宣言する。
    TMap<FIntPoint, int32> Distance;
    // 歩行距離（Distance）で、要素または対応関係を末尾へ追加する（引数：生成結果（Plan）のStartCell、0）。
    Distance.Add(Plan.StartCell, 0);
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
    TArray<FIntPoint> Queue{Plan.StartCell};
    int32 Best = 0;
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (const auto D : CourtyardLayoutSteps)
        {
            // 次に処理する位置または添字（Next）を右辺から自動判定した型として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素とDの和で初期化する。
            const auto Next = Queue[I] + D;
            // C（C）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：次に処理する位置または添字（Next））で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* C = Cells.Find(Next);
            // 「「「C」が成立しない」または「歩行距離（Distance）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」」に当てはまる場合だけ、続く文を実行する。
            if (!C || Distance.Contains(Next))
            {
                continue;
            }
            // Dist（Dist）を符号付き32ビット整数として用意し、歩行距離（Distance）で、登録済みであることを前提にキーの値を取得する（引数：幅優先探索の待ち行列（Queue）の「I」に対応する要素）と1の和で初期化する。
            const int32 Dist = Distance.FindChecked(Queue[I]) + 1;
            // 歩行距離（Distance）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next）、Dist）。
            Distance.Add(Next, Dist);
            // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
            Queue.Add(Next);
            // 「「Cの床の種別がECourtyardCellKind::Streetと等しい」かつ「Distが現在の最良候補（Best）より大きい」」に当てはまる場合だけ、続く文を実行する。
            if (C->Kind == ECourtyardCellKind::Street && Dist > Best)
            {
                // 現在の最良候補（Best）を、Distに設定する。
                Best = Dist;
                // 生成結果（Plan）のGoalCellを、次に処理する位置または添字（Next）に設定する。
                Plan.GoalCell = Next;
            }
        }
    }
    // 生成結果（Plan）の独立した周回路の件数を、生成結果（Plan）のLinksで、要素数を得るから生成結果（Plan）の広い空間の一覧で、要素数を得るを引いた値と1の和に設定する。
    Plan.LoopCount = Plan.Links.Num() - Plan.Nodes.Num() + 1;
    // 従来方式の床・部屋形状・トンネル・接続条件を検査する（引数：生成結果（Plan）、失敗理由（Error））を呼び出し元へ返す。
    return ValidateCourtyardPlan(Plan, Error);
}

// この関数の役割：従来方式の床・部屋形状・トンネル・接続条件を検査する。ここから本体を定義する。
bool UCourtyardLayoutLibrary::ValidateCourtyardPlan(const FCourtyardPlan& Plan, FString& Error)
{
    // 「生成結果（Plan）の部屋の塊と枝分かれする細道の方式かの識別」に当てはまる場合だけ、続く文を実行する。
    if (Plan.bOrganicLayout)
        // 部屋と細道の接続・入口・分岐・周回条件を検査する（引数：生成結果（Plan）、失敗理由（Error））を呼び出し元へ返す。
        return ValidateOrganicCourtyardPlan(Plan, Error);
    Error.Reset();
    // 「「「生成結果（Plan）の広い空間の一覧で、要素数を得るが4より小さい」または「生成結果（Plan）の広い空間の一覧で、要素数を得るが36より大きい」」または「生成結果（Plan）のセル列で、空かを調べる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (Plan.Nodes.Num() < 4 || Plan.Nodes.Num() > 36 || Plan.Cells.IsEmpty() ||
        // この行では「「生成結果（Plan）のセル列で、要素数を得るが10000より大きい」または「生成結果（Plan）の採用する直線トンネルの最長セル数が1より小さい」」または「生成結果（Plan）の採用する直線トンネルの最長セル数が3より大きい」を指定し、前後の行の計算・呼び出しへつなげる。
        Plan.Cells.Num() > 10000 || Plan.TunnelCellLimit < 1 || Plan.TunnelCellLimit > 3)
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid courtyard plan size"というUE文字列）を呼び出し元へ返す。
        return Fail(Error, TEXT("Invalid courtyard plan size"));
    }
    // 床セルの一覧または索引（Cells）をキーで値を引く表として宣言する。参照先がない場合もあるポインターとして扱う。
    TMap<FIntPoint, const FCourtyardCell*> Cells;
    for (const auto& C : Plan.Cells)
    {
        // 「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：Cのセル座標）」に当てはまる場合だけ、続く文を実行する。
        if (Cells.Contains(C.Position))
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Duplicate floor"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Duplicate floor"));
        }
        // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：Cのセル座標、Cのアドレス）。
        Cells.Add(C.Position, &C);
    }
    // 「「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：生成結果（Plan）のStartCell）」が成立しない」または「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：生成結果（Plan）のGoalCell）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
    if (!Cells.Contains(Plan.StartCell) || !Cells.Contains(Plan.GoalCell))
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid start/goal"というUE文字列）を呼び出し元へ返す。
        return Fail(Error, TEXT("Invalid start/goal"));
    }
    // Indoor（Indoor）を重複しない集合として宣言する。
    TSet<FIntPoint> Indoor;
    for (const auto& R : Plan.RoomAreas)
    {
        // 「「「「Rの大きさのX成分が2より小さい」または「Rの大きさのY成分が2より小さい」」または「Rの大きさのX成分が4より大きい」」または「Rの大きさのY成分が4より大きい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (R.Size.X < 2 || R.Size.Y < 2 || R.Size.X > 4 || R.Size.Y > 4 ||
            // この行では「「Rの実床セル一覧で、要素数を得るが6より小さい」または「Rの実床セル一覧で、要素数を得るが15より大きい」」または「Rの扉を開ける室内セルで、要素数を得るが2より小さい」を指定し、前後の行の計算・呼び出しへつなげる。
            R.FloorCells.Num() < 6 || R.FloorCells.Num() > 15 || R.DoorCells.Num() < 2 ||
            // この行ではRの扉を開ける室内セルで、要素数を得るがRの各扉の外向き方向で、要素数を得ると異なるを指定し、前後の行の計算・呼び出しへつなげる。
            R.DoorCells.Num() != R.DoorDirections.Num())
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid compact room"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Invalid compact room"));
        }
        // メッシュなどの外接範囲（Bounds）を長方形区画の位置と寸法として宣言する。
        FCourtyardRoomSection Bounds;
        // Boundsの各項目を、以下で計算・取得した値へまとめて設定する。
        Bounds.Min = R.Min;
        Bounds.Size = R.Size;
        for (auto P : R.FloorCells)
        {
            // C（C）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーの値を取得する。未登録なら既定値（引数：P）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* C = Cells.FindRef(P);
            // 「「「「セルが長方形区画の内側か調べる（引数：P、メッシュなどの外接範囲（Bounds））」が成立しない」または「「C」が成立しない」」または「Cの床の種別がECourtyardCellKind::Roomと異なる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (!InRect(P, Bounds) || !C || C->Kind != ECourtyardCellKind::Room ||
                // この行ではIndoorで、指定値が登録されているか調べる（引数：P）を指定し、前後の行の計算・呼び出しへつなげる。
                Indoor.Contains(P))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid indoor floor"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Invalid indoor floor"));
            }
            // Indoorで、要素または対応関係を末尾へ追加する（引数：P）。
            Indoor.Add(P);
        }
        for (int32 I = 0; I < R.DoorCells.Num(); ++I)
        {
            // D（D）を符号付き32ビット整数として用意し、Rの各扉の外向き方向の「I」に対応する要素で初期化する。
            const int32 D = R.DoorDirections[I];
            // P（P）を右辺から自動判定した型として用意し、Rの扉を開ける室内セルの「I」に対応する要素で初期化する。
            const auto P = R.DoorCells[I];
            // 「「「Dが0より小さい」または「Dが3より大きい」」または「「Rの実床セル一覧で、指定値が登録されているか調べる（引数：P）」が成立しない」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (D < 0 || D > 3 || !R.FloorCells.Contains(P) ||
                // この行ではRの実床セル一覧で、指定値が登録されているか調べる（引数：PとCourtyardLayoutStepsの「D」に対応する要素の和）を指定し、前後の行の計算・呼び出しへつなげる。
                R.FloorCells.Contains(P + CourtyardLayoutSteps[D]) ||
                // この行では「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：PとCourtyardLayoutStepsの「D」に対応する要素の和）」が成立しないを指定し、前後の行の計算・呼び出しへつなげる。
                !Cells.Contains(P + CourtyardLayoutSteps[D]))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Room door must open onto another path"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Room door must open onto another path"));
            }
        }
    }
    // 部屋の出入口（Ports）を配列として宣言する。
    TArray<TArray<FIntPoint>> Ports;
    // 部屋の出入口（Ports）で、要素数を指定件数にそろえる（引数：生成結果（Plan）の広い空間の一覧で、要素数を得る）。
    Ports.SetNum(Plan.Nodes.Num());
    for (int32 Id = 0; Id < Plan.Links.Num(); ++Id)
    {
        // L（L）を右辺から自動判定した型として用意し、生成結果（Plan）のLinksの「空間の識別番号（Id）」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& L = Plan.Links[Id];
        // 「「「生成結果（Plan）の広い空間の一覧で、指定値が有効な添字か調べる（引数：Lの接続元ID）」が成立しない」または「「生成結果（Plan）の広い空間の一覧で、指定値が有効な添字か調べる（引数：Lの接続先ID）」が成立しない」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (!Plan.Nodes.IsValidIndex(L.From) || !Plan.Nodes.IsValidIndex(L.To) ||
            // この行では「「Lのセル列で、空かを調べる」または「Lのセル列で、要素数を得るが生成結果（Plan）の採用する直線トンネルの最長セル数より大きい」」または「Lの方向番号が0より小さい」を指定し、前後の行の計算・呼び出しへつなげる。
            L.Cells.IsEmpty() || L.Cells.Num() > Plan.TunnelCellLimit || L.Direction < 0 ||
            // この行では「Lの方向番号が3より大きい」または「Lの方向軸がLの方向番号を2で割った余りと異なる」を指定し、前後の行の計算・呼び出しへつなげる。
            L.Direction > 3 || L.Axis != L.Direction % 2)
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid straight tunnel"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Invalid straight tunnel"));
        }
        // 隣接セルへ進む差分（Step）にCourtyardLayoutStepsの「Lの方向番号」に対応する要素を格納する。A（A）にLのセル列の「0」に対応する要素から隣接セルへ進む差分（Step）を引いた値を格納する。
        const auto Step = CourtyardLayoutSteps[L.Direction], A = L.Cells[0] - Step,
                   // Bを、Lのセル列で、末尾の要素を取得すると隣接セルへ進む差分（Step）の和に設定する。
                   B = L.Cells.Last() + Step;
        // 「「「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Lの接続元ID」に対応する要素、A）」が成立しない」または「「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Lの接続先ID」に対応する要素、B）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
        if (!Contains(Plan.Nodes[L.From], A) || !Contains(Plan.Nodes[L.To], B))
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Tunnel endpoint misses the end of a space"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Tunnel endpoint misses the end of a space"));
        }
        // 部屋の出入口（Ports）の「Lの接続元ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：A）。
        Ports[L.From].Add(A);
        // 部屋の出入口（Ports）の「Lの接続先ID」に対応する要素で、要素または対応関係を末尾へ追加する（引数：B）。
        Ports[L.To].Add(B);
        for (int32 I = 0; I < L.Cells.Num(); ++I)
        {
            // C（C）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーの値を取得する。未登録なら既定値（引数：Lのセル列の「I」に対応する要素）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* C = Cells.FindRef(L.Cells[I]);
            // 「「「「C」が成立しない」または「Cの床の種別がECourtyardCellKind::Tunnelと異なる」」または「Cの接続IDが空間の識別番号（Id）と異なる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (!C || C->Kind != ECourtyardCellKind::Tunnel || C->LinkId != Id ||
                // この行では「Cの方向軸がLの方向軸と異なる」または「「Iが0より大きい」かつ「Lのセル列の「I」に対応する要素がLのセル列の「Iから1を引いた値」に対応する要素と隣接セルへ進む差分（Step）の和と異なる」」を指定し、前後の行の計算・呼び出しへつなげる。
                C->Axis != L.Axis || (I > 0 && L.Cells[I] != L.Cells[I - 1] + Step))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Tunnel must remain straight"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Tunnel must remain straight"));
            }
        }
    }
    for (int32 Id = 0; Id < Plan.Nodes.Num(); ++Id)
    {
        // N（N）を右辺から自動判定した型として用意し、生成結果（Plan）の広い空間の一覧の「空間の識別番号（Id）」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& N = Plan.Nodes[Id];
        // 「「「Nの識別番号が空間の識別番号（Id）と異なる」または「Nの実床セル一覧で、空かを調べる」」または「部屋の出入口（Ports）の「空間の識別番号（Id）」に対応する要素で、要素数を得るが2より小さい」」に当てはまる場合だけ、続く文を実行する。
        if (N.Id != Id || N.FloorCells.IsEmpty() || Ports[Id].Num() < 2)
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"A maze space needs at least two connections"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("A maze space needs at least two connections"));
        }
        // 区画同士の隣接一覧（Adj）を配列として宣言する。
        TArray<TArray<int32>> Adj;
        // 「「長方形区画が端同士でL字の鎖としてつながるか検査する（引数：Nの長方形区画一覧、区画同士の隣接一覧（Adj）、失敗理由（Error））」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!BuildSectionChain(N.Sections, Adj, Error))
        {
            return false;
        }
        for (int32 I = 0; I < Adj.Num(); ++I)
        {
            for (int32 J : Adj[I])
            {
                // 「符号を取り除いた絶対値を求める（引数：IからJを引いた値）が1と異なる」に当てはまる場合だけ、続く文を実行する。
                if (FMath::Abs(I - J) != 1)
                {
                    // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Section order is not a simple chain"というUE文字列）を呼び出し元へ返す。
                    return Fail(Error, TEXT("Section order is not a simple chain"));
                }
            }
        }
        for (const auto P : Ports[Id])
        {
            // 「「セルが区画の端の帯に含まれるか調べる（引数：N、P）」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!AtSectionEnd(N, P))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Tunnel must attach near a section end corner"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Tunnel must attach near a section end corner"));
            }
        }
        // Copy（Copy）を右辺から自動判定した型として用意し、Nで初期化する。
        auto Copy = N;
        // 「「出口のない長い腕を調べ、指定されていれば短縮する（引数：Copy、部屋の出入口（Ports）の「空間の識別番号（Id）」に対応する要素、false（不成立・無効））」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!FixBlindEnds(Copy, Ports[Id], false))
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Long unconnected blind arm"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Long unconnected blind arm"));
        }
        // UniqueDetails（UniqueDetails）を重複しない集合として宣言する。
        TSet<FIntPoint> UniqueDetails;
        for (auto P : N.DetailCells)
        {
            bool NearSkeleton = false;
            for (const auto& R : N.Sections)
            {
                // DX（DX）を符号付き32ビット整数として用意し、で初期化する。
                const int32 DX =
                    // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Rの最小端のX成分からPのX成分を引いた値、PのX成分からRの最小端のX成分とRの大きさのX成分の和から1を引いた値を引いた値））。
                    FMath::Max(0, FMath::Max(R.Min.X - P.X, P.X - (R.Min.X + R.Size.X - 1)));
                // DY（DY）を符号付き32ビット整数として用意し、で初期化する。
                const int32 DY =
                    // 大きい方の値を選ぶ（引数：0、大きい方の値を選ぶ（引数：Rの最小端のY成分からPのY成分を引いた値、PのY成分からRの最小端のY成分とRの大きさのY成分の和から1を引いた値を引いた値））。
                    FMath::Max(0, FMath::Max(R.Min.Y - P.Y, P.Y - (R.Min.Y + R.Size.Y - 1)));
                // NearSkeletonへ、DXとDYの和が2以下とのビットORで該当するビットを残す。
                NearSkeleton |= DX + DY <= 2;
            }
            // 「「「「NearSkeleton」が成立しない」または「「Nの実床セル一覧で、指定値が登録されているか調べる（引数：P）」が成立しない」」または「UniqueDetailsで、指定値が登録されているか調べる（引数：P）」」に当てはまる場合だけ、続く文を実行する。
            if (!NearSkeleton || !N.FloorCells.Contains(P) || UniqueDetails.Contains(P))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid shallow widening"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Invalid shallow widening"));
            }
            // UniqueDetailsで、要素または対応関係を末尾へ追加する（引数：P）。
            UniqueDetails.Add(P);
        }
        // Owned（Owned）を重複しない集合として宣言する。
        TSet<FIntPoint> Owned;
        for (auto P : N.FloorCells)
        {
            // C（C）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーの値を取得する。未登録なら既定値（引数：P）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* C = Cells.FindRef(P);
            // 「「「C」が成立しない」または「Cの所属部屋IDが空間の識別番号（Id）と異なる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (!C || C->NodeId != Id ||
                // この行ではCの床の種別が「Indoorで、指定値が登録されているか調べる（引数：P）」ならECourtyardCellKind::Room、そうでなければNの床の種別と異なるを指定し、前後の行の計算・呼び出しへつなげる。
                C->Kind != (Indoor.Contains(P) ? ECourtyardCellKind::Room : N.Kind) ||
                // この行ではOwnedで、指定値が登録されているか調べる（引数：P）を指定し、前後の行の計算・呼び出しへつなげる。
                Owned.Contains(P))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid node ownership"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Invalid node ownership"));
            }
            // Ownedで、要素または対応関係を末尾へ追加する（引数：P）。
            Owned.Add(P);
            // Membership（Membership）に0を格納する。Part（Part）にINDEX_NONE（未指定を表す-1）を格納する。
            int32 Membership = 0, Part = INDEX_NONE;
            for (int32 I = 0; I < N.Sections.Num(); ++I)
            {
                // 「セルが長方形区画の内側か調べる（引数：P、Nの長方形区画一覧の「I」に対応する要素）」に当てはまる場合だけ、続く文を実行する。
                if (InRect(P, N.Sections[I]))
                {
                    ++Membership;
                    // Partを、Iに設定する。
                    Part = I;
                }
            }
            // 「「「Membershipが0と等しい」かつ「「Nの短い張り出しとして追加した床で、指定値が登録されているか調べる（引数：P）」が成立しない」」または「Membershipが2より大きい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if ((Membership == 0 && !N.DetailCells.Contains(P)) || Membership > 2 ||
                // この行ではCの所属する長方形区画の添字が「Membershipが1と等しい」ならPart、そうでなければINDEX_NONE（未指定を表す-1）と異なるを指定し、前後の行の計算・呼び出しへつなげる。
                C->SectionIndex != (Membership == 1 ? Part : INDEX_NONE))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Invalid corner footprint"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Invalid corner footprint"));
            }
        }
        for (const auto& R : N.Sections)
        {
            for (int32 X = 0; X < R.Size.X; ++X)
            {
                for (int32 Y = 0; Y < R.Size.Y; ++Y)
                {
                    // 「「Ownedで、指定値が登録されているか調べる（引数：Rの最小端と2次元の整数セル座標を組み立てる（引数：X、Y）の和）」が成立しない」に当てはまる場合だけ、続く文を実行する。
                    if (!Owned.Contains(R.Min + FIntPoint(X, Y)))
                    {
                        // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Missing section floor"というUE文字列）を呼び出し元へ返す。
                        return Fail(Error, TEXT("Missing section floor"));
                    }
                }
            }
        }
    }
    for (const auto& C : Plan.Cells)
    {
        // 「Cの床の種別がECourtyardCellKind::Tunnelと等しい」に当てはまる場合だけ、続く文を実行する。
        if (C.Kind == ECourtyardCellKind::Tunnel)
        {
            // 「「生成結果（Plan）のLinksで、指定値が有効な添字か調べる（引数：Cの接続ID）」が成立しない」、または次行の条件に当てはまる場合だけ、続く文を実行する。
            if (!Plan.Links.IsValidIndex(C.LinkId) ||
                // この行では「生成結果（Plan）のLinksの「Cの接続ID」に対応する要素のセル列で、指定値が登録されているか調べる（引数：Cのセル座標）」が成立しないを指定し、前後の行の計算・呼び出しへつなげる。
                !Plan.Links[C.LinkId].Cells.Contains(C.Position))
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Orphan tunnel floor"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Orphan tunnel floor"));
            }
            continue;
        }
        // 「「「生成結果（Plan）の広い空間の一覧で、指定値が有効な添字か調べる（引数：Cの所属部屋ID）」が成立しない」または「「セルがその部屋の実床に含まれるか調べる（引数：生成結果（Plan）の広い空間の一覧の「Cの所属部屋ID」に対応する要素、Cのセル座標）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
        if (!Plan.Nodes.IsValidIndex(C.NodeId) || !Contains(Plan.Nodes[C.NodeId], C.Position))
        {
            // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Orphan space floor"というUE文字列）を呼び出し元へ返す。
            return Fail(Error, TEXT("Orphan space floor"));
        }
        for (auto D : CourtyardLayoutSteps)
        {
            // Other（Other）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーの値を取得する。未登録なら既定値（引数：Cのセル座標とDの和）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* Other = Cells.FindRef(C.Position + D);
            // 「「「Other」かつ「Otherの床の種別がECourtyardCellKind::Tunnelと異なる」」かつ「Otherの所属部屋IDがCの所属部屋IDと異なる」」に当てはまる場合だけ、続く文を実行する。
            if (Other && Other->Kind != ECourtyardCellKind::Tunnel && Other->NodeId != C.NodeId)
            {
                // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Spaces touch outside their endpoint tunnels"というUE文字列）を呼び出し元へ返す。
                return Fail(Error, TEXT("Spaces touch outside their endpoint tunnels"));
            }
        }
    }
    // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する。
    TSet<FIntPoint> Seen{Plan.StartCell};
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
    TArray<FIntPoint> Queue{Plan.StartCell};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (auto D : CourtyardLayoutSteps)
        {
            // 次に処理する位置または添字（Next）を右辺から自動判定した型として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素とDの和で初期化する。
            const auto Next = Queue[I] + D;
            // 「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」かつ「「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」が成立しない」」に当てはまる場合だけ、続く文を実行する。
            if (Cells.Contains(Next) && !Seen.Contains(Next))
            {
                // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                Seen.Add(Next);
                // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                Queue.Add(Next);
            }
        }
    }
    // 「探索済みの集合（Seen）で、要素数を得るが床セルの一覧または索引（Cells）で、要素数を得ると異なる」に当てはまる場合だけ、続く文を実行する。
    if (Seen.Num() != Cells.Num())
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error）、"Unreachable floor"というUE文字列）を呼び出し元へ返す。
        return Fail(Error, TEXT("Unreachable floor"));
    }
    int32 Branches = 0;
    for (const auto& P : Ports)
    {
        // Pで、要素数を得るが3以上という判定を、trueなら1・falseなら0として分岐の件数（Branches）へ加算する。
        Branches += P.Num() >= 3;
    }
    // 「「生成結果（Plan）の独立した周回路の件数が生成結果（Plan）のLinksで、要素数を得るから生成結果（Plan）の広い空間の一覧で、要素数を得るを引いた値と1の和と異なる」または「生成結果（Plan）の独立した周回路の件数が3より小さい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (Plan.LoopCount != Plan.Links.Num() - Plan.Nodes.Num() + 1 || Plan.LoopCount < 3 ||
        // この行では分岐の件数（Branches）が4より小さいを指定し、前後の行の計算・呼び出しへつなげる。
        Branches < 4)
    {
        // Fail(を呼び出し元へ返す。
        return Fail(
            // この関数の役割：UEが使う文字列リテラルを指定する。ここでは呼び出し方を宣言する。
            Error, TEXT("Maze needs at least three independent loops and four decision spaces"));
    }
    return true;
}

// この関数の役割：配置の整列を崩す、小さなセル位置のずれを求める。ここから本体を定義する。
FIntPoint UCourtyardLayoutLibrary::GetStaggeredOrigin(
    // 配置または計測の基準位置（Origin）、int32 Id（int32 Id）、int32 Columns（int32 Columns）、int32 Seed（int32 Seed）、const FCourtyardSettings& Settings)（const FCourtyardSettings& Settings)）を、それぞれ2次元の整数セル座標として用意する。
    FIntPoint Origin, int32 Id, int32 Columns, int32 Seed, const FCourtyardSettings& Settings)
{
    // 「「「「生成設定（Settings）の旧方式で整列を少しずらす設定」が成立しない」または「配置する列数（Columns）が1より小さい」」または「空間の識別番号（Id）が0より小さい」」に当てはまる場合だけ、続く文を実行する。
    if (!Settings.bStaggeredLayout || Columns < 1 || Id < 0)
        // 配置または計測の基準位置（Origin）を呼び出し元へ返す。
        return Origin;
    // Shift（Shift）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Shift = [Seed](int32 Index, uint32 Salt)
    {
        // 「配列の添字（Index）が0と等しい」に当てはまる場合だけ、続く文を実行する。
        if (Index == 0)
            // 0を呼び出し元へ返す。
            return 0;
        // 「配列の添字（Index）が1と等しい」に当てはまる場合だけ、続く文を実行する。
        if (Index == 1)
            // 1を呼び出し元へ返す。
            return 1;
        // Hash（Hash）を符号なし32ビット整数として用意し、符号なし32ビット整数へ変換する（引数：再現用Seed（Seed））とSaltのビットXORと符号なし32ビット整数へ変換する（引数：配列の添字（Index））と0x9e3779b9uの積の和で初期化する。
        uint32 Hash = (uint32(Seed) ^ Salt) + uint32(Index) * 0x9e3779b9u;
        // Hashと= Hash >が16より大きいのビットXOR。
        Hash ^= Hash >> 16;
        // Hashへ、0x7feb352duを掛け合わせる。
        Hash *= 0x7feb352du;
        // Hashと= Hash >が15より大きいのビットXOR。
        Hash ^= Hash >> 15;
        // 符号付き32ビット整数へ変換する（引数：Hashと1uのビットAND）を呼び出し元へ返す。
        return int32(Hash & 1u);
    };
    // A whole row shares X shift; a whole column shares Y shift. Thus the
    // connection-axis gap is unchanged and a width-2 corner still overlaps.
    // One cell is the maximum safe perpendicular shift at the minimum gap.
    // 配置または計測の基準位置（Origin）と2次元の整数セル座標を組み立てる（引数：Shift(Id / Columns, 0x5b941a27u)、Shift(Id % Columns, 0x1d3f6a89u)）の和を呼び出し元へ返す。
    return Origin + FIntPoint(Shift(Id / Columns, 0x5b941a27u), Shift(Id % Columns, 0x1d3f6a89u));
}
