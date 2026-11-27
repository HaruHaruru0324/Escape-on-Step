#include "Generation/Courtyard/Layout/CourtyardEntranceLayout.h"

namespace CourtyardEntranceLayout
{
    /**
     * 屋外セルの四方向を調べ、導入通路の実幅・外幅・奥行きを確保できる候補を集める。
     * 候補を返すだけなので、BPは内側の部屋を除外したり、抽選条件を変更したりできる。
     * 部屋の形や接続を変更しない。配列の順序も元の実装から変更しない。
     */
    // 全床を読み、通路が収まる入口候補を列挙する。入力配列はコピーせず参照する。
    bool FindCandidates(const TArray<FCourtyardCell>& FloorCells,
        // 1セルの実際の辺の長さをcm単位で受け取る。セル数との換算に使う。
        float CellSize,
        // 導入通路の内側の幅をcm単位で受け取る。必要な開口幅の基準になる。
        float Width,
        // 導入通路を置くために必要な、入口の外側方向の奥行きをcm単位で受け取る。
        float ApproachDepth,
        // 壁を含む導入通路の外幅をcm単位で受け取り、他の床との重なりを避ける。
        float ApproachWidth,
        // 出力：寸法条件を満たす入口候補を、この配列へ格納して返す。
        TArray<FCourtyardEntranceSelection>& Candidates,
        // 出力：失敗時の理由を文字列で返す。成功時は空文字列にする。
        FString& Error)
    {
        Error.Reset();
        Candidates.Reset();
        // 1セルの実寸と通路幅が、NaNや無限大になっていないことを確認する。
        if (!FMath::IsFinite(CellSize) || !FMath::IsFinite(Width) ||
            // 奥行きも有限であることを確認し、0以下の実寸・幅と負の奥行きを不正とする。
            !FMath::IsFinite(ApproachDepth) || CellSize <= 0 || Width <= 0 || ApproachDepth < 0)
        {
            // 通路やセルの寸法が不正なため候補を計算できない、という理由を記録する。
            Error = TEXT("Invalid entrance dimensions");
            return false;
        }

        // 東・北・西・南へ1セル進む差分を、この順番で定義する。
        const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        // 外壁モジュールの張り出しを避ける余裕を取り、開口の両端は実幅に合わせて塞ぐ。
        // 内幅に半セルの余裕を足してセル数へ切り上げる。外壁の張り出しを避け、最低1セルは開ける。
        const int32 Rows = FMath::Max(1, FMath::CeilToInt((Width + CellSize * .5f) / CellSize));
        // 内幅と外幅の大きい方を、導入通路を置くために必要な幅として使う。
        const float ClearWidth = FMath::Max(Width, ApproachWidth);
        // 全床の最小・最大座標を求めるため、最小側を最大整数、最大側を最小整数で初期化する。
        FIntPoint FloorMin(MAX_int32, MAX_int32), FloorMax(MIN_int32, MIN_int32);
        // セル座標から屋外・屋内・トンネルの種別を引く表を作る。
        TMap<FIntPoint, ECourtyardCellKind> Cells;
        // 全床を配列の元の順番で1セルずつ読み、候補の順番と同じSeedの再現性を保つ。
        for (const auto& Cell : FloorCells)
        {
            // この座標の床の種別を登録し、隣接判定で配列全体を探さずに済むようにする。
            Cells.Add(Cell.Position, Cell.Kind);
            // 全床の最小・最大座標を、X・Yの両方向で更新する。
            FloorMin.X = FMath::Min(FloorMin.X, Cell.Position.X);
            FloorMin.Y = FMath::Min(FloorMin.Y, Cell.Position.Y);
            FloorMax.X = FMath::Max(FloorMax.X, Cell.Position.X);
            FloorMax.Y = FMath::Max(FloorMax.Y, Cell.Position.Y);
        }

        // 全床を配列の元の順番で1セルずつ読み、候補の順番と同じSeedの再現性を保つ。
        for (const auto& Cell : FloorCells)
        {
            // 屋外以外の床には導入通路の入口を作らないため、対象から外す。
            if (Cell.Kind != ECourtyardCellKind::Street)
            {
                continue;
            }

            // 今の屋外セルについて、東・北・西・南の4方向を入口候補として調べる。
            for (int32 Direction = 0; Direction < 4; ++Direction)
            {
                // 入口から迷路の外側へ向かう方向の差分を取得する。
                const FIntPoint Outward = Steps[Direction];
                // 外向き方向を90度回し、入口の幅を並べる方向を作る。
                const FIntPoint Tangent(-Outward.Y, Outward.X);
                // 今調べている入口のセル列・方向・位置を格納する構造体を作る。
                FCourtyardEntranceSelection Candidate;
                // この入口の外向き方向を、後の壁開口や導入通路の回転に使えるように記録する。
                Candidate.OutwardDirection = Direction;
                bool Clear = true;
                // 必要な開口幅のセル数だけ、横並びの床を確認する。
                for (int32 Row = 0; Row < Rows; ++Row)
                {
                    // 候補の起点から幅方向へRowセル進め、今回確認する床座標を求める。
                    const FIntPoint Position = Cell.Position + Tangent * Row;
                    // この座標の種別を表から探す。床がなければnullptrになる。
                    const auto* Kind = Cells.Find(Position);
                    // 床が存在しない、または屋外床ではないセルが幅の途中にあれば、候補を不採用にする。
                    if (!Kind || *Kind != ECourtyardCellKind::Street ||
                        // 開口のすぐ外側に別の床がある場合も、導入通路を差し込む入口にはしない。
                        Cells.Contains(Position + Outward))
                    {
                        Clear = false;
                        break;
                    }
                    // 幅の一部として条件を満たした床セルを、壁を開けるセル列へ追加する。
                    Candidate.Cells.Add(Position);
                }
                // 開口幅の途中で条件違反が見つかった場合は、その候補の空間検査を省く。
                if (!Clear)
                {
                    continue;
                }

                // セル番号を小数で扱う2次元座標へ変換し、開口の境界中央を計算し始める。
                const FVector2D Center = FVector2D(Cell.Position) +
                                         // 幅方向へセル列の半分だけずらし、開口の横方向の中央へ移す。
                                         FVector2D(Tangent) * ((Rows - 1) * .5) +
                                         // さらに外側へ半セルずらし、床の中心から壁がある境界線へ移す。
                                         FVector2D(Outward) * .5;

                // 導入通路の実際の奥行き分だけ、外側に他の床がないかを調べる。
                // 無限の半平面を空ける必要はないため、接続できるくぼみも候補にできる。
                // 他の床が導入通路を置く空間に重ならないか、全床の位置を検査する。
                for (const auto& Other : FloorCells)
                {
                    // 入口の境界中央から、検査する床の中心までの相対座標を求める。
                    const FVector2D Delta = FVector2D(Other.Position) - Center;
                    // 内積で相対座標を外向き方向へ投影し、入口の外側へ何セル離れているかを求める。
                    const double DistanceOut = FVector2D::DotProduct(Delta, FVector2D(Outward));
                    // 入口から幅方向へ何セル離れているかを計算するための値を宣言する。
                    const double DistanceSide =
                        // 幅方向への投影の絶対値を取り、左右どちら側でも正の距離として比較する。
                        FMath::Abs(FVector2D::DotProduct(Delta, FVector2D(Tangent)));
                    // 床が入口の外側にあり、導入通路の奥行きに1セルの余裕を足した範囲へ入るかを調べる。
                    if (DistanceOut > 0 && DistanceOut < ApproachDepth / CellSize + 1 &&
                        // さらに横方向も、通路の外幅の半分に余裕0.6セルを加えた範囲へ入るなら重なりとする。
                        DistanceSide < ClearWidth / (2 * CellSize) + .6)
                    {
                        Clear = false;
                        break;
                    }
                }
                // 開口幅と導入通路の配置空間の両方を確保できた候補だけを採用する。
                if (Clear)
                {
                    // 候補の各項目をまとめて設定する。特殊な項目の扱いは個別の説明を参照。
                    Candidate.BoundaryCenter = FVector(Center.X * CellSize, Center.Y * CellSize, 0);
                    Candidate.bInterior = Center.X > FloorMin.X + .5 &&
                                          // 最大X端からも半セル以上内側にあることを要求する。
                                          Center.X < FloorMax.X - .5 &&
                                          // Y方向の両端からも半セルより内側なら、外周ではなく内側の入口として記録する。
                                          Center.Y > FloorMin.Y + .5 && Center.Y < FloorMax.Y - .5;
                    // 採用候補を元の列挙順で追加する。セル配列を移動し、不要なコピーを避ける。
                    Candidates.Add(MoveTemp(Candidate));
                }
            }
        }

        // 条件を満たす入口が1個もなかったかを調べる。
        if (Candidates.IsEmpty())
        {
            // 通路の幅・奥行きが収まる場所がない、という失敗理由を設定する。
            Error = TEXT("No entrance fits the introductory passage width/depth");
            return false;
        }

        return true;
    }

    /** 元のC++版が使う入口。候補の列挙だけを共有し、抽選と距離探索の挙動は維持する。 */
    bool SelectEntrance(FCourtyardPlan& Plan,
        // 1セルの実際の辺の長さをcm単位で受け取る。セル数との換算に使う。
        float CellSize,
        // 導入通路の内側の幅をcm単位で受け取る。必要な開口幅の基準になる。
        float Width,
        // 導入通路を置くために必要な、入口の外側方向の奥行きをcm単位で受け取る。
        float ApproachDepth,
        // 入口抽選を再現するためのSeedを受け取る。部屋生成用の乱数列とは分けて使う。
        int32 Seed,
        // 出力：抽選された入口のセル列・外向き方向・位置を返す。
        FCourtyardEntranceSelection& Selection,
        // 出力：候補を作れない、または全床へ到達できない理由を返す。
        FString& Error,
        // 壁を含む導入通路の外幅。内幅より大きい場合は、こちらを空間検査に使う。
        float ApproachWidth)
    {
        // 出力を既定状態へ戻し、失敗時に前回の入口を残さない。
        Selection = FCourtyardEntranceSelection();
        // 今回のPlanと寸法に合う入口候補を受け取る配列を作る。
        TArray<FCourtyardEntranceSelection> Candidates;

        // 共通の入口候補検査を呼び、候補を列挙できなかった場合は失敗を返す。
        if (!FindCandidates(
                // 現在の全床と通路寸法を渡し、候補配列と失敗理由を受け取る。
                Plan.Cells, CellSize, Width, ApproachDepth, ApproachWidth, Candidates, Error))
        {
            return false;
        }

        // 選択後の到達性検査のため、座標から床の種別を参照する索引を作る。
        // 東・北・西・南へ1セル進む差分を、この順番で定義する。
        const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        // セル座標から屋外・屋内・トンネルの種別を引く表を作る。
        TMap<FIntPoint, ECourtyardCellKind> Cells;

        // 選択後の到達性検査に備え、現在のPlanの全床セルを読む。
        for (const auto& Cell : Plan.Cells)
        {
            // この座標の床の種別を登録し、隣接判定で配列全体を探さずに済むようにする。
            Cells.Add(Cell.Position, Cell.Kind);
        }

        // レイアウト生成の乱数列には触れず、同じSeedの入口も再現可能にする。
        // Seedに固定値をXORし、部屋生成とは独立した入口用の乱数生成器を作る。
        FRandomStream EntranceRandom(uint32(Seed) ^ 0x61b75d23u);
        // 候補配列の有効な添字を抽選し、その入口を出力へコピーする。
        Selection = Candidates[EntranceRandom.RandRange(0, Candidates.Num() - 1)];
        // Planの各項目を、以下で計算・取得した値へまとめて設定する。
        Plan.StartCell = Selection.Cells[0];
        Plan.GoalCell = Plan.StartCell;

        // 開始セルから各床セルへの最短歩行距離を記録する表を作る。
        TMap<FIntPoint, int32> Distances;
        // 開始セル自身までの距離を0マスとして登録する。
        Distances.Add(Plan.StartCell, 0);
        // 開始セルを入れた探索待ち行列を作る。後ろへ隣接セルを追加して幅優先探索する。
        TArray<FIntPoint> Queue{Plan.StartCell};
        int32 BestDistance = 0;
        // 行列の先頭から順に探索する。途中で追加したセルも最後まで調べる。
        for (int32 Index = 0; Index < Queue.Num(); ++Index)
        {
            // 現在セルの四方向それぞれにある隣接床を調べる。
            for (const auto Step : Steps)
            {
                // 現在セルから指定方向へ1セル進んだ座標を求める。
                const FIntPoint Next = Queue[Index] + Step;
                // 床がない座標、または距離を登録済みの座標を除外する。
                if (!Cells.Contains(Next) || Distances.Contains(Next))
                {
                    continue;
                }
                // 現在セルまでの距離に1マス足し、隣接セルまでの最短距離を求める。
                const int32 Distance = Distances.FindChecked(Queue[Index]) + 1;
                // 初めて到達した隣接セルへ、その最短距離を登録する。
                Distances.Add(Next, Distance);
                // 隣接セルの周囲も後で調べるため、待ち行列へ追加する。
                Queue.Add(Next);
                // 屋外セルだけを、このC++版のゴール候補にする。
                if (Cells.FindChecked(Next) == ECourtyardCellKind::Street &&
                    // 今までの候補より歩行距離が長い場合だけ、ゴールを更新する。
                    Distance > BestDistance)
                {
                    // 新しい候補までの距離を保存して、以後の比較基準にする。
                    BestDistance = Distance;
                    // 新しく見つけた最遠の屋外セルを、Planのゴールへ設定する。
                    Plan.GoalCell = Next;
                }
            }
        }
        // 探索で到達した床の件数と全床件数を比べ、孤立床がないか確認する。
        if (Queue.Num() != Cells.Num())
        {
            // 選んだ入口から全床へ到達できなかった、という理由を設定する。
            Error = TEXT("Entrance cannot reach every floor cell");
            return false;
        }
        return true;
    }
}
