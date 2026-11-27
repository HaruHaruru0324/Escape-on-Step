# 中庭のBP生成版

編集するBP：`/Game/AutoCreateMap/Courtyard/ISMPrototype/Blueprints/BP_Courtyard_Generator_OrganicBP`

元の `BP_Courtyard_Generator_ISM` を親として引き継ぐ。元のBP本体、モデル、マテリアルは変更しない。親の開始演出・敵配置・ゴール配置を使い、`GenerateCourtyardBP` の生成部分を上書きしている。

通常の編集ではPythonや作成用コマンドを使わず、このBPを直接編集してコンパイルする。

## 読み始める順序

1. `GenerateCourtyardBP` → `GenerateOrganicCourtyardBP`：Seedを決め、生成中の落下を防ぎ、繰り返しTimerを開始。
2. `OBStartLayout` → `OBResetAttempt`：設定と作業配列を初期化。
3. `OBTryPlaceRoom`：幅2セルの帯を重ね、5〜6セル四方のL/U字の塊をランダム配置。
4. `OBBeginPacking`：部屋の形を保ち、配置の余白を減らす。
5. `OBMakePairs` → `OBAdvancePair`：近い部屋のペアから接続を検討。
6. `OBStartRoute` → `OBCommitRoute`：経路候補を取得し、採用した接続を床とネットワークへ反映。
7. `OBChooseIndoorRooms`：一部の塊を屋内化。
8. `OBMakeCells` → `OBClassifyTunnel`：床をまとめ、短い直線の接続だけをトンネル化。
9. `OBBeginGoalSearch` → `OBValidatePlan`：歩行距離や接続の大量検査はC++へ渡し、採用・再試行はBPで判断。
10. `OBBeginEntrance` → `OBChooseEntrance`：C++で入口候補を集め、BPで抽選。内側の部屋も候補。選んだ入口から歩行距離を再計算。`OBTestEntrance`は説明用のBP探索側で使用。
11. `OBRunTimer`：完成したResultPlanをISMレンダラーへ渡す。描画完了時は親BPのOnCourtyardReadyへ続く。

## BPで変更する箇所

| 変更したいもの | 関数・設定 |
|---|---|
| 部屋の数 | Settings → StreetCount |
| 部屋の大きさ・L/U字の割合 | OBTryPlaceRoom の5〜6、0.55のノード |
| 部屋間の最初の余白 | OBTryPlaceRoom のGapとNearestの比較 |
| 屋内の割合と上限 | Settings → RoomChance / MaxRooms |
| 接続数・周回条件 | OBAdvancePair とOBValidatePlan。両方の最低条件を揃える |
| トンネル化する最長セル数 | Settings → MaxTunnelCells、OBClassifyTunnel |
| 入口候補と抽選 | OBBeginEntrance / OBChooseEntrance。候補のbAlleyは、この配列では内側の入口かを表す |
| 歩行距離で遠いゴール | OBBeginGoalSearch / OBChooseEntrance内のCompute Walking Distances。bRequireRoomNodeで細道を候補に含めるか指定 |
| 同一Seedの再現／毎回抽選 | Seed。負の値なら毎回抽選 |
| 1回に進める小作業数 | OBOperationsPerTick |
| 1フレームの作業時間の目安 | OBWorkBudgetMilliseconds（既定3ms）。小作業のまとまりの間で判定するため、厳密な上限ではない |

## C++に残したもの

- `CompactBlueprintRooms`：部屋の位置を詰める重い探索。元のC++版と同じ実装を共有。
- `FindBlueprintRoomRoute`：2室間の経路候補を探索。どの部屋をつなぐか、追加するか、何本必要かはBPで判断。
- `CourtyardBlueprintQueries`：入口数・周回数の集計、入口候補の列挙、歩行距離の計算。入口の抽選や接続を追加するかの判断はBP。
- `ValidateOrganicCourtyardPlan`：孤立床・行き止まりなどの基本安全検査。失敗時に作り直すかはBP。
- 内壁の物理的な幅測定、選んだ入口への実際のActor配置、ISMの一括配置、実メッシュの通行検査。
- `RouteCellNoise`：同一Seedで同じ経路コストにするための小さなハッシュ計算。

`bOBUseNativeSearch` をOFFにすると、空白圧縮・経路探索・候補列挙・集計などの中身もBPで進める。既定はON。OFF側は読み解くための実装として残しており、処理時間は増える。

## フレーム分割と失敗時の挙動

`OBRunTimer` は引数・戻り値なしの関数。Set Timer by Function Nameで呼び、1フレーム1回に制限する。

`OBRunTimer`は時間予算内であれば同じフレームで`OBAdvance`を続けて呼び、不要なフレーム待ちを減らす。

`OBAdvance` が `OBPhase` に応じて作業する。Phase 1=部屋、2=BP圧縮、3=接続候補、4=BP経路探索、5=トンネル分類、6=ゴール探索、7=入口選択開始、10=入口候補検査、11=入口から再探索、12=描画開始可能、8=描画へ引き渡し済み。

条件を満たさなければ最大24回まで配置データを作り直す。失敗時はTimerを止め、暗転と移動制限を解除してGenerationErrorへ理由を残す。ループ上限の設定は増やしていない。

## 高速化後の検証

同じ5種類のSeedで、変更前後の部屋・通路・入口候補・開始位置・ゴールを比較し、すべて一致。配置データ計算は約0.56〜1.22秒から約0.06〜0.11秒へ短縮した。入口候補の集計もBPの全セル走査からC++での一括計算へ変更。

同じ4種類のSeedによるPIE開始から描画完了の検証では、以前の約7.5〜9.6秒に対し、再実行時は約1.7〜1.8秒、最初の読み込み込みでは約3.9秒。描画を省いた検証環境の値であり、初回アセット読み込みやゲーム画面の描画負荷で変動する。

全床の物理的到達、入口の中央・左右の通行、床の支持、保存後の通常開始と導入スキップを確認。既存の入口探索の自動テストも通過。

## C++を読む順番

1. `Generation/Courtyard/Layout/CourtyardBlueprintQueries.h`：BPから呼ぶ各計算の入力・戻り値と役割。
2. 同じ場所の `CourtyardBlueprintQueries.cpp`：集計、距離探索、入口候補の受け渡し。処理の塊ごとに空行を設け、判定理由や単位をコメントで説明。
3. `Generation/Courtyard/Layout/CourtyardEntranceLayout.cpp`：入口候補の寸法・周囲の空きの検査。
4. `Generation/Courtyard/ISM/CourtyardBlueprintLayoutBridge.cpp`：BP開始・失敗時の状態管理と、重い探索の呼び出し口。
