# Courtyard ISM 試作

## 再生するマップ

コンテンツブラウザーで次を開き、通常どおり再生する。

`/Game/AutoCreateMap/Courtyard/ISMPrototype/Maps/LVL_BackAlley_AutoCreateMap_ISM`

元の `LVL_BackAlley_AutoCreateMap` と `LVL_BackAlley` は変更していない。
プロジェクトの起動マップも変更していない。

## フォルダ

すべての試作アセットは `/Game/AutoCreateMap/Courtyard/ISMPrototype` 以下に配置した。

| フォルダ | 内容 |
|---|---|
| Maps | 元マップのコピー。生成アクターだけ試作BPへ差し替え |
| Blueprints | 元の生成BPをコピーした `BP_Courtyard_Generator_ISM` |
| Data | コピーしたテーマと、建物モジュールの配置情報カタログ |
| Assets/Meshes | ISMで使用するモデルのコピー |
| Assets/Materials | コピーしたマテリアルと親マテリアル |
| Assets/Blueprints | ランダム装飾BP・ランプBPのコピー |

追加したC++は `Source/Escape_on_Step_Demo/Generation/Courtyard/ISM` (runtime), `Source/Escape_on_Step_Demo/Editor/Courtyard/ISM` (editor tools) にまとめた。
元のC++・既存BP・モデル・マテリアル・設定ファイルは編集していない。
テクスチャ等は既存アセットを読み取り専用で参照する。動作付きBPの内部参照にも既存アセットが残るが、それらは変更していない。

## 変更した仕組み

迷路の形を決めるBPの処理はコピー元と同じ。
描画の呼び出しを `RenderCourtyardPlan` から `RenderCourtyardISMPlan` に切り替えた。

従来は、外壁やトンネルの部品をレベルとして何百個も読み込んでいた。
試作では、あらかじめ各部品のメッシュ・位置・回転・大きさ・マテリアル・当たり判定をカタログに記録し、同じ描画設定のメッシュをISMへまとめて配置する。
実行時に建物レベルを複製して読み込む処理を省いた。

床・天井・小部屋の壁もISMにまとめる。
通路の接続部分でアーチを隠す条件、床の高さ、小部屋の開口寸法は元の描画処理を引き継いでいる。

ランダムに装飾を選ぶBPやランプBPは、動作を保つためアクターとして配置する。
汚れなどのデカールも残す。
配置終了後は元と同様に、プレイヤーのカプセルで全マスの接続、床の存在、ゴール位置を検査する。

## 配置の分割

試作BP・試作マップの設定値は次のとおり。

- Frame Budget Milliseconds: 4
- Max Instances Per Frame: 512
- Max Actors Per Frame: 16
- Max Decals Per Frame: 96

1フレームにすべてを生成せず、時間と個数の上限で分割する。
4msは処理を続けるか判断する目安であり、1回の登録処理やGPU側処理まで含めた厳密な上限ではない。
形状データをキューにまとめる準備と、最後の到達検査は同期処理。
エディター起動、マップ自体の読み込み、初回シェーダーコンパイルは生成時間とは別に発生する。

## 編集・更新時の注意

この試作カタログは抽出時点の配置情報。元の建物レベルを後から編集しても自動で反映されない。
試作で見た目を調整する場合は、試作フォルダ内のメッシュ・マテリアル・カタログを編集する。
同じメッシュでもマテリアルや当たり判定が違うものは別のISMにまとめる。

再生成はコピーBPの `GenerateCourtyardBP` から行う。
親に残っている旧 `GenerateCourtyard` は従来のレベル読み込み方式なので、この試作では呼ばない。
試作側の消去関数は `ClearCourtyardISM`。

## 検証記録

ビルド結果、同一シードでの形状一致、到達検査、生成時間の比較は別紙 `Verification.md` を参照。
