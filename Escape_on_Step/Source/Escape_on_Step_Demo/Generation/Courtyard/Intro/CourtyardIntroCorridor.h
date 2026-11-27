#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CourtyardIntroCorridor.generated.h"
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UStaticMesh;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UMaterialInterface;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UStaticMeshComponent;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class UBoxComponent;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
class ACourtyardISMGenerator;
// 後で定義される型の名前を先に宣言し、ポインターなどの参照に使えるようにする。
struct FCourtyardPlan;
// 列挙型をUEへ登録し、BPでも種類の選択に使えるようにする。
UENUM(BlueprintType)
// 床の種類や導入方法などを、名前付きの選択肢で表す列挙型を宣言する。
enum class ECourtyardIntroMode : uint8
{
    // 導入通路を別レベルとして使い、出口で本編レベルへ切り替える選択肢を定義する。
    SceneTravel UMETA(DisplayName = "Change Map"),
    // 導入通路と迷路を同じレベル内でつなぎ、そのまま歩いて入る選択肢を定義する。
    WalkIntoMaze UMETA(DisplayName = "Walk Into Maze (Same Map)")
};

/** BPからメッシュ・寸法を変更できる導入通路。同一マップ接続とマップ遷移に対応する。 */
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS(Blueprintable)

class ESCAPE_ON_STEP_DEMO_API ACourtyardIntroCorridor : public AActor
{
    // UEが型登録・BP連携・保存のために生成する宣言を挿入する。
    GENERATED_BODY()
public:
    // 導入通路Actorの生成時に、床・壁の参照や標準部品を初期化するコンストラクターを宣言する。
    ACourtyardIntroCorridor();
    // この関数の役割：エディターでActorを配置・変更したときの組み立て処理。ここでは呼び出し方を宣言する。
    virtual void OnConstruction(const FTransform& Transform) override;
    // この関数の役割：ゲーム開始時の初期化処理。ここでは呼び出し方を宣言する。
    virtual void BeginPlay() override;
    // この関数の役割：部品の初期化完了後に開始位置などを整える。ここでは呼び出し方を宣言する。
    virtual void PostInitializeComponents() override;
    // この関数の役割：フレームごとの開始待ち・演出の進行処理。ここでは呼び出し方を宣言する。
    virtual void Tick(float DeltaSeconds) override;
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能、BPで読み書き可能）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Transition")
    // レベル切替か同一マップ接続かの選択（TransitionMode）をECourtyardIntroModeとして用意し、ECourtyardIntroMode::SceneTravelで初期化する。
    ECourtyardIntroMode TransitionMode = ECourtyardIntroMode::SceneTravel;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Transition")
    // 導入後に開くレベル名（NextLevelName）をFNameとして用意し、"/Game/AutoCreateMap/Courtyard/ISMPrototype/Maps/LVL_BackAlley"というUE文字列で初期化する。
    FName NextLevelName = TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/Maps/LVL_BackAlley");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Transition")
    // 同一マップで生成する迷路Actorのクラス（MazeGeneratorClass）をTSubclassOf<ACourtyardISMGenerator>として宣言する。
    TSubclassOf<ACourtyardISMGenerator> MazeGeneratorClass;
    // この後の値をUEのプロパティとして登録する（配置済みActorごとに編集可能、BPで読み書き可能）。
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Intro|Transition")
    // 配置済みの迷路生成Actor（ExistingMazeGenerator）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<ACourtyardISMGenerator> ExistingMazeGenerator;
    // この後の値をUEのプロパティとして登録する（BPからは読み取り専用）。
    UPROPERTY(BlueprintReadOnly, Category = "Intro|Result")
    // 接続先の迷路生成Actor（MazeGenerator）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<ACourtyardISMGenerator> MazeGenerator;
    UPROPERTY(BlueprintReadOnly, Category = "Intro|Result")
    // 迷路へ入れる状態になったか（bMazeReady）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bMazeReady = false;
    UPROPERTY(BlueprintReadOnly, Category = "Intro|Result")
    // 導入通路の接続に失敗した理由（ConnectionError）を文字列として宣言する。
    FString ConnectionError;
    // この関数の役割：この壁境界が導入通路の開口か判定する。ここでは呼び出し方を宣言する。
    bool IsEntranceBoundary(FIntPoint Cell, int32 Direction) const;
    // この関数の役割：従来の導入通路へつながる入口を探し、開始とゴールを更新する。ここでは呼び出し方を宣言する。
    bool PrepareInlinePlan(ACourtyardISMGenerator* Generator, FCourtyardPlan& Plan);
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 床に使うメッシュ（FloorMesh）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UStaticMesh> FloorMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 壁に使うメッシュ（WallMesh）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UStaticMesh> WallMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 天井に使うメッシュ（CeilingMesh）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UStaticMesh> CeilingMesh;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 床の材質（FloorMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> FloorMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 壁の材質（WallMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> WallMaterial;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Meshes")
    // 天井の材質（CeilingMaterial）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UMaterialInterface> CeilingMaterial;
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能）。
    UPROPERTY(EditAnywhere,
        // UEの公開設定を続け、BPから読み書きできる範囲や編集可能な場所を指定する。
        BlueprintReadWrite,
        // 詳細パネルやBPの一覧で、この値を表示する分類名を指定する。
        Category = "Intro|Size",
        // 詳細パネルでの入力範囲など、編集補助のメタデータを指定する。実行時の計算式ではない。
        meta = (ClampMin = "1", ClampMax = "32"))
    // 導入通路を構成する直線区画の個数（SegmentCount）を符号付き32ビット整数として用意し、8で初期化する。
    int32 SegmentCount = 8;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size", meta = (ClampMin = "100"))
    // 導入通路の1区画の長さ（cm）（SegmentLength）を単精度の実数として用意し、320で初期化する。
    float SegmentLength = 320;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size", meta = (ClampMin = "160"))
    // 導入通路の内幅（cm）（InteriorWidth）を単精度の実数として用意し、480で初期化する。
    float InteriorWidth = 480;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size", meta = (ClampMin = "200"))
    // 導入通路の内側の高さ（cm）（InteriorHeight）を単精度の実数として用意し、320で初期化する。
    float InteriorHeight = 320;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size", meta = (ClampMin = "5"))
    // 導入通路の壁厚（cm）（WallThickness）を単精度の実数として用意し、20で初期化する。
    float WallThickness = 20;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size")
    // 天井を作るかの設定（bCeiling）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bCeiling = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size")
    // 導入通路の手前を壁で閉じる設定（bCloseEntrance）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bCloseEntrance = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Intro|Size")
    // 導入通路の奥を壁で閉じる設定（bCloseExit）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bCloseExit = false;
    // この後の関数をUEへ登録する。実行順を指定できるBPノードとして公開。
    UFUNCTION(BlueprintCallable, Category = "Intro")
    // この関数の役割：設定されたメッシュで導入通路の床・壁・天井を作り直す。ここでは呼び出し方を宣言する。
    void RebuildCorridor();
    // この後の関数をUEへ登録する。実行ピンなしで値を返すBPノードとして公開。
    UFUNCTION(BlueprintPure, Category = "Intro")
    // この関数の役割：導入通路の開始位置をワールド座標で返す。ここでは呼び出し方を宣言する。
    FVector GetEntrancePosition() const;
    UFUNCTION(BlueprintPure, Category = "Intro")
    // この関数の役割：導入通路の出口位置をワールド座標で返す。ここでは呼び出し方を宣言する。
    FVector GetExitPosition() const;

private:
    // この後の値をUEのプロパティとして登録する（実行時専用でアセットへ保存しない）。
    UPROPERTY(Transient)
    // Pieces（Pieces）を配列として宣言する。
    TArray<TObjectPtr<UStaticMeshComponent>> Pieces;
    UPROPERTY(Transient)
    // CollisionPieces（CollisionPieces）を配列として宣言する。
    TArray<TObjectPtr<UBoxComponent>> CollisionPieces;
    // この後の値をUEのプロパティとして登録する。
    UPROPERTY(VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Intro",
        meta = (AllowPrivateAccess = "true"))
    // 導入通路の出口を検知するBox（ExitTrigger）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UBoxComponent> ExitTrigger;
    UPROPERTY(VisibleAnywhere,
        BlueprintReadOnly,
        Category = "Intro",
        meta = (AllowPrivateAccess = "true"))
    // 迷路の準備中に出口をふさぐ仮の壁（LoadingGate）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UBoxComponent> LoadingGate;
    // この後の関数をUEへ登録する。UEの関数情報へ登録。
    UFUNCTION()
    // この関数の役割：出口へ触れたActorを調べ、必要ならステージへ移動する。ここでは呼び出し方を宣言する。
    void HandleOverlap(AActor* OverlappedActor, AActor* OtherActor);
    // 壁を開ける入口セルの集合（EntranceCells）を重複しない集合として宣言する。
    TSet<FIntPoint> EntranceCells;
    // 移動演出やレベル切替を開始したかのフラグ（bTransitionStarted）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bTransitionStarted = false;
    // 移動オプションで導入を飛ばすかの結果（bSkipIntro）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bSkipIntro = false;
    // この関数の役割：同じマップ内の迷路生成を開始して導入通路へ接続する。ここでは呼び出し方を宣言する。
    void StartInlineMaze();
    // この関数の役割：導入通路の構成メッシュを寸法に合わせて作る。ここでは呼び出し方を宣言する。
    void AddPiece(UStaticMesh* Mesh,
        // 使用するマテリアル（Material）をUMaterialInterfaceとして引数で受け取る。参照先がない場合もあるポインターとして扱う。
        UMaterialInterface* Material,
        // 中央位置（Center）を3次元ベクトルとして引数で受け取る。元の値を参照し、コピーしない。
        const FVector& Center,
        // 範囲の大きさ（Size）を3次元ベクトルとして引数で受け取る。元の値を参照し、コピーしない。
        const FVector& Size);
};
