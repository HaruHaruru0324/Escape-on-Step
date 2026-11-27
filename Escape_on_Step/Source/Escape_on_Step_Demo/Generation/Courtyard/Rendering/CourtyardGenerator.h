#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "Generation/Courtyard/Layout/CourtyardLayout.h"
#include "CourtyardGenerator.generated.h"

// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class ULevelStreamingDynamic;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UStaticMeshComponent;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class ACharacter;

/** 配置するレベルと地面の参照をデータ化。既存レベル/マテリアル自体は変更しない。 */
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS(BlueprintType)

class ESCAPE_ON_STEP_DEMO_API UCourtyardTheme : public UDataAsset
{
    // UEが型登録・BP連携・保存のために生成する宣言を挿入する。
    GENERATED_BODY()
public:
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能、BPで読み書き可能）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // Facades（Facades）を配列として宣言する。
    TArray<TSoftObjectPtr<UWorld>> Facades;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // Portals（Portals）を配列として宣言する。
    TArray<TSoftObjectPtr<UWorld>> Portals;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // 直線トンネルに使う部品レベル（Passage）をUEオブジェクトへの参照として宣言する。
    TSoftObjectPtr<UWorld> Passage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // 屋内の外周に使う壁部品（RoomFacade）をUEオブジェクトへの参照として宣言する。
    TSoftObjectPtr<UWorld> RoomFacade;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
    // 床に使うメッシュ（FloorMesh）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UStaticMesh> FloorMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
    // 屋外床の材質（StreetMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> StreetMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
    // 屋内床の材質（RoomMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> RoomMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces")
    // 天井の材質（CeilingMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> CeilingMaterial;
};

// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardModulePlacement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // 配置する部品レベル（Level）をUEオブジェクトへの参照として宣言する。
    TSoftObjectPtr<UWorld> Level;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // 位置・回転・拡大率（Transform）を位置・回転・拡大率として宣言する。
    FTransform Transform;
    /** 部品内の追加補正。通常はIdentity。導入接続では元の壁の奥行きだけを薄くする。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    FTransform LocalAdjustment = FTransform::Identity;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // この配置が直線トンネルかの識別（bPassage）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bPassage = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // トンネル手前のアーチを隠す設定（bHideStartArch）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bHideStartArch = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules")
    // トンネル奥のアーチを隠す設定（bHideEndArch）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bHideEndArch = false;
};

/** BPはこのアクターを継承する。データ生成と描画の入口を分け、後からBP生成へ差し替えられる。 */
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS(Blueprintable)

class ESCAPE_ON_STEP_DEMO_API ACourtyardGenerator : public AActor
{
    GENERATED_BODY()
public:
    // レベル部品方式の迷路Actorを作成するときに、ルート部品や既定設定を用意するコンストラクターを宣言する。
    ACourtyardGenerator();
    // この関数の役割：ゲーム開始時の初期化処理。ここでは呼び出し方を宣言する。
    virtual void BeginPlay() override;
    // この関数の役割：終了時のタイマー解除や移動制限解除。ここでは呼び出し方を宣言する。
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Layout")
    // 再現用Seed（Seed）を符号付き32ビット整数として用意し、-1で初期化する。
    int32 Seed = -1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Layout")
    // 生成設定（Settings）を迷路の生成設定として宣言する。
    FCourtyardSettings Settings;
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能）。
    UPROPERTY(EditAnywhere,
        // UEの公開設定を続け、BPから読み書きできる範囲や編集可能な場所を指定する。
        BlueprintReadWrite,
        // 詳細パネルやBPの一覧で、この値を表示する分類名を指定する。
        Category = "Courtyard|Layout",
        // 詳細パネルでの入力範囲など、編集補助のメタデータを指定する。実行時の計算式ではない。
        meta = (ClampMin = "200", ClampMax = "800"))
    // 1セルの実寸（cm）（CellSize）を単精度の実数として用意し、320で初期化する。
    float CellSize = 320;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Modules")
    // 壁・床・通路の見た目の参照データ（Theme）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UCourtyardTheme> Theme;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Gameplay")
    // C++のBeginPlayで自動生成する設定（bGenerateOnBeginPlay）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bGenerateOnBeginPlay = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Gameplay")
    // 生成するゴールActorのクラス（GoalClass）をTSubclassOf<AActor>として宣言する。
    TSubclassOf<AActor> GoalClass;
    // この後の値をUEのプロパティとして登録する（BPからは読み取り専用）。
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 生成結果（Plan）を迷路全体の生成結果として宣言する。
    FCourtyardPlan Plan;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 配置する部品と姿勢の一覧（ModulePlacements）を配列として宣言する。
    TArray<FCourtyardModulePlacement> ModulePlacements;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 生成完了フラグ（bGenerationComplete）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bGenerationComplete = false;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 生成失敗フラグ（bGenerationFailed）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bGenerationFailed = false;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 生成失敗の理由（GenerationError）を文字列として宣言する。
    FString GenerationError;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 今回実際に使ったSeed（ActualSeed）を符号付き32ビット整数として用意し、0で初期化する。
    int32 ActualSeed = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result")
    // 実際の衝突形状で到達できた床の件数（PhysicallyReachableCells）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PhysicallyReachableCells = 0;

    // この後の関数をUEへ登録する。実行順を指定できるBPノードとして公開。
    UFUNCTION(BlueprintCallable, Category = "Courtyard")
    // この関数の役割：配置データを作り、従来のレベル部品方式で描画を開始する。ここでは呼び出し方を宣言する。
    bool GenerateCourtyard();
    // BPで作ったPlanをここへ渡せる。C++のBuildCourtyardPlanを使うことは必須ではない。
    UFUNCTION(BlueprintCallable, Category = "Courtyard")
    // この関数の役割：渡された配置データを従来のレベル部品方式で描画する。ここでは呼び出し方を宣言する。
    bool RenderCourtyardPlan(const FCourtyardPlan& NewPlan);
    UFUNCTION(BlueprintCallable, Category = "Courtyard")
    // この関数の役割：生成したレベル部品・床・Actorを片付ける。ここでは呼び出し方を宣言する。
    void ClearCourtyard();
    // この後の関数をUEへ登録する。実行ピンなしで値を返すBPノードとして公開。
    UFUNCTION(BlueprintPure, Category = "Courtyard")
    // この関数の役割：セル座標と高さをワールド座標へ変換する。ここでは呼び出し方を宣言する。
    FVector GetCellWorldPosition(FIntPoint Cell, float Height = 0) const;
    // この後の関数をUEへ登録する。BPで実装する生成完了通知として公開。
    UFUNCTION(BlueprintImplementableEvent, Category = "Courtyard")
    // この関数の役割：迷路の生成完了をBPへ通知する。ここでは呼び出し方を宣言する。
    void OnCourtyardReady();

private:
    // この後の値をUEのプロパティとして登録する（実行時専用でアセットへ保存しない）。
    UPROPERTY(Transient)
    // 読み込み済みのレベル部品（LoadedModules）を配列として宣言する。
    TArray<TObjectPtr<ULevelStreamingDynamic>> LoadedModules;
    UPROPERTY(Transient)
    // 生成したメッシュ部品一覧（GeneratedMeshes）を配列として宣言する。
    TArray<TObjectPtr<UStaticMeshComponent>> GeneratedMeshes;
    UPROPERTY(Transient)
    // 生成したActorの一覧（GeneratedActors）を配列として宣言する。
    TArray<TObjectPtr<AActor>> GeneratedActors;
    // PreparedModules（PreparedModules）を重複しない集合として宣言する。
    TSet<int32> PreparedModules;
    // 部品読み込みの確認予約（LoadingTimer）をFTimerHandleとして宣言する。
    FTimerHandle LoadingTimer;
    // 部品の読み込みを開始した実時間（LoadingStarted）を倍精度の実数として用意し、0で初期化する。
    double LoadingStarted = 0;
    // 作り直しの回数を区別する番号（GenerationSerial）を符号付き32ビット整数として用意し、0で初期化する。
    int32 GenerationSerial = 0;
    // レベル部品の読み込みを進めているかのフラグ（bRendering）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bRendering = false;
    // FrozenPlayer（FrozenPlayer）をUEオブジェクトへの参照として宣言する。
    TWeakObjectPtr<ACharacter> FrozenPlayer;
    // PreviousMovementMode（PreviousMovementMode）を0〜255の符号なし整数として用意し、0で初期化する。
    uint8 PreviousMovementMode = 0;
    // この関数の役割：各床セルの境界へ壁・出入口・通路などの部品配置を計画する。ここでは呼び出し方を宣言する。
    void PlanModules();
    // この関数の役割：レベル部品の読み込み完了を確認し、準備できた部品を表示する。ここでは呼び出し方を宣言する。
    void PollModules();
    // この関数の役割：実際の床支持と衝突で全床へ到達できるか検査する。ここでは呼び出し方を宣言する。
    bool ValidatePhysicalLayout();
    // この関数の役割：レベル部品の読み込み中にプレイヤーを停止または復帰する。ここでは呼び出し方を宣言する。
    void SetPlayerFrozen(bool Frozen);
    // この関数の役割：エラーを記録して失敗を返す。ここでは呼び出し方を宣言する。
    bool Fail(const FString& Reason);
    // この関数の役割：指定セルに床または天井の描画データを追加する。ここでは呼び出し方を宣言する。
    void AddSurface(
        // Cell（Cell）、float Height（float Height）、UMaterialInterface* Material（UMaterialInterface* Material）、bool Ceiling = false)（bool Ceiling = false)）を、それぞれ2次元の整数セル座標として用意する。
        FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling = false);
};
