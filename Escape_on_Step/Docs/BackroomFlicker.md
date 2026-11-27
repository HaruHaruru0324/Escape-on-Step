# BackRoom：発光メッシュと照明の同期点滅

2026-11-18更新。従来のLight Function方式を、同じ数値でメッシュと実ライトを更新する方式へ変更した。

## 生成と設定

`BP_Backroom_Renderer.Generate_Corridor_Lights` は、照明と発光メッシュを生成して `ConfigureBackroomFlicker(Light, Fixture, Chance)` を呼ぶ。呼び出し側のChanceの配線、部屋限定の判定、配置位置、ライトの色には変更を加えていない。

`ConfigureBackroomFlicker` は1灯につき1回だけ確率抽選する。各分岐で既存の通常／点滅メッシュマテリアルを設定後、C++の `Set Synchronized Pair` を呼ぶ。Light Functionはこの関数で解除する。

## 実行時

`UBackroomSynchronizedFlicker` は発光メッシュの所有Actorにつき1個のコンポーネントで、登録された点滅ペアをまとめて更新する。

- 点滅：同じBrightnessを、Dynamic Materialの `SyncBrightness` と `元のIntensity × Brightness` に渡す。
- 通常：Brightnessを1にし、元のIntensityへ戻す。
- 再設定：以前の元Intensityを保持し、暗い瞬間のIntensityを基準値にしない。
- ライトがない場合：発光メッシュを通常点灯にする。メッシュ単独の点滅は開始しない。
- ペア破棄：残った側を通常へ戻し、無効なペアを登録から除く。

マテリアルは専用の `M_BackroomFixtureSynced` を変更。`SyncEnabled=1` のとき、元の一定発光へ `SyncBrightness` を掛ける。通常のマテリアル使用時（SyncEnabled=0）は以前の計算を維持する。元のBodycamマテリアルは変更していない。

同期時の明るさ：

```text
Wave = sin(WorldTimeSeconds × Speed × 2π / 3)
Brightness = Saturate(Wave - floor(Wave) + Bias)^3
```

Speed/Biasは設定時にメッシュのMIから `Time` / `Sine` を読み取る。同期制御中はMF_BackroomFlickerやLight Function側の式を変更しても反映されない。強弱の指数は現在C++内の3。

## 検証

- Editorビルド成功、BPコンパイル0エラー・0警告。
- 点滅0％・100％・20％抽選、点滅と通常の繰り返し再設定を検証。発光倍率とIntensityの比率が一致。
- D3D12の再生描画テスト：通常時の床輝度145.5～145.625、点滅時1.164～145.625。同時に毎サンプルのメッシュ倍率とIntensityの一致を検証。
- 記録：作業用フォルダ `Escape_on_Step_Demo/Saved/BackroomFlickerSync`。

別のライト、環境光、間接光が重なる場所では、対象ライトが暗くなっても空間全体が暗くなるとは限らない。
