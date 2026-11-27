#pragma once
#include "CoreMinimal.h"
#include "Generation/Courtyard/Rendering/CourtyardGenerator.h"
#include "Generation/Courtyard/ISM/CourtyardISMModuleCatalog.h"
#include "CourtyardISMGenerator.generated.h"
class UInstancedStaticMeshComponent;
class UDecalComponent;
class ACourtyardIntroCorridor;
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT()

struct FCourtyardISMGroup
{
    // UEが型登録・BP連携・保存のために生成する宣言を挿入する。
    GENERATED_BODY()
    // この後の値をUEのプロパティとして登録する。
    UPROPERTY()
    // 生成設定（Settings）をISMのメッシュ・材質・衝突・配置設定として宣言する。
    FCourtyardISMMesh Settings;
    UPROPERTY()
    // インスタンスごとの配置姿勢一覧（Transforms）を配列として宣言する。
    TArray<FTransform> Transforms;
    UPROPERTY()
    // 対象のコンポーネント（Component）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UInstancedStaticMeshComponent> Component;
    // このグループへ追加済みのインスタンス数（Added）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Added = 0;
};

/** 試作BP用のISM描画Actor。配置データの生成・描画・通行検査の入口をBPへ公開する。 */
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS(Blueprintable)

class ESCAPE_ON_STEP_DEMO_API ACourtyardISMGenerator : public ACourtyardGenerator
{
    GENERATED_BODY()
public:
    // ISM方式の生成Actorを作るときの、既定値や初期部品の準備を行うコンストラクターを宣言する。
    ACourtyardISMGenerator();
    // BPのBranchで新方式と従来方式を切り替える。生成アルゴリズムの選択に使う。
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能、BPで読み書き可能）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM|Layout")
    // 元BPで部屋の塊と細道の方式を選ぶ設定（bUseOrganicLayout）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bUseOrganicLayout = true;
    // この後の関数をUEへ登録する。実行順を指定できるBPノードとして公開。
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Layout")
    // この関数の役割：C++版の部屋・細道データを作ってISM描画を開始する。ここでは呼び出し方を宣言する。
    bool GenerateOrganicCourtyard();

    /** BPの分割生成を開始する。完成までの落下を防ぎ、前回の表示を片付ける。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Blueprint Layout")
    // この関数の役割：BP生成開始時に前回の表示を消し、プレイヤーを一時停止する。ここでは呼び出し方を宣言する。
    bool BeginBlueprintLayout();
    /** BP生成失敗時も暗転・移動制限を必ず解除する。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Blueprint Layout")
    // この関数の役割：BPの生成失敗を記録して暗転と移動制限を解除する。ここでは呼び出し方を宣言する。
    bool FailBlueprintLayout(const FString& Reason);
    /** 乱数順を変えず、既存C++版と同じセルごとの小さな経路コストを返す。 */
    // この後の関数をUEへ登録する。実行ピンなしで値を返すBPノードとして公開。
    UFUNCTION(BlueprintPure, Category = "Courtyard ISM|Blueprint Layout")
    // この関数の役割：座標とSeedから経路コストの小さな変化を求める。ここでは呼び出し方を宣言する。
    static int32 RouteCellNoise(FIntPoint Cell, int32 LayoutSeed);

    /** BPが入口候補を選ぶ際に必要な物理寸法。X=内幅、Y=奥行、Z=外幅。0=導入なし。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Blueprint Layout")
    // この関数の役割：導入通路の内幅・奥行・外幅を求める。ここでは呼び出し方を宣言する。
    FVector GetBlueprintEntranceDimensions();

    /** 重い配置探索のみC++へ渡す。部屋の形・個数・実行順は呼出し元BPが決める。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Blueprint Search")
    // この関数の役割：BPの部屋配列を共有C++探索へ渡して位置を詰める。ここでは呼び出し方を宣言する。
    static void CompactBlueprintRooms(
        // BPへ公開する引数を参照渡しとして扱う。呼び出し元の配列などを直接更新できる。
        UPARAM(ref) TArray<FCourtyardNode>& Rooms, int32 Limit, int32 LayoutSeed);
    /** 2室間の候補経路だけ返す。採用・接続数・周回条件はBPが判断する。 */
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Blueprint Search")
    // この関数の役割：BPが指定した2室間の経路候補を共有探索で求める。ここでは呼び出し方を宣言する。
    static bool FindBlueprintRoomRoute(int32 FromRoom,
        // ToRoom（ToRoom）を符号付き32ビット整数として引数で受け取る。
        int32 ToRoom,
        // 部屋の一覧（Rooms）を配列として引数で受け取る。元の値を参照し、コピーしない。
        const TArray<FCourtyardNode>& Rooms,
        // 床セルから所属部屋を引く表（Owners）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& Owners,
        // RoadCells（RoadCells）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, bool>& RoadCells,
        // 使用済み入口と部屋IDの対応表（UsedEntrances）をキーで値を引く表として引数で受け取る。元の値を参照し、コピーしない。
        const TMap<FIntPoint, int32>& UsedEntrances,
        // 配置盤面の一辺のセル数（Limit）を符号付き32ビット整数として引数で受け取る。
        int32 Limit,
        // 配置用Seed（LayoutSeed）を符号付き32ビット整数として引数で受け取る。
        int32 LayoutSeed,
        // Route（Route）を2室間の接続情報として引数で受け取る。元の値を参照し、コピーしない。
        FCourtyardLink& Route);

    /** BPで抽選済みの入口。選定アルゴリズムはBP、実メッシュとの接続はISM側で行う。 */
    // この後の値をUEのプロパティとして登録する（BPで読み書き可能）。
    UPROPERTY(BlueprintReadWrite, Category = "Courtyard ISM|Blueprint Layout")
    // BPで抽選した入口データ（BlueprintEntrance）を2室間の接続情報として宣言する。
    FCourtyardLink BlueprintEntrance;
    UPROPERTY(BlueprintReadWrite, Category = "Courtyard ISM|Blueprint Layout")
    // BPによる入口抽選が完了したかのフラグ（bBlueprintEntranceReady）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bBlueprintEntranceReady = false;

    // 導入通路の参照。指定されている場合は、その出口へ迷路を接続する。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM|Intro")
    // 従来の導入通路Actor（IntroCorridor）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<ACourtyardIntroCorridor> IntroCorridor;
    /** 自作の導入エリアActor。MazeConnectionPointを出口として直接使う。旧通路Actorは不要。 */
    // この後の値をUEのプロパティとして登録する（配置済みActorごとに編集可能、BPで読み書き可能）。
    UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "Courtyard ISM|Entrance")
    // 自作の導入エリアActor（EntranceArea）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<AActor> EntranceArea;
    /** ONなら出口の少し内側で左右の壁を測定する。測れない場合はEntranceWidthを使う。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM|Entrance")
    // 実際の内壁幅を測る設定（bMeasureEntranceWidth）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bMeasureEntranceWidth = true;
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能）。
    UPROPERTY(EditAnywhere,
        // UEの公開設定を続け、BPから読み書きできる範囲や編集可能な場所を指定する。
        BlueprintReadWrite,
        // 詳細パネルやBPの一覧で、この値を表示する分類名を指定する。
        Category = "Courtyard ISM|Entrance",
        // 詳細パネルでの入力範囲など、編集補助のメタデータを指定する。実行時の計算式ではない。
        meta = (ClampMin = "140"))
    // 測れなかった場合に使う入口幅（EntranceWidth）を単精度の実数として用意し、480で初期化する。
    float EntranceWidth = 480;
    /** 入口の四方向を実際の開始位置へ反映する。OFFなら導入エリアの位置を固定する。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM|Entrance")
    // 導入エリアを抽選入口へ移動する設定（bMoveEntranceAreaToMaze）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bMoveEntranceAreaToMaze = true;
    // この後の値をUEのプロパティとして登録する（BPからは読み取り専用）。
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 測定・採用した入口の実幅（ActualEntranceWidth）を単精度の実数として用意し、0で初期化する。
    float ActualEntranceWidth = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 内壁から幅を測定できたかの結果（bEntranceWidthMeasured）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bEntranceWidthMeasured = false;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 入口の外向き方向番号（EntranceOutwardDirection）を符号付き32ビット整数として用意し、2で初期化する。
    int32 EntranceOutwardDirection = 2;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 内側の入口を選んだかの結果（bEntranceIsInterior）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bEntranceIsInterior = false;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 導入通路出口の位置と向き（EntranceTransform）を位置・回転・拡大率として宣言する。
    FTransform EntranceTransform;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Entrance|Result")
    // 導入エリア内でのプレイヤー配置姿勢（EntranceSpawnTransform）を位置・回転・拡大率として宣言する。
    FTransform EntranceSpawnTransform;
    // この関数の役割：終了時のタイマー解除や移動制限解除。ここでは呼び出し方を宣言する。
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM")
    // レベルから抽出した配置部品のカタログ（ModuleCatalog）をUEオブジェクトへの参照として宣言する。
    TObjectPtr<UCourtyardISMModuleCatalog> ModuleCatalog;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Courtyard ISM|Budget",
        meta = (ClampMin = "0.5", ClampMax = "16"))
    // 1フレームのISM追加作業時間の目安（ms）（FrameBudgetMilliseconds）を単精度の実数として用意し、4で初期化する。
    float FrameBudgetMilliseconds = 4;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Courtyard ISM|Budget",
        meta = (ClampMin = "32", ClampMax = "2048"))
    // 1フレームに追加するメッシュの上限数（MaxInstancesPerFrame）を符号付き32ビット整数として用意し、512で初期化する。
    int32 MaxInstancesPerFrame = 512;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Courtyard ISM|Budget",
        meta = (ClampMin = "1", ClampMax = "64"))
    // 1フレームに生成する小物Actorの上限数（MaxActorsPerFrame）を符号付き32ビット整数として用意し、16で初期化する。
    int32 MaxActorsPerFrame = 16;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Courtyard ISM|Budget",
        meta = (ClampMin = "1", ClampMax = "256"))
    // 1フレームに生成するデカールの上限数（MaxDecalsPerFrame）を符号付き32ビット整数として用意し、96で初期化する。
    int32 MaxDecalsPerFrame = 96;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // ISM描画にかかった秒数（PrototypeRenderSeconds）を倍精度の実数として用意し、0で初期化する。
    double PrototypeRenderSeconds = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 描画開始前の準備時間（ms）（PrototypePreparationMilliseconds）を倍精度の実数として用意し、0で初期化する。
    double PrototypePreparationMilliseconds = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 描画作業1回の最大処理時間（ms）（PeakGenerationTickMilliseconds）を倍精度の実数として用意し、0で初期化する。
    double PeakGenerationTickMilliseconds = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 追加済みメッシュの総件数（PrototypeInstances）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PrototypeInstances = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 作成したISM部品の総件数（PrototypeISMComponents）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PrototypeISMComponents = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 生成した小物Actorの総件数（PrototypeActorCount）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PrototypeActorCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 生成したデカールの総件数（PrototypeDecalCount）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PrototypeDecalCount = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Result")
    // 分割描画を進めた呼び出し回数（PrototypeTickCount）を符号付き32ビット整数として用意し、0で初期化する。
    int32 PrototypeTickCount = 0;
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM")
    // この関数の役割：配置データを受け取りISMによる描画を開始する。ここでは呼び出し方を宣言する。
    bool RenderCourtyardISMPlan(const FCourtyardPlan& NewPlan);
    UFUNCTION(BlueprintCallable, Category = "Courtyard ISM")
    // この関数の役割：生成済みISM・小物・作業待ちを片付ける。ここでは呼び出し方を宣言する。
    void ClearCourtyardISM();

	/*
	* 部屋Idに対応する壁をBPで選ぶ
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Courtyard ISM|Walls")
	TSoftObjectPtr<UWorld> SelectFacadeForNode(
		int32							NodeId,
		const TSoftObjectPtr<UWorld>&	DefaultFacade);

	/*
	* 入口Idに対応する門をBPで選ぶ
	*/
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category = "Courtyard ISM|Portals")
	TSoftObjectPtr<UWorld> SelectPortalForNode(
		int32							NodeId,
		const TSoftObjectPtr<UWorld>&	DefaultFacade);


    /** ONなら直接つながる部屋・細道の高さを揃え、切り替えを短いトンネル内に限定する。 */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard ISM|Walls")
    bool bSeparateWallStylesWithTunnels = true;

    /** 各床セルが壁選択に使う代表部屋ID。細道も-1ではなく同じ空間の部屋IDを使う。 */
    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Walls|Result")
    TMap<FIntPoint, int32> WallStyleOwners;

    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Walls|Result")
    int32 WallStyleRegionCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "Courtyard ISM|Walls|Result")
    int32 WallStyleAddedTunnelCount = 0;

private:
    // この後の値をUEのプロパティとして登録する（実行時専用でアセットへ保存しない）。
    UPROPERTY(Transient)
    // 共通設定ごとのISM描画グループ一覧（Groups）を配列として宣言する。
    TArray<FCourtyardISMGroup> Groups;
    UPROPERTY(Transient)
    // Actorの生成待ち一覧（ActorQueue）を配列として宣言する。
    TArray<FCourtyardISMActor> ActorQueue;
    UPROPERTY(Transient)
    // デカールの生成待ち一覧（DecalQueue）を配列として宣言する。
    TArray<FCourtyardISMDecal> DecalQueue;
    UPROPERTY(Transient)
    // 生成した小物Actorの管理一覧（PrototypeActors）を配列として宣言する。
    TArray<TObjectPtr<AActor>> PrototypeActors;
    UPROPERTY(Transient)
    // 生成したデカールの管理一覧（PrototypeDecals）を配列として宣言する。
    TArray<TObjectPtr<UDecalComponent>> PrototypeDecals;
    // ISM描画の継続予約（BuildTimer）をFTimerHandleとして宣言する。
    FTimerHandle BuildTimer;
    // 次に描画するグループの添字（GroupIndex）に0を格納する。次に生成するActorの添字（ActorIndex）に0を格納する。次に生成するデカールの添字（DecalIndex）に0を格納する。
    int32 GroupIndex = 0, ActorIndex = 0, DecalIndex = 0;
    // ISM描画を開始した実時間（秒）（BuildStarted）を倍精度の実数として用意し、0で初期化する。
    double BuildStarted = 0;
    // ISMの分割描画を進めているかのフラグ（bPrototypeRendering）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bPrototypeRendering = false;
    // PrototypeFrozenPlayer（PrototypeFrozenPlayer）をUEオブジェクトへの参照として宣言する。
    TWeakObjectPtr<ACharacter> PrototypeFrozenPlayer;
    // PrototypeMovementMode（PrototypeMovementMode）を0〜255の符号なし整数として用意し、0で初期化する。
    uint8 PrototypeMovementMode = 0;
    // この関数の役割：同じ描画・衝突設定のメッシュをISMグループへまとめる。ここでは呼び出し方を宣言する。
    void QueueMesh(const FCourtyardISMMesh& Entry);
    // この関数の役割：指定セルに床または天井の描画データを追加する。ここでは呼び出し方を宣言する。
    void AddSurface(
        // Cell（Cell）、float Height（float Height）、UMaterialInterface* Material（UMaterialInterface* Material）、bool Ceiling = false)（bool Ceiling = false)）を、それぞれ2次元の整数セル座標として用意する。
        FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling = false);
    // 壁を開ける入口セルの集合（EntranceCells）を重複しない集合として宣言する。
    TSet<FIntPoint> EntranceCells;
    // 入口境界のローカル中央座標（EntranceBoundaryCenter）を3次元ベクトルとして用意し、FVector::ZeroVectorで初期化する。
    FVector EntranceBoundaryCenter = FVector::ZeroVector;
    // この関数の役割：自作導入エリアを抽選入口へ接続し、開始位置を整える。ここでは呼び出し方を宣言する。
    bool PrepareCustomEntrance();
    FTransform FitEntranceModule(TSoftObjectPtr<UWorld> Level, float WidthScale = 1.f) const;

    // この関数の役割：セル幅の開口の両端を実際の通路幅まで狭める。ここでは呼び出し方を宣言する。
    void QueueEntranceJambs();
    // この関数の役割：入口周辺の薄い壁をISMの描画待ちへ追加する。ここでは呼び出し方を宣言する。
    void QueueEntranceBox(const FVector& Center, const FVector& Size);
    // この関数の役割：壁部品が自作導入エリアへ重なるか調べる。ここでは呼び出し方を宣言する。
    bool BoundaryOverlapsEntrance(
        // Cell（Cell）、int32 Direction（int32 Direction）、TSoftObjectPtr<UWorld> Level) const（TSoftObjectPtr<UWorld> Level) const）を、それぞれ2次元の整数セル座標として用意する。
        FIntPoint Cell, int32 Direction, TSoftObjectPtr<UWorld> Level) const;
    // この関数の役割：導入エリアに食い込む外壁を薄い壁や扉枠に置き換える。ここでは呼び出し方を宣言する。
    void QueueThinEntranceBoundary(FIntPoint Cell, int32 Direction);
    // この関数の役割：各床セルの境界へ壁・出入口・通路などの部品配置を計画する。ここでは呼び出し方を宣言する。
    void PlanModules();
    // この関数の役割：時間と件数の予算内でISM・小物・デカールを追加する。ここでは呼び出し方を宣言する。
    void BuildNextBatch();
    // この関数の役割：ISM生成中のプレイヤー移動を停止または復帰する。ここでは呼び出し方を宣言する。
    void FreezePrototypePlayer(bool Frozen);
    // この関数の役割：実際の床支持と衝突で全床へ到達できるか検査する。ここでは呼び出し方を宣言する。
    bool ValidatePhysicalLayout();
    // この関数の役割：ISM生成を失敗状態にして移動制限と暗転を解除する。ここでは呼び出し方を宣言する。
    bool PrototypeFail(const FString& Reason);
};
