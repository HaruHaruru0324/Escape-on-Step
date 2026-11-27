#include "Generation/Courtyard/Intro/CourtyardIntroCorridor.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Engine/StaticMesh.h"
#include "UObject/ConstructorHelpers.h"
#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "Components/LightComponent.h"

// このActorを作成するときに、標準部品や初期設定を用意するコンストラクター。
ACourtyardIntroCorridor::ACourtyardIntroCorridor()
{
    // RootComponentを、Actorの標準部品を作成する（引数："Root"というUE文字列）に設定する。
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    // Cube（Cube）をConstructorHelpers::FObjectFinder<UStaticMesh>として作る。括弧内に並べる値を初期値として使う。
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(
        // "/Engine/BasicShapes/Cube.Cube"というUE文字列。
        TEXT("/Engine/BasicShapes/Cube.Cube"));
    // 床に使うメッシュ（FloorMesh）を、CubeのObjectに設定する。
    FloorMesh = Cube.Object;
    // 壁に使うメッシュ（WallMesh）を、CubeのObjectに設定する。
    WallMesh = Cube.Object;
    // 天井に使うメッシュ（CeilingMesh）を、CubeのObjectに設定する。
    CeilingMesh = Cube.Object;
    // 導入通路の出口を検知するBox（ExitTrigger）を、Actorの標準部品を作成する（引数："ExitTrigger"というUE文字列）に設定する。
    ExitTrigger = CreateDefaultSubobject<UBoxComponent>(TEXT("ExitTrigger"));
    // 導入通路の出口を検知するBox（ExitTrigger）で、親部品との取り付け関係を設定する（引数：RootComponent）。
    ExitTrigger->SetupAttachment(RootComponent);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    ExitTrigger->SetCollisionProfileName(TEXT("Trigger"));
    ExitTrigger->SetGenerateOverlapEvents(true);
    // 迷路の準備中に出口をふさぐ仮の壁（LoadingGate）を、Actorの標準部品を作成する（引数："LoadingGate"というUE文字列）に設定する。
    LoadingGate = CreateDefaultSubobject<UBoxComponent>(TEXT("LoadingGate"));
    // 迷路の準備中に出口をふさぐ仮の壁（LoadingGate）で、親部品との取り付け関係を設定する（引数：RootComponent）。
    LoadingGate->SetupAttachment(RootComponent);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    LoadingGate->SetCollisionProfileName(TEXT("BlockAll"));
    LoadingGate->SetCanEverAffectNavigation(false);
    // PrimaryActorTickの各項目を、以下で計算・取得した値へまとめて設定する。
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickInterval = .1f;
}

// この関数の役割：エディターでActorを配置・変更したときの組み立て処理。ここから本体を定義する。
void ACourtyardIntroCorridor::OnConstruction(const FTransform& Transform)
{
    // 親クラスの処理を呼ぶ：エディターでActorを配置・変更したときの組み立て処理（引数：位置・回転・拡大率（Transform））。
    Super::OnConstruction(Transform);
    // 設定されたメッシュで導入通路の床・壁・天井を作り直す。
    RebuildCorridor();
}

/** 指定寸法の見た目と独立したBox衝突を作る。メッシュ差し替え後も通路の衝突寸法を保つ。 */
void ACourtyardIntroCorridor::AddPiece(
    // 静的メッシュ（Mesh）、UMaterialInterface* Material（UMaterialInterface* Material）、const FVector& Center（const FVector& Center）、const FVector& Size)（const FVector& Size)）を、それぞれUStaticMeshとして用意する。参照先がない場合もあるポインターとして扱う。
    UStaticMesh* Mesh, UMaterialInterface* Material, const FVector& Center, const FVector& Size)
{
    // 見た目のメッシュとは別のBoxで衝突を作り、メッシュ側の衝突設定に依存させない。
    // Collision（Collision）を右辺から自動判定した型として用意し、指定型のUEオブジェクトを新規作成する（引数：this）で初期化する。参照先がない場合もあるポインターとして扱う。
    auto* Collision = NewObject<UBoxComponent>(this);
    // CollisionのCreationMethodを、EComponentCreationMethod::UserConstructionScriptに設定する。
    Collision->CreationMethod = EComponentCreationMethod::UserConstructionScript;
    // Collisionで、親部品との取り付け関係を設定する（引数：RootComponent）。
    Collision->SetupAttachment(RootComponent);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    Collision->SetRelativeLocation(Center);
    Collision->SetBoxExtent(Size * .5f);
    Collision->SetCollisionProfileName(TEXT("BlockAll"));
    Collision->SetCanEverAffectNavigation(true);
    // Collisionで、作成した部品をワールドへ登録し表示・衝突を有効にする。
    Collision->RegisterComponent();
    // CollisionPiecesで、要素または対応関係を末尾へ追加する（引数：Collision）。
    CollisionPieces.Add(Collision);
    if (!Mesh)
    {
        return;
    }
    // Visual（Visual）を右辺から自動判定した型として用意し、指定型のUEオブジェクトを新規作成する（引数：this）で初期化する。参照先がない場合もあるポインターとして扱う。
    auto* Visual = NewObject<UStaticMeshComponent>(this);
    // VisualのCreationMethodを、EComponentCreationMethod::UserConstructionScriptに設定する。
    Visual->CreationMethod = EComponentCreationMethod::UserConstructionScript;
    // Visualで、親部品との取り付け関係を設定する（引数：RootComponent）。
    Visual->SetupAttachment(RootComponent);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    Visual->SetStaticMesh(Mesh);
    Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Visual->SetCanEverAffectNavigation(false);
    // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、静的メッシュ（Mesh）で、メッシュの外接範囲を取得するで初期化する。
    const auto Bounds = Mesh->GetBounds();
    // MeshSize（MeshSize）を3次元ベクトルとして用意し、メッシュなどの外接範囲（Bounds）の外接Boxの半サイズと2の積で初期化する。
    const FVector MeshSize = Bounds.BoxExtent * 2;
    // 拡大率（Scale）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
    const FVector Scale(Size.X / FMath::Max(MeshSize.X, 1.f),
        // この行では範囲の大きさ（Size）のY成分を大きい方の値を選ぶ（引数：MeshSizeのY成分、1.f）で割った値を指定し、前後の行の計算・呼び出しへつなげる。
        Size.Y / FMath::Max(MeshSize.Y, 1.f),
        // 範囲の大きさ（Size）のZ成分を大きい方の値を選ぶ（引数：MeshSizeのZ成分、1.f）で割った値。
        Size.Z / FMath::Max(MeshSize.Z, 1.f));
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    Visual->SetRelativeScale3D(Scale);
    Visual->SetRelativeLocation(Center - Bounds.Origin * Scale);
    if (Material)
    {
        // Visualで、指定スロットへマテリアルを設定する（引数：0、使用するマテリアル（Material））。
        Visual->SetMaterial(0, Material);
    }
    // Visualで、作成した部品をワールドへ登録し表示・衝突を有効にする。
    Visual->RegisterComponent();
    // Piecesで、要素または対応関係を末尾へ追加する（引数：Visual）。
    Pieces.Add(Visual);
}

/** BPの寸法・メッシュ設定から、直線通路の床・側壁・天井・端壁を作り直す。 */
void ACourtyardIntroCorridor::RebuildCorridor()
{
    for (UStaticMeshComponent* C : Pieces)
    {
        if (IsValid(C))
        {
            // Cで、生成した部品を破棄する。
            C->DestroyComponent();
        }
    }
    Pieces.Reset();
    for (UBoxComponent* C : CollisionPieces)
    {
        if (IsValid(C))
        {
            // Cで、生成した部品を破棄する。
            C->DestroyComponent();
        }
    }
    CollisionPieces.Reset();
    // 対象の件数（Count）を符号付き32ビット整数として用意し、指定した最小値と最大値の範囲へ収める（引数：導入通路を構成する直線区画の個数（SegmentCount）、1、32）で初期化する。
    const int32 Count = FMath::Clamp(SegmentCount, 1, 32);
    // L（L）に大きい方の値を選ぶ（引数：導入通路の1区画の長さ（cm）（SegmentLength）、100.f）を格納する。W（W）に大きい方の値を選ぶ（引数：導入通路の内幅（cm）（InteriorWidth）、160.f）を格納する。
    const float L = FMath::Max(SegmentLength, 100.f), W = FMath::Max(InteriorWidth, 160.f),
                // Hを、FMath::Max(InteriorHeight, 200.f), T = FMath::Max(WallThickness, 5.f)に設定する。
                H = FMath::Max(InteriorHeight, 200.f), T = FMath::Max(WallThickness, 5.f);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    ExitTrigger->SetRelativeLocation(FVector(Count * L - 120, 0, H * .5f));
    ExitTrigger->SetBoxExtent(FVector(100, W * .5f - 10, H * .5f));
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    LoadingGate->SetRelativeLocation(FVector(Count * L - 10, 0, H * .5f));
    LoadingGate->SetBoxExtent(FVector(10, W * .5f, H * .5f));
    LoadingGate->SetCollisionEnabled(TransitionMode == ECourtyardIntroMode::WalkIntoMaze
                                         ? ECollisionEnabled::QueryAndPhysics
                                         : ECollisionEnabled::NoCollision);
    for (int32 I = 0; I < Count; ++I)
    {
        // X（X）を単精度の実数として用意し、Iとの5fの和とLの積で初期化する。
        const float X = (I + .5f) * L;
        // 導入通路の構成メッシュを寸法に合わせて作る（引数：床に使うメッシュ（FloorMesh）、床の材質（FloorMaterial）、3次元の位置・方向・寸法を組み立てる（引数：X、0、Tの符号を反転した値との5fの積）、3次元の位置・方向・寸法を組み立てる（引数：L、W、T））。
        AddPiece(FloorMesh, FloorMaterial, FVector(X, 0, -T * .5f), FVector(L, W, T));
        // 導入通路の構成メッシュを寸法に合わせて作る（引数：壁に使うメッシュ（WallMesh）、壁の材質（WallMaterial）、3次元の位置・方向・寸法を組み立てる（引数：X、WとTの和の符号を反転した値との5fの積、Hとの5fの積）、3次元の位置・方向・寸法を組み立てる（引数：L、T、H））。
        AddPiece(WallMesh, WallMaterial, FVector(X, -(W + T) * .5f, H * .5f), FVector(L, T, H));
        // 導入通路の構成メッシュを寸法に合わせて作る（引数：壁に使うメッシュ（WallMesh）、壁の材質（WallMaterial）、3次元の位置・方向・寸法を組み立てる（引数：X、WとTの和との5fの積、Hとの5fの積）、3次元の位置・方向・寸法を組み立てる（引数：L、T、H））。
        AddPiece(WallMesh, WallMaterial, FVector(X, (W + T) * .5f, H * .5f), FVector(L, T, H));
        if (bCeiling)
        {
            // 導入通路の構成メッシュを寸法に合わせて作る（引数：天井に使うメッシュ（CeilingMesh）、天井の材質（CeilingMaterial）、3次元の位置・方向・寸法を組み立てる（引数：X、0、HとTとの5fの積の和）、3次元の位置・方向・寸法を組み立てる（引数：L、W、T））。
            AddPiece(CeilingMesh, CeilingMaterial, FVector(X, 0, H + T * .5f), FVector(L, W, T));
        }
    }
    if (bCloseEntrance)
    {
        // 導入通路の構成メッシュを寸法に合わせて作る（引数：壁に使うメッシュ（WallMesh）、壁の材質（WallMaterial）、3次元の位置・方向・寸法を組み立てる（引数：Tの符号を反転した値との5fの積、0、Hとの5fの積）、3次元の位置・方向・寸法を組み立てる（引数：T、W、H））。
        AddPiece(WallMesh, WallMaterial, FVector(-T * .5f, 0, H * .5f), FVector(T, W, H));
    }
    // 「「導入通路の奥を壁で閉じる設定（bCloseExit）」かつ「レベル切替か同一マップ接続かの選択（TransitionMode）がECourtyardIntroMode::SceneTravelと等しい」」に当てはまる場合だけ、続く文を実行する。
    if (bCloseExit && TransitionMode == ECourtyardIntroMode::SceneTravel)
    {
        // 導入通路の奥を閉じる壁を、次行に指定するメッシュ・材質・位置・寸法で作る。
        AddPiece(
            // 通路長の奥端へ、壁厚を考慮した位置と内幅・内高に合う寸法で壁を配置する。
            WallMesh, WallMaterial, FVector(Count * L + T * .5f, 0, H * .5f), FVector(T, W, H));
    }
}

/** プレイヤーを置く導入通路側の開始位置をワールド座標で返す。 */
FVector ACourtyardIntroCorridor::GetEntrancePosition() const
{
    // Actorの位置・回転・拡大率を取得するで、ローカル位置を姿勢に従って変換する（引数：3次元の位置・方向・寸法を組み立てる（引数：160、0、0））を呼び出し元へ返す。
    return GetActorTransform().TransformPosition(FVector(160, 0, 0));
}

/** セグメント数と長さから、迷路と接続する通路の出口位置を返す。 */
FVector ACourtyardIntroCorridor::GetExitPosition() const
{
    // Actorの位置・回転・拡大率を取得するのTransformPosition(を呼び出し元へ返す。
    return GetActorTransform().TransformPosition(
        // 区画数を1〜32、区画長を最低100cmとして全長を計算し、ローカルX方向の出口位置を作る。
        FVector(FMath::Clamp(SegmentCount, 1, 32) * FMath::Max(SegmentLength, 100.f), 0, 0));
}

// この関数の役割：ゲーム開始時の初期化処理。ここから本体を定義する。
void ACourtyardIntroCorridor::BeginPlay()
{
    // 親クラスの処理を呼ぶ：ゲーム開始時の初期化処理。
    Super::BeginPlay();
    if (bSkipIntro)
    {
        return;
    }
    // OnActorBeginOverlapで、イベントの通知先となるメンバー関数を登録する（引数：this、ACourtyardIntroCorridor::HandleOverlapのアドレス）。
    OnActorBeginOverlap.AddDynamic(this, &ACourtyardIntroCorridor::HandleOverlap);
    // 「レベル切替か同一マップ接続かの選択（TransitionMode）がECourtyardIntroMode::WalkIntoMazeと異なる」に当てはまる場合だけ、続く文を実行する。
    if (TransitionMode != ECourtyardIntroMode::WalkIntoMaze)
    {
        return;
    }
    // ワールドのタイマー管理機能を取得するで、次フレームに処理を継続するよう予約する（引数：this、ACourtyardIntroCorridor::StartInlineMazeのアドレス）。
    GetWorldTimerManager().SetTimerForNextTick(this, &ACourtyardIntroCorridor::StartInlineMaze);
}

/** BPのBeginPlayより前に、既設の迷路生成Actorと導入通路を関連付ける。 */
void ACourtyardIntroCorridor::PostInitializeComponents()
{
    // 親クラスの処理を呼ぶ：部品の初期化完了後に開始位置などを整える。
    Super::PostInitializeComponents();
    // 移動オプションで導入を飛ばすかの結果（bSkipIntro）を、数字の文字列を整数へ変換する（引数：所属するワールドを取得するのURLで、文字列から指定項目を読む（引数："SkipIntro="というUE文字列、"0"というUE文字列））が0と異なるに設定する。
    bSkipIntro = FCString::Atoi(GetWorld()->URL.GetOption(TEXT("SkipIntro="), TEXT("0"))) != 0;
    // 「「所属するワールドを取得するで、実行中ゲームのワールドかを調べる」かつ「移動オプションで導入を飛ばすかの結果（bSkipIntro）」」に当てはまる場合だけ、続く文を実行する。
    if (GetWorld()->IsGameWorld() && bSkipIntro)
    {
        if (IsValid(ExistingMazeGenerator))
        {
            // 配置済みの迷路生成Actor（ExistingMazeGenerator）の従来の導入通路Actorを、nullptr（参照先なし）に設定する。
            ExistingMazeGenerator->IntroCorridor = nullptr;
        }
        // Actor全体をゲーム中に隠すか設定する（引数：true（成立・有効））。
        SetActorHiddenInGame(true);
        // Actorの衝突を有効にするか設定する（引数：false（不成立・無効））。
        SetActorEnableCollision(false);
        for (TActorIterator<AActor> It(GetWorld()); It; ++It)
        {
            // 「Itで、Actorに指定タグがあるか調べる（引数："CourtyardIntroLight"というUE文字列）」に当てはまる場合だけ、続く文を実行する。
            if (It->ActorHasTag(TEXT("CourtyardIntroLight")))
            {
                // Lights（Lights）を、括弧内の値を使ってTInlineComponentArray<ULightComponent*>として初期化する。参照先がない場合もあるポインターとして扱う。
                TInlineComponentArray<ULightComponent*> Lights(*It);
                for (auto* Light : Lights)
                {
                    // Lightで、部品の表示・非表示を設定する（引数：false（不成立・無効））。
                    Light->SetVisibility(false);
                }
            }
        }
        return;
    }
    // BPがBeginPlayで生成を開始する前に、配置済みの生成Actorへ通路の参照を渡す。
    // 「「所属するワールドを取得するで、実行中ゲームのワールドかを調べる」かつ「レベル切替か同一マップ接続かの選択（TransitionMode）がECourtyardIntroMode::WalkIntoMazeと等しい」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
    if (GetWorld()->IsGameWorld() && TransitionMode == ECourtyardIntroMode::WalkIntoMaze &&
        // 再利用する既設の迷路生成Actorが有効な場合だけ、そのActorへ導入通路の参照を渡す。
        IsValid(ExistingMazeGenerator))
    {
        // 接続先の迷路生成Actor（MazeGenerator）を、配置済みの迷路生成Actor（ExistingMazeGenerator）に設定する。
        MazeGenerator = ExistingMazeGenerator;
        // 接続先の迷路生成Actor（MazeGenerator）の従来の導入通路Actorを、thisに設定する。
        MazeGenerator->IntroCorridor = this;
    }
}

/** 同一マップ接続では既設Actorを再利用し、未指定なら設定された生成Actorを生成する。 */
void ACourtyardIntroCorridor::StartInlineMaze()
{
    if (IsValid(ExistingMazeGenerator))
    {
        // 接続先の迷路生成Actor（MazeGenerator）を、配置済みの迷路生成Actor（ExistingMazeGenerator）に設定する。
        MazeGenerator = ExistingMazeGenerator;
        // 接続先の迷路生成Actor（MazeGenerator）の従来の導入通路Actorを、thisに設定する。
        MazeGenerator->IntroCorridor = this;
        return;
    }
    if (!MazeGeneratorClass)
    {
        // 導入通路の接続に失敗した理由（ConnectionError）を、"Set Maze Generator Class in BP_IntroCorridor"というUE文字列に設定する。
        ConnectionError = TEXT("Set Maze Generator Class in BP_IntroCorridor");
        return;
    }
    // 位置・回転・拡大率（Transform）を、括弧内の値を使って位置・回転・拡大率として初期化する。
    const FTransform Transform(FRotator::ZeroRotator, GetExitPosition());
    // 接続先の迷路生成Actor（MazeGenerator）を、所属するワールドを取得するのSpawnActorDeferred<ACourtyardISMGenerator>(MazeGeneratorClass,に設定する。
    MazeGenerator = GetWorld()->SpawnActorDeferred<ACourtyardISMGenerator>(MazeGeneratorClass,
        // この行では位置・回転・拡大率（Transform）を指定し、前後の行の計算・呼び出しへつなげる。
        Transform,
        // 生成する迷路Actorの所有者として、この導入通路Actor自身を渡す。
        this,
        // この行ではnullptr（参照先なし）を指定し、前後の行の計算・呼び出しへつなげる。
        nullptr,
        // 生成地点がほかの衝突と重なってもActor自体は生成する。安全性は後の床・通行検査で確認する。
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
    if (!MazeGenerator)
    {
        // 導入通路の接続に失敗した理由（ConnectionError）を、"Could not spawn the maze generator"というUE文字列に設定する。
        ConnectionError = TEXT("Could not spawn the maze generator");
        return;
    }
    // 接続先の迷路生成Actor（MazeGenerator）の従来の導入通路Actorを、thisに設定する。
    MazeGenerator->IntroCorridor = this;
    // 接続先の迷路生成Actor（MazeGenerator）で、事前に参照を設定したActorの生成とConstructionを完了する（引数：位置・回転・拡大率（Transform））。
    MazeGenerator->FinishSpawning(Transform);
}

/** 迷路の生成完了を監視し、安全に入れる状態になったら出口の仮の遮断壁を解除する。 */
void ACourtyardIntroCorridor::Tick(float DeltaSeconds)
{
    // 親クラスの処理を呼ぶ：フレームごとの開始待ち・演出の進行処理（引数：前フレームからの経過秒数（DeltaSeconds））。
    Super::Tick(DeltaSeconds);
    // 「「「移動オプションで導入を飛ばすかの結果（bSkipIntro）」または「レベル切替か同一マップ接続かの選択（TransitionMode）がECourtyardIntroMode::WalkIntoMazeと異なる」」または「「接続先の迷路生成Actor（MazeGenerator）」が成立しない」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (bSkipIntro || TransitionMode != ECourtyardIntroMode::WalkIntoMaze || !MazeGenerator ||
        // 既に迷路へ入れる状態まで確認済みなら、以後のフレームで同じ準備確認を繰り返さない。
        bMazeReady)
    {
        return;
    }
    // 「接続先の迷路生成Actor（MazeGenerator）の生成失敗フラグ」に当てはまる場合だけ、続く文を実行する。
    if (MazeGenerator->bGenerationFailed)
    {
        // 導入通路の接続に失敗した理由（ConnectionError）を、接続先の迷路生成Actor（MazeGenerator）の生成失敗の理由に設定する。
        ConnectionError = MazeGenerator->GenerationError;
        return;
    }
    // 「接続先の迷路生成Actor（MazeGenerator）の生成完了フラグ」に当てはまる場合だけ、続く文を実行する。
    if (MazeGenerator->bGenerationComplete)
    {
        bMazeReady = true;
        // 迷路の準備中に出口をふさぐ仮の壁（LoadingGate）で、衝突判定の有効範囲を設定する（引数：ECollisionEnabled::NoCollision）。
        LoadingGate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    }
}

/** マップ切り替え方式の出口処理。プレイヤーだけを対象に、重複遷移を防いで次へ進む。 */
void ACourtyardIntroCorridor::HandleOverlap(AActor*, AActor* OtherActor)
{
    // 「「レベル切替か同一マップ接続かの選択（TransitionMode）がECourtyardIntroMode::SceneTravelと異なる」または「移動演出やレベル切替を開始したかのフラグ（bTransitionStarted）」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (TransitionMode != ECourtyardIntroMode::SceneTravel || bTransitionStarted ||
        // この行ではOtherActorが指定プレイヤー番号のキャラクターを取得する（引数：this、0）と異なるを指定し、前後の行の計算・呼び出しへつなげる。
        OtherActor != UGameplayStatics::GetPlayerCharacter(this, 0))
    {
        return;
    }
    bTransitionStarted = true;
    // 遷移先にも導入通路がある場合に備え、SkipIntroで二重表示を防ぐ。
    // 次のレベルへ移動し、SkipIntro=1を渡す。移動先でもう一度導入通路を作ることを避ける。
    UGameplayStatics::OpenLevel(this, NextLevelName, true, TEXT("SkipIntro=1"));
}

/** 通路幅を確保できる外周の入口を選び、迷路の位置・開始セル・遠いゴールを合わせる。 */
bool ACourtyardIntroCorridor::PrepareInlinePlan(
    // Generator（Generator）、FCourtyardPlan& Plan)（FCourtyardPlan& Plan)）を、それぞれACourtyardISMGeneratorとして用意する。参照先がない場合もあるポインターとして扱う。
    ACourtyardISMGenerator* Generator, FCourtyardPlan& Plan)
{
    EntranceCells.Reset();
    // 床セルの一覧または索引（Cells）をキーで値を引く表として宣言する。
    TMap<FIntPoint, ECourtyardCellKind> Cells;
    for (const auto& C : Plan.Cells)
    {
        // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：Cのセル座標、Cの床の種別）。
        Cells.Add(C.Position, C.Kind);
    }
    // EntranceRows（EntranceRows）を符号付き32ビット整数として用意し、大きい方の値を選ぶ（引数：1、端数を切り上げて整数にする（引数：導入通路の内幅（cm）（InteriorWidth）をGeneratorの1セルの実寸（cm）で割った値））で初期化する。
    const int32 EntranceRows = FMath::Max(1, FMath::CeilToInt(InteriorWidth / Generator->CellSize));
    bool Found = false;
    // Entrance（Entrance）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint Entrance = FIntPoint::ZeroValue;
    // BestX（BestX）を符号付き32ビット整数として用意し、MAX_int32で初期化する。
    int32 BestX = MAX_int32;
    // HalfRows（HalfRows）を単精度の実数として用意し、導入通路の内幅（cm）（InteriorWidth）を2とGeneratorの1セルの実寸（cm）の積で割った値との5fの和で初期化する。
    const float HalfRows = InteriorWidth / (2 * Generator->CellSize) + .5f;
    for (const auto& C : Plan.Cells)
    {
        // 「「Cの床の種別がECourtyardCellKind::Streetと異なる」または「Cのセル座標のX成分がBestXより大きい」」に当てはまる場合だけ、続く文を実行する。
        if (C.Kind != ECourtyardCellKind::Street || C.Position.X > BestX)
        {
            continue;
        }
        bool Clear = true;
        for (int32 Row = 0; Row < EntranceRows; ++Row)
        {
            // Kind（Kind）を右辺から自動判定した型として用意し、床セルの一覧または索引（Cells）で、キーに対応する値の参照を探す。未登録ならnullptr（引数：Cのセル座標と2次元の整数セル座標を組み立てる（引数：0、Row）の和）で初期化する。参照先がない場合もあるポインターとして扱う。
            const auto* Kind = Cells.Find(C.Position + FIntPoint(0, Row));
            // 「「「Kind」が成立しない」または「Kindの参照先の値がECourtyardCellKind::Streetと異なる」」に当てはまる場合だけ、続く文を実行する。
            if (!Kind || *Kind != ECourtyardCellKind::Street)
            {
                Clear = false;
                break;
            }
        }
        // CentreY（CentreY）を単精度の実数として用意し、Cのセル座標のY成分とEntranceRowsから1を引いた値との5fの積の和で初期化する。
        const float CentreY = C.Position.Y + (EntranceRows - 1) * .5f;
        for (const auto& Other : Plan.Cells)
        {
            // 「Otherのセル座標のX成分がCのセル座標のX成分より小さい」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
            if (Other.Position.X < C.Position.X &&
                // この行では符号を取り除いた絶対値を求める（引数：Otherのセル座標のY成分からCentreYを引いた値）がHalfRowsより小さいを指定し、前後の行の計算・呼び出しへつなげる。
                FMath::Abs(Other.Position.Y - CentreY) < HalfRows)
            {
                Clear = false;
                break;
            }
        }
        // 「「配置空間が空いているかの判定（Clear）」かつ「「「「Found」が成立しない」または「Cのセル座標のX成分がBestXより小さい」」または「Cのセル座標のY成分がEntranceのY成分より小さい」」」に当てはまる場合だけ、続く文を実行する。
        if (Clear && (!Found || C.Position.X < BestX || C.Position.Y < Entrance.Y))
        {
            Found = true;
            // Entranceを、Cのセル座標に設定する。
            Entrance = C.Position;
            // BestXを、EntranceのX成分に設定する。
            BestX = Entrance.X;
        }
    }
    if (!Found)
    {
        // 導入通路の接続に失敗した理由（ConnectionError）を、"No street boundary wide enough for the introductory corridor"というUE文字列に設定する。
        ConnectionError = TEXT("No street boundary wide enough for the introductory corridor");
        return false;
    }
    for (int32 Row = 0; Row < EntranceRows; ++Row)
    {
        // 壁を開ける入口セルの集合（EntranceCells）で、要素または対応関係を末尾へ追加する（引数：Entranceと2次元の整数セル座標を組み立てる（引数：0、Row）の和）。
        EntranceCells.Add(Entrance + FIntPoint(0, Row));
    }
    // 生成結果（Plan）のStartCellを、Entranceに設定する。
    Plan.StartCell = Entrance;
    // 新しい入口からの歩行距離を幅優先探索で計算し、最も遠い屋外床をゴールにする。
    // const FIntPoint Steps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    // 各セルまたは候補の距離情報（Distances）をキーで値を引く表として宣言する。
    TMap<FIntPoint, int32> Distances;
    // 各セルまたは候補の距離情報（Distances）で、要素または対応関係を末尾へ追加する（引数：Entrance、0）。
    Distances.Add(Entrance, 0);
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
    TArray<FIntPoint> Queue{Entrance};
    // 生成結果（Plan）のGoalCellを、Entranceに設定する。
    Plan.GoalCell = Entrance;
    int32 BestDistance = 0;
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (const auto& Step : Steps)
        {
            // Q（Q）を2次元の整数セル座標として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素と隣接セルへ進む差分（Step）の和で初期化する。
            const FIntPoint Q = Queue[I] + Step;
            // 「「「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：Q）」が成立しない」または「各セルまたは候補の距離情報（Distances）で、指定値が登録されているか調べる（引数：Q）」」に当てはまる場合だけ、続く文を実行する。
            if (!Cells.Contains(Q) || Distances.Contains(Q))
            {
                continue;
            }
            // 歩行距離（Distance）を符号付き32ビット整数として用意し、各セルまたは候補の距離情報（Distances）で、登録済みであることを前提にキーの値を取得する（引数：幅優先探索の待ち行列（Queue）の「I」に対応する要素）と1の和で初期化する。
            const int32 Distance = Distances.FindChecked(Queue[I]) + 1;
            // 各セルまたは候補の距離情報（Distances）で、要素または対応関係を末尾へ追加する（引数：Q、歩行距離（Distance））。
            Distances.Add(Q, Distance);
            // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：Q）。
            Queue.Add(Q);
            // 「「床セルの一覧または索引（Cells）で、登録済みであることを前提にキーの値を取得する（引数：Q）がECourtyardCellKind::Streetと等しい」かつ「歩行距離（Distance）が最遠候補までの歩行距離（BestDistance）より大きい」」に当てはまる場合だけ、続く文を実行する。
            if (Cells.FindChecked(Q) == ECourtyardCellKind::Street && Distance > BestDistance)
            {
                // 最遠候補までの歩行距離（BestDistance）を、歩行距離（Distance）に設定する。
                BestDistance = Distance;
                // 生成結果（Plan）のGoalCellを、Qに設定する。
                Plan.GoalCell = Q;
            }
        }
    }
    // 回転（Rotation）を、括弧内の値を使って回転角として初期化する。
    const FRotator Rotation(0, GetActorRotation().Yaw, 0);
    // LocalBoundary（LocalBoundary）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
    const FVector LocalBoundary((Entrance.X - .5f) * Generator->CellSize,
        // この行ではEntranceのY成分とEntranceRowsから1を引いた値との5fの積の和とGeneratorの1セルの実寸（cm）の積を指定し、前後の行の計算・呼び出しへつなげる。
        (Entrance.Y + (EntranceRows - 1) * .5f) * Generator->CellSize,
        // 入口境界のローカルZを0にし、生成する床面の高さを基準に接続位置を計算する。
        0);
    // 迷路生成Actor全体を、導入通路の出口へ接続する位置・回転へ移動する。
    Generator->SetActorLocationAndRotation(
        // 導入通路の出口位置をワールド座標で返すから回転（Rotation）のRotateVector(LocalBoundary), Rotationを引いた値。
        GetExitPosition() - Rotation.RotateVector(LocalBoundary), Rotation);
    // セル幅との端数だけを通路の外側で塞ぎ、通路内の歩行幅を狭めない。
    // 横方向または盤面の一辺（Side）を単精度の実数として用意し、EntranceRowsとGeneratorの1セルの実寸（cm）の積から導入通路の内幅（cm）（InteriorWidth）を引いた値との5fの積で初期化する。
    const float Side = (EntranceRows * Generator->CellSize - InteriorWidth) * .5f;
    // 「横方向または盤面の一辺（Side）が1より大きい」に当てはまる場合だけ、続く文を実行する。
    if (Side > 1)
    {
        for (float Sign : {-1.f, 1.f})
        {
            // セル幅の開口の余りをふさぐため、導入通路の奥へ追加の細い壁を作る。
            AddPiece(WallMesh,
                // 追加する細い壁にも、設定済みの壁マテリアルを使う。
                WallMaterial,
                // 通路の最奥から壁厚の半分だけ手前を、追加壁のローカルX位置にする。
                FVector(SegmentCount * SegmentLength - WallThickness * .5f,
                    // この行ではSignと導入通路の内幅（cm）（InteriorWidth）との5fの積と横方向または盤面の一辺（Side）との5fの積の和の積を指定し、前後の行の計算・呼び出しへつなげる。
                    Sign * (InteriorWidth * .5f + Side * .5f),
                    // この行では導入通路の内側の高さ（cm）（InteriorHeight）との5fの積を指定し、前後の行の計算・呼び出しへつなげる。
                    InteriorHeight * .5f),
                // 追加壁の寸法を、壁厚・余った片側幅・通路の内高で指定する。
                FVector(WallThickness, Side, InteriorHeight));
        }
    }
    return true;
}

/** 導入通路につながる境界かを返す。この部分には迷路側の外壁を置かない。 */
bool ACourtyardIntroCorridor::IsEntranceBoundary(FIntPoint Cell, int32 Direction) const
{
    // 「四方向の番号（Direction）が2と等しい」かつ「壁を開ける入口セルの集合（EntranceCells）で、指定値が登録されているか調べる（引数：Cell）」を呼び出し元へ返す。
    return Direction == 2 && EntranceCells.Contains(Cell);
}
