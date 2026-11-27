# C++ソースの配置

モジュールは従来どおり `Escape_on_Step_Demo` の1つ。クラス名・関数名・BPが参照する型のパスは維持しています。
`.h` と対応する `.cpp` は同じフォルダに配置します。

```text
Escape_on_Step_Demo/
├ Escape_on_Step_Demo.Build.cs   ビルド設定
├ Escape_on_Step_Demo.h         モジュール共通宣言
├ Escape_on_Step_Demo.cpp       モジュールの起動処理
├ Escape_on_Step_DemoCharacter.* / PlayerController.* / CameraManager.* / GameMode.*
│                              既存の共通クラス（元の位置を維持）
├ Generation/
│ ├ Backroom/
│ │ ├ Layout/                  部屋の間取り・隣接生成
│ │ ├ Walls/                   壁の配置・接続・トポロジー
│ │ ├ Lighting/                照明配置・領域判定・明滅同期
│ │ ├ Goals/                   ゴール候補・領域・配置
│ │ └ Runtime/                 生成の再試行など実行時の管理
│ └ Courtyard/
│   ├ Layout/                  路地の間取り生成
│   ├ Rendering/               既存のレベル配置方式
│   └ ISM/                     ISM配置方式の試作・データ定義
├ Editor/
│ ├ Backroom/                  バックルームBPの編集補助・診断
│ └ Courtyard/
│   └ ISM/                     ISM用データ抽出・試作BPの編集補助
├ Tests/
│ ├ Backroom/                  バックルームの自動テスト
│ └ Courtyard/                 路地生成の自動テスト
├ Variant_Horror/              Horror用キャラクター・ゲームモード・UI
└ Variant_Shooter/             Shooter用キャラクター・AI・武器・UI
```

整理対象は、この作業で私たちが作成した生成処理・編集補助・テストです。
既存の共通クラスと `Variant_Horror`・`Variant_Shooter` は、ファイルの位置・内容とも整理前に戻しました。

## 参照の書き方

移動したプロジェクト内ヘッダーは、モジュールのルートからのパスで参照します。

```cpp
#include "Generation/Backroom/Layout/DungeonAdjacentLayout.h"
#include "Generation/Courtyard/Rendering/CourtyardGenerator.h"
```

UEの `.generated.h` は従来どおりファイル名のみで記述し、ヘッダー内の最後のincludeに置きます。
フォルダを追加するたびにBuild.csへ検索先を追加する必要はありません。

`Editor` は用途を示すフォルダです。別モジュールへ分離したものではありません。
既存の `WITH_EDITOR` 条件とビルド設定をそのまま使っています。

## 今回の整理範囲

私たちが作成した44ファイルを移動し、27ファイルのincludeパスを更新しました。
対象外の47ファイルは整理前のバックアップと完全一致することを検査済みです。
処理本体の変更、クラスや関数の改名、BP・モデル・マテリアル・マップの編集は行っていません。
整理前の全ソースは別途バックアップを保存し、各ファイルが移動後も「includeパスの変更のみ」で一致することを検査しました。
