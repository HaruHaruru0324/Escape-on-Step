# 通路ライトの最遠点優先配置

## 変更範囲

`DungeonLightPlacementLibrary` を追加し、`BP_Dugeon_Generator.Generate_Corridor_Lights` に組み込み済み。Blueprint は UE のグラフ取り込み・コンパイル・保存 API を通して保存した。バイナリを直接書き換えていない。部屋ライトのロジック、ライト強度、器具メッシュ設定は維持した。組み込み用の一時C++コードは取り除き、Build.cs の依存関係も元に戻した。

現在の実装は、既存と同じ床タイプ判定 → ローカル候補配列への追加 → 全走査完了時にC++の配置計算 → 返された座標ごとに既存のRect Lightと器具を生成、という流れ。最大追加数は256、距離計算予算は2,000,000、最小距離は既存の `Min_Light_Distance` を使用する。以下は組み込み内容の説明であり、ユーザーによる追加接続は不要。

実際の床生成が `Grid_Pos * Grid_size` をワールド座標として使うため、それと同じXYを候補に使用している。半セルの補正やActor Transformは重ねて適用しない。ライト本体Z=270、器具メッシュZ=290を維持。床生成自体をローカル座標方式へ変更する場合は、候補生成も同時に変更する必要がある。

変更前バックアップ：`Saved/CodexBackups/20261104_CorridorFPS/BP_Dugeon_Generator.before.uasset`。

UE エディターを閉じて `Escape_on_Step_DemoEditor / Development / Win64` を通常ビルドした後、エディターを開く。Blueprint で **Plan Corridor Light Positions** を検索する。新規 UCLASS のため、最初は Live Coding だけで済ませない。

## 1. 候補配列を集める

`Generate_Corridor_Lights` に Vector 配列のローカル変数 `CorridorCandidates` を作成済み。関数呼び出しごとに空配列で初期化される。

既存の `For Each Loop (Grids)` と通路判定はそのまま残す。通路条件を満たした場所で、従来の「既存ライトとの距離判定 → ライト生成」を、**候補ワールド座標を CorridorCandidates に Add** に置き換える。この段階ではライトを作らない。

`Grid_room_ids` が別配列なら、Grids の Array Index がその配列でも有効かを確認してから取得する。部屋ID -1 だけで床の存在まで保証されるとは限らないため、既存の Grid_Type などの条件も維持する。

柱・壁との干渉や端からの余白を判定している場合、その判定も候補収集の段階で行う。今回の C++ は障害物を調べない。

### 座標を一致させる

候補は「実際にライトを置く位置」のワールド座標にする。床の位置の計算式と同じ XY を使い、Z を天井から少し下のライト設置高さにする。

- 床がローカル座標で配置されている場合：床と同じグリッド中心補正とライト高さを適用したローカル Vector → `Get Actor Transform` の `Transform Location` → 候補配列。
- 床の計算結果が既にワールド座標なら、Actor Transform を再適用しない。
- 床が専用 Scene Component を基準にしているなら、その Component Transform を使う。
- 半セルの補正 `Grid_size * 0.5` は、床側が使っている場合だけ同様に加える。無条件に追加するとずれる。

保存済みの `Saved/CodexBackups/20261029_RouteSafety/Generate_Floors.after.t3d` は参考にできるが、最新の通路ライト関数のエクスポートは確認できていない。現在の床生成ノードを基準に合わせること。

既存の `Placed_Light_Positions` もワールド座標である必要がある。部屋ライトの記録がローカル座標なら、関数の直前で一時配列にワールド変換するか、記録時にコンポーネントの `Get World Location` を使用する。異なる座標系を混ぜない。

## 2. 配置計算ノードを接続する

候補収集用 `For Each Loop` の **Completed** から `Plan Corridor Light Positions` を実行する。Loop Body から呼ぶと毎セル再計算になってしまう。

| 入力 | 接続する値 |
|---|---|
| Candidate World Positions | CorridorCandidates |
| Existing Light World Positions | Placed_Light_Positions（部屋ライトを含む） |
| Min Light Distance | 既存の Min_Light_Distance。ワールド単位（cm） |
| Max New Lights | 今回追加する通路ライト数の上限。例：64 |
| Max Distance Evaluations（詳細ピン） | 初期値 2,000,000 |

`Grid_size = 300` なら距離 900 はワールド座標で3セル分に相当する。ただし Actor Scale を変えている場合はこの対応も変わる。

Max New Lights は部屋ライトを含まない追加数。全ライト数の上限にしたいなら `Max(0, TotalLimit - Placed_Light_Positions.Length)` を渡す。内部では追加数を最大256に制限する。

## 3. 返された座標に既存のライトを生成する

```text
候補収集 For Each Loop.Completed
  → Plan Corridor Light Positions
  → For Each Loop（Return Value）
      Loop Body
        → 既存の Add Rect Light Component
        → Is Valid（生成されたコンポーネント）
        → Set World Location（Target = 生成コンポーネント、New Location = Array Element）
        → 既存の向き・強度・色・減衰範囲などの設定
        → Get World Location（生成コンポーネント）
        → Add Unique（Placed_Light_Positions）
```

既存の器具メッシュも同じ位置を基準に配置する。ライト自体の向きやメッシュとの位置差は既存処理を引き継ぐ。

`Add Rect Light Component` の Relative Transform にワールド座標をそのまま入れない。上記のように `Set World Location` を使うか、親コンポーネントの Transform で `Inverse Transform Location` してから Relative Location に渡す。

古い距離判定はこの生成ループには残さない。今回の関数が部屋ライトと新規通路ライトの両方を考慮して選択済み。計算後に別条件で生成を飛ばすと、そのライトがある前提で選んだ配置に穴ができる。干渉する候補は計算前に除外する。

コンポーネント生成に失敗した座標は Placed_Light_Positions に追加しない。再試行が必要なら失敗候補を除外し、実際に存在するライト位置から改めて計算する。無制限に再試行しない。

## 4. 実行順と初期化

```text
生成開始：以前の生成ライトの削除・Placed_Light_Positions の Clear
  → 床・壁・柱などを生成
  → Generate_Room_Lights（既存処理）
  → Generate_Corridor_Lights（今回の変更）
```

部屋ライト生成と通路ライト生成の間では Placed_Light_Positions を Clear しない。再生成時には以前の生成コンポーネントも削除する。既存の初期化ができていれば追加変更不要。

## アルゴリズムと制限

候補を X、Y、Z の昇順に整列し、同じ XY の候補を1つにする（Z が最小の候補を採用）。既存ライトがなければ先頭から開始。同距離もこの座標順で決定するため、元配列の順番に依存しない。多層ダンジョンでは同じ XY の別階も同一扱いになるので階別に呼ぶ。

最初だけ全候補と既存ライトの最小距離を計算し、以降は新しく選んだ位置との距離だけでキャッシュを更新する。距離は XY の二乗で比較し、Z は出力に保存する。最小距離ちょうどは配置可能。同じ位置は距離設定が0でも再選択しない。

負の最小距離は0、不正な数値の位置は除外する。最小距離が非有限値または二乗不能なら空配列を返す。入力配列は変更せず、Actor や Component に触れない同期計算関数。

距離計算予算を超える場合は `Budget Exhausted` が true。初期計算が収まらなければ空配列、途中で尽きた場合はそれまでの有効な配置結果を返す。まず Print String 等で確認する。予算は距離計算回数を制限するもので、配列コピー・ソートの時間やメモリ量の絶対上限ではない。

この方式は平面距離の偏りを抑えるもので、壁越しに光が届くか、すべての通路が明るいかは保証しない。壁で隔てられた近隣通路を別々に照らしたい場合は、領域ごとに候補と参照ライトを分けるなどの追加条件が必要。

## 検証

自動テスト名：`EscapeOnStep.Dungeon.LightPlacement`。決定性、XY距離、境界値、入力保持、重複除去、予算制限、最大配置数、およびキャッシュ結果と全距離再計算結果の一致を検査する。

2026-11-04 の検証結果：UEエディター終了後、UE5.7.4 の Development Editor ビルドはリンクを含め成功。UnrealEditor-Cmd の NullRHI 実行で `EscapeOnStep.Dungeon.LightPlacement` は Success、終了コード0。結果は `Saved/Automation/CorridorLights/index.json`、ログは `Saved/Logs/CorridorLightTests.log` に保存した。

起動ログには既存アセット `Content/Bodycam_VHS_Effect/Materials/Instances/Buildings/MI_Building_Wall.uasset` の保存バージョンが使用エンジンより新しく読み込めないエラー、および LVL_BackRooms の ModelComponent の頂点参照警告も出ている。今回の計算テストは成功しているが、これらのアセットの修正・表示検証は対象外であり変更していない。

組み込み後の検証：保存したBlueprintを別のUEプロセスで読み直し、実際の自動生成マップにあるActorでグリッド・部屋・床・壁・部屋ライト・通路ライトを生成した。1600グリッド、Grid_size=300、Min_Light_Distance=900 の試行で、部屋ライト17個を保持し、候補215か所から通路ライト13個を追加。通路ライトの計算結果と実際のコンポーネント位置が全件一致、全追加ライトの距離条件を満たし、強度500を維持。2回目の通路ライト呼び出しの追加数は0。結果は `Saved/CodexBackups/20261104_CorridorFPS/verification.json`。この検証では部屋ライトの実座標まで確認しておらず、下記の既存不具合を見落としていた。NullRHI（描画なし）なので、ゲーム画面での明るさ・影の見た目は別途確認する。

## 部屋ライトの座標接続修正（2026-11-04）

PIE観測でRectLight 36個のうち部屋ライト24個が `(0,0,270)` に重なっていた。`Generate_Room_Lights` の `Set World Location` と照明メッシュの `SpawnActor StaticMeshActor` はX/Yが未接続で、計算済みの部屋中心を使用していなかった。この未接続はFPS組み込み前のエクスポートにも存在する。

UEエディター上で、Make Vectorに入力しているX/Yの数値出力を、上記2ノードのX/Yへ接続（計4本）。ライトZ=270、メッシュZ=290、強度・距離設定は維持。Blueprintコンパイル成功、保存済み。変更前バックアップは `Saved/CodexBackups/20261104_CorridorFPS/BP_Dugeon_Generator.before_room_position_fix.uasset`。

修正後のPIE試行は部屋23個＋通路10個＝33個、実XY位置は31か所、原点のライト0個。全33個の実XY位置が登録配列の位置と一致した。部屋中心が一致する既存の重複2個は残る。個数はランダムなマップで変化する。証跡は同フォルダーの `pie_observation.before_room_fix.json`、`pie_observation.after_room_fix.json`、`Generate_Room_Lights.fixed.t3d`。全領域の明るさは保証しない。
