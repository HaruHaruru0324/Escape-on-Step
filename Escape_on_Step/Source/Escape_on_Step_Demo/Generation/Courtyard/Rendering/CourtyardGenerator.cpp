#include "Generation/Courtyard/Rendering/CourtyardGenerator.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreamingDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

namespace
{
    // const FIntPoint CourtyardRenderSteps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint CourtyardRenderSteps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

// このActorを作成するときに、標準部品や初期設定を用意するコンストラクター。
ACourtyardGenerator::ACourtyardGenerator()
{
    // PrimaryActorTickのbCanEverTickを、false（不成立・無効）に設定する。
    PrimaryActorTick.bCanEverTick = false;
    // RootComponentを、Actorの標準部品を作成する（引数："Root"というUE文字列）に設定する。
    RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

// この関数の役割：ゲーム開始時の初期化処理。ここから本体を定義する。
void ACourtyardGenerator::BeginPlay()
{
    // 親クラスの処理を呼ぶ：ゲーム開始時の初期化処理。
    Super::BeginPlay();
    if (bGenerateOnBeginPlay)
    {
        // 配置データを作り、従来のレベル部品方式で描画を開始する。
        GenerateCourtyard();
    }
}

// この関数の役割：終了時のタイマー解除や移動制限解除。ここから本体を定義する。
void ACourtyardGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する（引数：部品読み込みの確認予約（LoadingTimer））。
    GetWorldTimerManager().ClearTimer(LoadingTimer);
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：false（不成立・無効））。
    SetPlayerFrozen(false);
    // 親クラスの処理を呼ぶ：終了時のタイマー解除や移動制限解除（引数：終了または失敗の理由（Reason））。
    Super::EndPlay(Reason);
}

// この関数の役割：セル座標と高さをワールド座標へ変換する。ここから本体を定義する。
FVector ACourtyardGenerator::GetCellWorldPosition(FIntPoint Cell, float Height) const
{
    // Actorの位置・回転・拡大率を取得するのTransformPosition(を呼び出し元へ返す。
    return GetActorTransform().TransformPosition(
        // セル番号へ実寸を掛けたXYと指定高さからローカル位置を作り、Actorの姿勢でワールド位置へ変換する。
        FVector(Cell.X * CellSize, Cell.Y * CellSize, Height));
}

// この関数の役割：レベル部品の読み込み中にプレイヤーを停止または復帰する。ここから本体を定義する。
void ACourtyardGenerator::SetPlayerFrozen(bool Frozen)
{
    if (Frozen)
    {
        // 「FrozenPlayerで、保持している参照先が有効か調べる」に当てはまる場合だけ、続く文を実行する。
        if (FrozenPlayer.IsValid())
        {
            return;
        }
        // 「autoとPlayer = UGameplayStatics::GetPlayerCharacter(this, 0)の積」に当てはまる場合だけ、続く文を実行する。
        if (auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
        {
            // FrozenPlayerを、プレイヤーキャラクター（Player）に設定する。
            FrozenPlayer = Player;
            // PreviousMovementModeを、0〜255の符号なし整数へ変換する（引数：プレイヤーキャラクター（Player）で、キャラクターの移動制御部品を取得するのMovementMode）に設定する。
            PreviousMovementMode = uint8(Player->GetCharacterMovement()->MovementMode);
            // プレイヤーキャラクター（Player）で、キャラクターの移動制御部品を取得するで、キャラクターの移動を停止する。
            Player->GetCharacterMovement()->DisableMovement();
        }
    }
    // 前の条件が不成立で、さらに「autoとPlayer = FrozenPlayerで、保持している参照先を取得するの積」に当てはまる場合だけ、続く文を実行する。
    else if (auto* Player = FrozenPlayer.Get())
    {
        // プレイヤーキャラクター（Player）で、キャラクターの移動制御部品を取得するで、キャラクターの移動モードを設定する（引数：EMovementMode(PreviousMovementMode)）。
        Player->GetCharacterMovement()->SetMovementMode(EMovementMode(PreviousMovementMode));
        FrozenPlayer.Reset();
    }
}

// この関数の役割：エラーを記録して失敗を返す。ここから本体を定義する。
bool ACourtyardGenerator::Fail(const FString& Reason)
{
    // 生成失敗の理由（GenerationError）を、終了または失敗の理由（Reason）に設定する。
    GenerationError = Reason;
    bGenerationFailed = true;
    bGenerationComplete = false;
    bRendering = false;
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する（引数：部品読み込みの確認予約（LoadingTimer））。
    GetWorldTimerManager().ClearTimer(LoadingTimer);
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：false（不成立・無効））。
    SetPlayerFrozen(false);
    // ログへ計測結果・警告・失敗理由を記録する。引数の値を指定書式へ埋め込む。
    UE_LOG(LogTemp, Error, TEXT("COURTYARD_GENERATION failed: %s"), *Reason);
    return false;
}

// この関数の役割：生成したレベル部品・床・Actorを片付ける。ここから本体を定義する。
void ACourtyardGenerator::ClearCourtyard()
{
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する（引数：部品読み込みの確認予約（LoadingTimer））。
    GetWorldTimerManager().ClearTimer(LoadingTimer);
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：false（不成立・無効））。
    SetPlayerFrozen(false);
    for (UStaticMeshComponent* Component : GeneratedMeshes)
    {
        if (IsValid(Component))
        {
            // 対象のコンポーネント（Component）で、生成した部品を破棄する。
            Component->DestroyComponent();
        }
    }
    GeneratedMeshes.Reset();
    for (AActor* Actor : GeneratedActors)
    {
        if (IsValid(Actor))
        {
            // 対象のActor（Actor）で、Actorを破棄する。
            Actor->Destroy();
        }
    }
    GeneratedActors.Reset();
    for (ULevelStreamingDynamic* Module : LoadedModules)
    {
        if (IsValid(Module))
        {
            // 配置する1種類の部品データ（Module）で、生成したレベル部品の読み込み解除と削除を要求する（引数：true（成立・有効））。
            Module->SetIsRequestingUnloadAndRemoval(true);
        }
    }
    LoadedModules.Reset();
    ModulePlacements.Reset();
    PreparedModules.Reset();
    bRendering = false;
    bGenerationComplete = false;
    bGenerationFailed = false;
    GenerationError.Reset();
    PhysicallyReachableCells = 0;
}

// この関数の役割：配置データを作り、従来のレベル部品方式で描画を開始する。ここから本体を定義する。
bool ACourtyardGenerator::GenerateCourtyard()
{
    // 「「「所属するワールドを取得する」が成立しない」または「「所属するワールドを取得するで、実行中ゲームのワールドかを調べる」が成立しない」」に当てはまる場合だけ、続く文を実行する。
    if (!GetWorld() || !GetWorld()->IsGameWorld())
    {
        return false;
    }
    if (bRendering)
    {
        return false;
    }
    // 今回実際に使ったSeed（ActualSeed）を、「再現用Seed（Seed）が0より小さい」なら今回のランダムSeed用の整数を取得する、そうでなければ再現用Seed（Seed）に設定する。
    ActualSeed = Seed < 0 ? FMath::Rand() : Seed;
    // 今回描画する生成結果（NewPlan）を迷路全体の生成結果として宣言する。
    FCourtyardPlan NewPlan;
    // 失敗理由（Error）を文字列として宣言する。
    FString Error;
    // 「「従来方式の折れた空間とトンネルの配置データを作る（引数：生成設定（Settings）、今回実際に使ったSeed（ActualSeed）、今回描画する生成結果（NewPlan）、失敗理由（Error））」が成立しない」に当てはまる場合だけ、続く文を実行する。
    if (!UCourtyardLayoutLibrary::BuildCourtyardPlan(Settings, ActualSeed, NewPlan, Error))
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error））を呼び出し元へ返す。
        return Fail(Error);
    }
    // 渡された配置データを従来のレベル部品方式で描画する（引数：今回描画する生成結果（NewPlan））を呼び出し元へ返す。
    return RenderCourtyardPlan(NewPlan);
}

// この関数の役割：各床セルの境界へ壁・出入口・通路などの部品配置を計画する。ここから本体を定義する。
void ACourtyardGenerator::PlanModules()
{
    ModulePlacements.Reset();
    // 床セルの一覧または索引（Cells）をキーで値を引く表として宣言する。参照先がない場合もあるポインターとして扱う。
    TMap<FIntPoint, const FCourtyardCell*> Cells;
    for (const auto& C : Plan.Cells)
    {
        // 床セルの一覧または索引（Cells）で、要素または対応関係を末尾へ追加する（引数：Cのセル座標、Cのアドレス）。
        Cells.Add(C.Position, &C);
    }
    // 再現可能な乱数生成器（Random）を、括弧内の値を使って再現可能な乱数生成器として初期化する。
    FRandomStream Random(Plan.Seed ^ 0x31f7);
    // 拡大率（Scale）を単精度の実数として用意し、1セルの実寸（cm）（CellSize）を400.fで割った値で初期化する。
    const float Scale = CellSize / 400.f;
    // AddModule（AddModule）を、周囲の値を使って判定・計算する小さな関数として定義する。
    auto AddModule = [&](TSoftObjectPtr<UWorld> Level,
                         // ローカル座標またはローカル姿勢（Local）を3次元ベクトルとして引数で受け取る。
                         FVector Local,
                         // 水平回転（Yaw）を単精度の実数として引数で受け取る。
                         float Yaw,
                         // 直線トンネルに使う部品レベル（Passage）を真偽値として用意し、false（不成立・無効）で初期化する。
                         bool Passage = false,
                         // HideStart（HideStart）を真偽値として用意し、false（不成立・無効）で初期化する。
                         bool HideStart = false,
                         // HideEnd（HideEnd）を真偽値として用意し、false（不成立・無効）で初期化する。
                         bool HideEnd = false)
    {
        // P（P）を配置するレベル部品と姿勢として宣言する。
        FCourtyardModulePlacement P;
        // Pの各項目を、以下で計算・取得した値へまとめて設定する。
        P.Level = Level;
        P.Transform = FTransform(FRotator(0, Yaw, 0), Local, FVector(Scale)) * GetActorTransform();
        P.bPassage = Passage;
        P.bHideStartArch = HideStart;
        P.bHideEndArch = HideEnd;
        // 配置する部品と姿勢の一覧（ModulePlacements）で、要素または対応関係を末尾へ追加する（引数：P）。
        ModulePlacements.Add(P);
    };
    // 外壁のローカル範囲はX=0、Y=-400～0。建物側が+X、道側が-Xになるよう回す。
    // Boundary（Boundary）を、周囲の値を使って判定・計算する小さな関数として定義する。
    auto Boundary = [&](FIntPoint Cell, int32 Direction, TSoftObjectPtr<UWorld> Level)
    {
        // 水平回転（Yaw）を単精度の実数として用意し、四方向の番号（Direction）と90.fの積で初期化する。
        const float Yaw = Direction * 90.f;
        // 中央位置（Center）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
        const FVector Center(Cell.X * CellSize + CourtyardRenderSteps[Direction].X * CellSize * .5f,
            // この行ではCellのY成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの「四方向の番号（Direction）」に対応する要素のY成分と1セルの実寸（cm）（CellSize）の積との5fの積の和を指定し、前後の行の計算・呼び出しへつなげる。
            Cell.Y * CellSize + CourtyardRenderSteps[Direction].Y * CellSize * .5f,
            // 境界に置くレベル部品の基準Z座標を0にして、床面へ合わせる。
            0);
        // 選んだ部品レベルを、基準位置と方向に合う姿勢で配置予定へ追加する。
        AddModule(
            // Level, Centerから各軸の回転角を組み立てる（引数：0、水平回転（Yaw）、0）のRotateVector(FVector(0, -CellSize * .5f, 0)), Yawを引いた値。
            Level, Center - FRotator(0, Yaw, 0).RotateVector(FVector(0, -CellSize * .5f, 0)), Yaw);
    };
    for (const auto& C : Plan.Cells)
    {
        // 「Cの床の種別がECourtyardCellKind::Tunnelと等しい」に当てはまる場合だけ、続く文を実行する。
        if (C.Kind == ECourtyardCellKind::Tunnel)
        {
            // Link（Link）を右辺から自動判定した型として用意し、生成結果（Plan）のLinksの「Cの接続ID」に対応する要素で初期化する。元の値を参照し、コピーしない。
            const auto& Link = Plan.Links[C.LinkId];
            // First（First）にCのセル座標がLinkのセル列の「0」に対応する要素と等しいを格納する。Last（Last）にCのセル座標がLinkのセル列で、末尾の要素を取得すると等しいを格納する。
            const bool First = C.Position == Link.Cells[0], Last = C.Position == Link.Cells.Last();
            // RoadStart（RoadStart）を真偽値として用意し、で初期化する。
            const bool RoadStart =
                // トンネルの手前へ1セル戻った床の種別を読み、手前アーチを隠す必要があるか調べる。
                Cells.FindChecked(Link.Cells[0] - CourtyardRenderSteps[Link.Direction])->Kind ==
                // 手前の接続先が屋外床であれば、屋外へ直接開く側としてアーチの表示を扱う。
                ECourtyardCellKind::Street;
            // RoadEnd（RoadEnd）を真偽値として用意し、で初期化する。
            const bool RoadEnd =
                // トンネルの奥へ1セル進んだ床の種別を読み、奥アーチを隠す必要があるか調べる。
                Cells.FindChecked(Link.Cells.Last() + CourtyardRenderSteps[Link.Direction])->Kind ==
                // 奥の接続先が屋外床であれば、屋外へ直接開く側としてアーチの表示を扱う。
                ECourtyardCellKind::Street;
            // 四方向の番号（Direction）を符号付き32ビット整数として用意し、Linkの方向番号で初期化する。
            const int32 Direction = Link.Direction;
            // 水平回転（Yaw）を単精度の実数として用意し、四方向の番号（Direction）と90.fの積で初期化する。
            const float Yaw = Direction * 90.f;
            // 中央位置（Center）を、括弧内の値を使って3次元ベクトルとして初期化する。
            const FVector Center(C.Position.X * CellSize, C.Position.Y * CellSize, 0);
            // テーマで指定された直線トンネルの部品レベルを、配置予定へ追加する。
            AddModule(Theme->Passage,
                // トンネルの基準中央から部品レベルの原点のずれを引き、実際の配置位置を補正する。
                Center -
                    // この行では各軸の回転角を組み立てる（引数：0、水平回転（Yaw）、0）で、指定ベクトルを回転させる（引数：3次元の位置・方向・寸法を組み立てる（引数：1セルの実寸（cm）（CellSize）との5fの積、1セルの実寸（cm）（CellSize）の符号を反転した値との5fの積、0））を指定し、前後の行の計算・呼び出しへつなげる。
                    FRotator(0, Yaw, 0).RotateVector(FVector(CellSize * .5f, -CellSize * .5f, 0)),
                // この行では水平回転（Yaw）を指定し、前後の行の計算・呼び出しへつなげる。
                Yaw,
                // この行ではtrue（成立・有効）を指定し、前後の行の計算・呼び出しへつなげる。
                true,
                // この行では「「First」が成立しない」または「RoadStart」を指定し、前後の行の計算・呼び出しへつなげる。
                !First || RoadStart,
                // 「Last」かつ「RoadEnd」。
                Last && RoadEnd);
            // 「「First」かつ「RoadStart」」に当てはまる場合だけ、続く文を実行する。
            if (First && RoadStart)
            {
                // この行ではBoundary(C.Position - CourtyardRenderStepsの「四方向の番号（Direction）」に対応する要素を指定し、前後の行の計算・呼び出しへつなげる。
                Boundary(C.Position - CourtyardRenderSteps[Direction],
                    // この行では四方向の番号（Direction）を指定し、前後の行の計算・呼び出しへつなげる。
                    Direction,
                    // 壁・床・通路の見た目の参照データ（Theme）のPortalsの「再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、壁・床・通路の見た目の参照データ（Theme）のPortalsで、要素数を得るから1を引いた値）」に対応する要素。
                    Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)]);
            }
            // 「「Last」かつ「RoadEnd」」に当てはまる場合だけ、続く文を実行する。
            if (Last && RoadEnd)
            {
                // この行ではBoundary(C.Position + CourtyardRenderStepsの「四方向の番号（Direction）」に対応する要素を指定し、前後の行の計算・呼び出しへつなげる。
                Boundary(C.Position + CourtyardRenderSteps[Direction],
                    // この行では四方向の番号（Direction）と2の和を4で割った余りを指定し、前後の行の計算・呼び出しへつなげる。
                    (Direction + 2) % 4,
                    // 壁・床・通路の見た目の参照データ（Theme）のPortalsの「再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、壁・床・通路の見た目の参照データ（Theme）のPortalsで、要素数を得るから1を引いた値）」に対応する要素。
                    Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)]);
            }
        }
        // 前の条件が不成立で、さらに「Cの床の種別がECourtyardCellKind::Roomと異なる」に当てはまる場合だけ、続く文を実行する。
        else if (C.Kind != ECourtyardCellKind::Room)
        {
            for (int32 D = 0; D < 4; ++D)
            {
                // 「床セルの一覧または索引（Cells）で、指定値が登録されているか調べる（引数：Cのセル座標とCourtyardRenderStepsの「D」に対応する要素の和）」に当てはまる場合だけ、続く文を実行する。
                if (Cells.Contains(C.Position + CourtyardRenderSteps[D]))
                {
                    continue;
                }
                // 配置する部品レベル（Level）を右辺から自動判定した型として用意し、で初期化する。
                const auto Level =
                    // この床が屋内部屋に属するかを調べ、境界へ置く壁の種類を分ける。
                    C.Kind == ECourtyardCellKind::Room
                        // 屋内部屋の床なら、屋内用の壁・扉レベル部品を使う。
                        ? Theme->RoomFacade
                        // : ThemeのFacadesの「再現可能な乱数生成器（Random）で、指定範囲の整数をSeedに従って抽選する（引数：0、壁・床・通路の見た目の参照データ（Theme）のFacadesで、要素数を得るから1を引いた値）」に対応する要素。
                        : Theme->Facades[Random.RandRange(0, Theme->Facades.Num() - 1)];
                // この床セルの指定方向にある外周へ、選択した壁・出入口の配置を計画する。
                Boundary(C.Position, D, Level);
            }
        }
    }
    // Box（Box）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Box = [&](FVector Center, FVector Size)
    {
        // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュで、メッシュの外接範囲を取得するで初期化する。
        const auto Bounds = Theme->FloorMesh->GetBounds();
        // MeshScale（MeshScale）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
        FVector MeshScale(Size.X / (Bounds.BoxExtent.X * 2),
            // この行では範囲の大きさ（Size）のY成分をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのY成分と2の積で割った値を指定し、前後の行の計算・呼び出しへつなげる。
            Size.Y / (Bounds.BoxExtent.Y * 2),
            // 範囲の大きさ（Size）のZ成分をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのZ成分と2の積で割った値。
            Size.Z / (Bounds.BoxExtent.Z * 2));
        // C（C）を右辺から自動判定した型として用意し、指定型のUEオブジェクトを新規作成する（引数：this）で初期化する。参照先がない場合もあるポインターとして扱う。
        auto* C = NewObject<UStaticMeshComponent>(this);
        // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
        C->SetMobility(EComponentMobility::Movable);
        C->SetStaticMesh(Theme->FloorMesh);
        C->SetMaterial(0, Theme->CeilingMaterial);
        C->SetCollisionProfileName(TEXT("BlockAll"));
        // Cで、親部品との取り付け関係を設定する（引数：RootComponent）。
        C->SetupAttachment(RootComponent);
        // 作成した壁メッシュ部品を、指定したローカル位置・回転・拡大率へ設定する。
        C->SetRelativeTransform(
            // 回転は付けず、メッシュ原点のずれを差し引いた位置と計算済みの拡大率で配置する。
            FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * MeshScale, MeshScale));
        // Cで、作成した部品をワールドへ登録し表示・衝突を有効にする。
        C->RegisterComponent();
        // 生成したメッシュ部品一覧（GeneratedMeshes）で、要素または対応関係を末尾へ追加する（引数：C）。
        GeneratedMeshes.Add(C);
    };
    // 高さ（Height）に350.fと拡大率（Scale）の積を格納する。DoorHeight（DoorHeight）に260.fと拡大率（Scale）の積を格納する。Opening（Opening）に180.fと拡大率（Scale）の積を格納する。
    const float Height = 350.f * Scale, DoorHeight = 260.f * Scale, Opening = 180.f * Scale,
                // Thicknessを、16.fと拡大率（Scale）の積に設定する。
                Thickness = 16.f * Scale;
    for (const auto& Room : Plan.RoomAreas)
    {
        // R（R）を長方形区画の位置と寸法として宣言する。
        FCourtyardRoomSection R;
        // Rの各項目を、以下で計算・取得した値へまとめて設定する。
        R.Min = Room.Min;
        R.Size = Room.Size;
        for (int32 X = 0; X < R.Size.X; ++X)
        {
            for (int32 Y = 0; Y < R.Size.Y; ++Y)
            {
                for (int32 D = 0; D < 4; ++D)
                {
                    // P（P）にRの最小端と2次元の整数セル座標を組み立てる（引数：X、Y）の和を格納する。Q（Q）にPとCourtyardRenderStepsの「D」に対応する要素の和を格納する。
                    const auto P = R.Min + FIntPoint(X, Y), Q = P + CourtyardRenderSteps[D];
                    // 「「対象の部屋（Room）の実床セル一覧で、指定値が登録されているか調べる（引数：P）」が成立しない」に当てはまる場合だけ、続く文を実行する。
                    if (!Room.FloorCells.Contains(P))
                    {
                        continue;
                    }
                    // 「対象の部屋（Room）の実床セル一覧で、指定値が登録されているか調べる（引数：Q）」に当てはまる場合だけ、続く文を実行する。
                    if (Room.FloorCells.Contains(Q))
                    {
                        continue;
                    }
                    bool Door = false;
                    for (int32 I = 0; I < Room.DoorCells.Num(); ++I)
                    {
                        // Doorへ、「対象の部屋（Room）の扉を開ける室内セルの「I」に対応する要素がPと等しい」かつ「対象の部屋（Room）の各扉の外向き方向の「I」に対応する要素がDと等しい」とのビットORで該当するビットを残す。
                        Door |= Room.DoorCells[I] == P && Room.DoorDirections[I] == D;
                    }
                    // 中央位置（Center）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
                    const FVector Center(
                        // この行ではPのX成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの「D」に対応する要素のX成分と1セルの実寸（cm）（CellSize）の積との5fの積の和を指定し、前後の行の計算・呼び出しへつなげる。
                        P.X * CellSize + CourtyardRenderSteps[D].X * CellSize * .5f,
                        // この行ではPのY成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの「D」に対応する要素のY成分と1セルの実寸（cm）（CellSize）の積との5fの積の和を指定し、前後の行の計算・呼び出しへつなげる。
                        P.Y * CellSize + CourtyardRenderSteps[D].Y * CellSize * .5f,
                        // 壁の基準中央を、床と同じローカルZ=0の高さで求める。
                        0);
                    // 入口の幅方向（Tangent）を、括弧内の値を使って3次元ベクトルとして初期化する。
                    const FVector Tangent(-CourtyardRenderSteps[D].Y, CourtyardRenderSteps[D].X, 0);
                    // Wall（Wall）を、周囲の値を使って判定・計算する小さな関数として定義する。
                    const auto Wall = [&](float Offset, float Length, float Z, float H)
                    {
                        // 範囲の大きさ（Size）を3次元ベクトルとして用意し、Dを2で割った余りが0 ? FVector(Thickness, Length, H)と等しいで初期化する。
                        FVector Size = D % 2 == 0 ? FVector(Thickness, Length, H)
                                                  // この関数の役割：3次元の位置・方向・寸法を組み立てる。ここでは呼び出し方を宣言する。
                                                  : FVector(Length, Thickness, H);
                        // 壁の横ずれと高さを基準中央に足し、指定した寸法の薄いBox壁を配置する。
                        Box(Center + Tangent * Offset + FVector(0, 0, Z), Size);
                    };
                    if (!Door)
                    {
                        // 開口がない辺には、幅1セル分を全面的にふさぐ壁を配置する。
                        Wall(0, CellSize, Height * .5f, Height);
                    }
                    // 直前の条件に当てはまらなかった場合の処理へ進む。
                    else
                    {
                        // 横方向または盤面の一辺（Side）を単精度の実数として用意し、1セルの実寸（cm）（CellSize）からOpeningを引いた値との5fの積で初期化する。
                        const float Side = (CellSize - Opening) * .5f;
                        // 中央の扉開口の一方に、余った片側幅をふさぐ壁を配置する。
                        Wall(-(Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
                        // 扉開口の反対側にも同じ幅の壁を置き、両脇をふさぐ。
                        Wall((Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
                        // 扉の上端から壁の上端までをふさぐ、上側の壁を配置する。
                        Wall(0, CellSize, (Height + DoorHeight) * .5f, Height - DoorHeight);
                    }
                }
            }
        }
    }
}

// この関数の役割：指定セルに床または天井の描画データを追加する。ここから本体を定義する。
void ACourtyardGenerator::AddSurface(
    // Cell（Cell）、float Height（float Height）、UMaterialInterface* Material（UMaterialInterface* Material）、bool Ceiling)（bool Ceiling)）を、それぞれ2次元の整数セル座標として用意する。
    FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling)
{
    // 静的メッシュ（Mesh）をUStaticMeshとして用意し、壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュで初期化する。参照先がない場合もあるポインターとして扱う。
    UStaticMesh* Mesh = Theme->FloorMesh;
    // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、静的メッシュ（Mesh）で、メッシュの外接範囲を取得するで初期化する。
    const auto Bounds = Mesh->GetBounds();
    // 「「「メッシュなどの外接範囲（Bounds）の外接Boxの半サイズのX成分がの1より小さい」または「メッシュなどの外接範囲（Bounds）の外接Boxの半サイズのY成分がの1より小さい」」または「メッシュなどの外接範囲（Bounds）の外接Boxの半サイズのZ成分がの1より小さい」」に当てはまる場合だけ、続く文を実行する。
    if (Bounds.BoxExtent.X < .1 || Bounds.BoxExtent.Y < .1 || Bounds.BoxExtent.Z < .1)
    {
        return;
    }
    // メッシュ原点に依存せず、上面を床Z=0へ合わせる。天井は厚さ20cmの板とする。
    // Depth（Depth）を単精度の実数として用意し、「床でなく天井を作るかの指定（Ceiling）」なら20.f、そうでなければ12.fで初期化する。
    const float Depth = Ceiling ? 20.f : 12.f;
    // 拡大率（Scale）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
    const FVector Scale(CellSize / (Bounds.BoxExtent.X * 2),
        // この行では1セルの実寸（cm）（CellSize）をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのY成分と2の積で割った値を指定し、前後の行の計算・呼び出しへつなげる。
        CellSize / (Bounds.BoxExtent.Y * 2),
        // Depthをメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのZ成分と2の積で割った値。
        Depth / (Bounds.BoxExtent.Z * 2));
    // 中央位置（Center）を、括弧内の値を使って3次元ベクトルとして初期化する。
    const FVector Center(Cell.X * CellSize, Cell.Y * CellSize, Height - Depth * .5f);
    // 対象のコンポーネント（Component）を右辺から自動判定した型として用意し、指定型のUEオブジェクトを新規作成する（引数：this）で初期化する。参照先がない場合もあるポインターとして扱う。
    auto* Component = NewObject<UStaticMeshComponent>(this);
    // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
    Component->SetMobility(EComponentMobility::Movable);
    Component->SetStaticMesh(Mesh);
    if (Material)
    {
        // 対象のコンポーネント（Component）で、指定スロットへマテリアルを設定する（引数：0、使用するマテリアル（Material））。
        Component->SetMaterial(0, Material);
    }
    // 対象のコンポーネント（Component）で、衝突のプリセットを設定する（引数："BlockAll"というUE文字列）。
    Component->SetCollisionProfileName(TEXT("BlockAll"));
    // 対象のコンポーネント（Component）で、親部品との取り付け関係を設定する（引数：RootComponent）。
    Component->SetupAttachment(RootComponent);
    // 作成した床または天井の部品へ、計算した相対姿勢を設定する。
    Component->SetRelativeTransform(
        // メッシュ原点を補正した中心位置と拡大率を使い、回転なしの姿勢を部品に適用する。
        FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * Scale, Scale));
    // 対象のコンポーネント（Component）で、作成した部品をワールドへ登録し表示・衝突を有効にする。
    Component->RegisterComponent();
    // 生成したメッシュ部品一覧（GeneratedMeshes）で、要素または対応関係を末尾へ追加する（引数：対象のコンポーネント（Component））。
    GeneratedMeshes.Add(Component);
}

// この関数の役割：渡された配置データを従来のレベル部品方式で描画する。ここから本体を定義する。
bool ACourtyardGenerator::RenderCourtyardPlan(const FCourtyardPlan& NewPlan)
{
    // 「「「「所属するワールドを取得する」が成立しない」または「「所属するワールドを取得するで、実行中ゲームのワールドかを調べる」が成立しない」」または「レベル部品の読み込みを進めているかのフラグ（bRendering）」」に当てはまる場合だけ、続く文を実行する。
    if (!GetWorld() || !GetWorld()->IsGameWorld() || bRendering)
    {
        return false;
    }
    // 失敗理由（Error）を文字列として宣言する。
    FString Error;
    // 「「従来方式の床・部屋形状・トンネル・接続条件を検査する（引数：今回描画する生成結果（NewPlan）、失敗理由（Error））」が成立しない」に当てはまる場合だけ、続く文を実行する。
    if (!UCourtyardLayoutLibrary::ValidateCourtyardPlan(NewPlan, Error))
    {
        // エラーを記録して失敗を返す（引数：失敗理由（Error））を呼び出し元へ返す。
        return Fail(Error);
    }
    // 「「「「「壁・床・通路の見た目の参照データ（Theme）」が成立しない」または「「壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュ」が成立しない」」または「壁・床・通路の見た目の参照データ（Theme）のFacadesで、空かを調べる」」または「壁・床・通路の見た目の参照データ（Theme）のPortalsで、空かを調べる」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
    if (!Theme || !Theme->FloorMesh || Theme->Facades.IsEmpty() || Theme->Portals.IsEmpty() ||
        // この行では「壁・床・通路の見た目の参照データ（Theme）の直線トンネルに使う部品レベルで、アセット参照が未指定か調べる」または「壁・床・通路の見た目の参照データ（Theme）の屋内の外周に使う壁部品で、アセット参照が未指定か調べる」を指定し、前後の行の計算・呼び出しへつなげる。
        Theme->Passage.IsNull() || Theme->RoomFacade.IsNull())
    {
        // エラーを記録して失敗を返す（引数："Courtyard theme is incomplete"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Courtyard theme is incomplete"));
    }
    // 「「「「NaNや無限大でないかを確認する（引数：1セルの実寸（cm）（CellSize））」が成立しない」または「1セルの実寸（cm）（CellSize）が200より小さい」」または「1セルの実寸（cm）（CellSize）が800より大きい」」に当てはまる場合だけ、続く文を実行する。
    if (!FMath::IsFinite(CellSize) || CellSize < 200 || CellSize > 800)
    {
        // エラーを記録して失敗を返す（引数："CellSize must be 200..800 cm"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("CellSize must be 200..800 cm"));
    }
    // 旧ストリーミングの除去は同期して終わらせ、次の物理検査へ残さない。
    // 生成したレベル部品・床・Actorを片付ける。
    ClearCourtyard();
    // 所属するワールドを取得するで、保留中のレベル読み込みや表示変更を進める（引数：EFlushLevelStreamingType::Full）。
    GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
    // 生成結果（Plan）を、今回描画する生成結果（NewPlan）に設定する。
    Plan = NewPlan;
    // 今回実際に使ったSeed（ActualSeed）を、生成結果（Plan）の再現用Seedに設定する。
    ActualSeed = Plan.Seed;
    bRendering = true;
    ++GenerationSerial;
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：true（成立・有効））。
    SetPlayerFrozen(true);
    for (const auto& C : Plan.Cells)
    {
        // 現在の床セルへ、屋内・屋外の種別に合う床マテリアルのメッシュを配置する。
        AddSurface(C.Position,
            // 床の基準高さを、生成ActorのローカルZ=0に指定する。
            0,
            // 「Cの床の種別がECourtyardCellKind::Streetと等しい」なら壁・床・通路の見た目の参照データ（Theme）の屋外床の材質、そうでなければ壁・床・通路の見た目の参照データ（Theme）の屋内床の材質。
            C.Kind == ECourtyardCellKind::Street ? Theme->StreetMaterial : Theme->RoomMaterial);
        // 「Cの床の種別がECourtyardCellKind::Roomと等しい」に当てはまる場合だけ、続く文を実行する。
        if (C.Kind == ECourtyardCellKind::Room)
        {
            // 指定セルに床または天井の描画データを追加する（引数：Cのセル座標、350.fと1セルの実寸（cm）（CellSize）の積を400.fで割った値、壁・床・通路の見た目の参照データ（Theme）の天井の材質、true（成立・有効））。
            AddSurface(C.Position, 350.f * CellSize / 400.f, Theme->CeilingMaterial, true);
        }
    }
    // 各床セルの境界へ壁・出入口・通路などの部品配置を計画する。
    PlanModules();
    for (int32 I = 0; I < ModulePlacements.Num(); ++I)
    {
        // P（P）を右辺から自動判定した型として用意し、配置する部品と姿勢の一覧（ModulePlacements）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& P = ModulePlacements[I];
        bool Success = false;
        // Name（Name）を文字列として用意し、で初期化する。
        const FString Name =
            // 指定形式へ値を埋め込んだ文字列を作る（引数："Courtyard_%s_%d_%d"というUE文字列、名前を文字列として取得するをUEの文字配列ポインターとして取り出した値、作り直しの回数を区別する番号（GenerationSerial）、I）。
            FString::Printf(TEXT("Courtyard_%s_%d_%d"), *GetName(), GenerationSerial, I);
        // 配置する1種類の部品データ（Module）を右辺から自動判定した型として用意し、ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(で初期化する。参照先がない場合もあるポインターとして扱う。
        auto* Module = ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(
            // this, PのLevel, PのTransform, Success, Name。
            this, P.Level, P.Transform, Success, Name);
        // 「「「Success」が成立しない」または「「配置する1種類の部品データ（Module）」が成立しない」」に当てはまる場合だけ、続く文を実行する。
        if (!Success || !Module)
        {
            // エラーを記録して失敗を返す（引数："Failed to load module: ") + P.Level.ToString(というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Failed to load module: ") + P.Level.ToString());
        }
        // 読み込み済みのレベル部品（LoadedModules）で、要素または対応関係を末尾へ追加する（引数：配置する1種類の部品データ（Module））。
        LoadedModules.Add(Module);
    }
    // 部品の読み込みを開始した実時間（LoadingStarted）を、高精度な実時間を秒で取得するに設定する。
    LoadingStarted = FPlatformTime::Seconds();
    // 部品レベルの読み込み状況を一定間隔で確認するタイマーを予約する。まだ読めていない間は後から再確認する。
    GetWorldTimerManager().SetTimer(
        // LoadingTimer, this,とACourtyardGenerator::PollModules,の05f, trueのビットAND。
        LoadingTimer, this, &ACourtyardGenerator::PollModules, .05f, true);
    return true;
}

// この関数の役割：レベル部品の読み込み完了を確認し、準備できた部品を表示する。ここから本体を定義する。
void ACourtyardGenerator::PollModules()
{
    // BeginPlay時にPawnが未生成でも、見つかったフレームから移動を止める。
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：true（成立・有効））。
    SetPlayerFrozen(true);
    // 「高精度な実時間を秒で取得するから部品の読み込みを開始した実時間（LoadingStarted）を引いた値が90より大きい」に当てはまる場合だけ、続く文を実行する。
    if (FPlatformTime::Seconds() - LoadingStarted > 90)
    {
        // (void)Fail(TEXT("Courtyard modules timed out"))を呼び出し元へ返す。
        return (void)Fail(TEXT("Courtyard modules timed out"));
    }
    bool AllVisible = true;
    for (int32 I = 0; I < LoadedModules.Num(); ++I)
    {
        // 配置する1種類の部品データ（Module）をULevelStreamingDynamicとして用意し、読み込み済みのレベル部品（LoadedModules）の「I」に対応する要素で初期化する。参照先がない場合もあるポインターとして扱う。
        ULevelStreamingDynamic* Module = LoadedModules[I];
        // 「「「「配置する1種類の部品データ（Module）」が成立しない」または「「配置する1種類の部品データ（Module）で、レベル部品の読み込みが完了したか調べる」が成立しない」」または「「配置する1種類の部品データ（Module）で、読み込んだレベルが表示可能な状態か調べる」が成立しない」」に当てはまる場合だけ、続く文を実行する。
        if (!Module || !Module->IsLevelLoaded() || !Module->IsLevelVisible())
        {
            AllVisible = false;
            continue;
        }
        // 「PreparedModulesで、指定値が登録されているか調べる（引数：I）」に当てはまる場合だけ、続く文を実行する。
        if (PreparedModules.Contains(I))
        {
            continue;
        }
        // P（P）を右辺から自動判定した型として用意し、配置する部品と姿勢の一覧（ModulePlacements）の「I」に対応する要素で初期化する。元の値を参照し、コピーしない。
        const auto& P = ModulePlacements[I];
        // 通路を連結した境界でアーチを二重に重ねない。元アセットは編集せず、このインスタンスだけ除外する。
        // 「Pのこの配置が直線トンネルかの識別」に当てはまる場合だけ、続く文を実行する。
        if (P.bPassage)
        {
            for (AActor* Actor : Module->GetLoadedLevel()->Actors)
            {
                if (!IsValid(Actor))
                {
                    continue;
                }
                // Actorが持つコンポーネント一覧（Components）をTInlineComponentArray<UStaticMeshComponent*>として宣言する。参照先がない場合もあるポインターとして扱う。
                TInlineComponentArray<UStaticMeshComponent*> Components;
                // 対象のActor（Actor）で、Actorの部品一覧を取得する（引数：Actorが持つコンポーネント一覧（Components））。
                Actor->GetComponents(Components);
                for (auto* C : Components)
                {
                    // 「「「Cで、部品が使用する静的メッシュを取得する」が成立しない」または「Cで、部品が使用する静的メッシュを取得するで、名前を文字列として取得するが"SM_archT1"というUE文字列と異なる」」に当てはまる場合だけ、続く文を実行する。
                    if (!C->GetStaticMesh() || C->GetStaticMesh()->GetName() != TEXT("SM_archT1"))
                    {
                        continue;
                    }
                    // ローカル座標またはローカル姿勢（Local）を3次元ベクトルとして用意し、で初期化する。
                    const FVector Local =
                        // Pの位置・回転・拡大率で、ワールド位置をローカルへ逆変換する（引数：対象のActor（Actor）で、Actorの位置を取得する）。
                        P.Transform.InverseTransformPosition(Actor->GetActorLocation());
                    // 「「Pのトンネル手前のアーチを隠す設定」かつ「符号を取り除いた絶対値を求める（引数：ローカル座標またはローカル姿勢（Local）のX成分）が2より小さい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
                    if ((P.bHideStartArch && FMath::Abs(Local.X) < 2) ||
                        // 奥アーチを隠す指定があり、部品が元レベルの奥端X=400付近にある場合も非表示対象にする。
                        (P.bHideEndArch && FMath::Abs(Local.X - 400) < 2))
                    {
                        // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
                        Actor->SetActorHiddenInGame(true);
                        Actor->SetActorEnableCollision(false);
                    }
                }
            }
        }
        // PreparedModulesで、要素または対応関係を末尾へ追加する（引数：I）。
        PreparedModules.Add(I);
    }
    if (!AllVisible)
    {
        return;
    }
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する（引数：部品読み込みの確認予約（LoadingTimer））。
    GetWorldTimerManager().ClearTimer(LoadingTimer);
    // 「「実際の床支持と衝突で全床へ到達できるか検査する」が成立しない」に当てはまる場合だけ、続く文を実行する。
    if (!ValidatePhysicalLayout())
    {
        return;
    }
    bRendering = false;
    bGenerationComplete = true;
    // レベル部品の読み込み中にプレイヤーを停止または復帰する（引数：false（不成立・無効））。
    SetPlayerFrozen(false);
    // ログへ計測結果・警告・失敗理由を記録する。引数の値を指定書式へ埋め込む。
    UE_LOG(LogTemp,
        // 生成完了時のSeed・空間数・接続数・到達数などを、通常表示レベルのログへ出す。
        Display,
        // この後の書式文字列へ、次行以降に並べる計測値を埋め込む。
        TEXT(
            // 完了ログにSeed、空間数、接続数、周回数、床数、到達床数、部品数を表示する書式を指定する。
            "COURTYARD_READY seed=%d nodes=%d links=%d loops=%d floors=%d reachable=%d modules=%d"),
        // この行では今回実際に使ったSeed（ActualSeed）を指定し、前後の行の計算・呼び出しへつなげる。
        ActualSeed,
        // この行では生成結果（Plan）の広い空間の一覧で、要素数を得るを指定し、前後の行の計算・呼び出しへつなげる。
        Plan.Nodes.Num(),
        // この行では生成結果（Plan）のLinksで、要素数を得るを指定し、前後の行の計算・呼び出しへつなげる。
        Plan.Links.Num(),
        // この行では生成結果（Plan）の独立した周回路の件数を指定し、前後の行の計算・呼び出しへつなげる。
        Plan.LoopCount,
        // この行では生成結果（Plan）のセル列で、要素数を得るを指定し、前後の行の計算・呼び出しへつなげる。
        Plan.Cells.Num(),
        // この行では実際の衝突形状で到達できた床の件数（PhysicallyReachableCells）を指定し、前後の行の計算・呼び出しへつなげる。
        PhysicallyReachableCells,
        // 読み込み済みのレベル部品（LoadedModules）で、要素数を得る。
        LoadedModules.Num());
    // 迷路の生成完了をBPへ通知する。
    OnCourtyardReady();
}

// この関数の役割：実際の床支持と衝突で全床へ到達できるか検査する。ここから本体を定義する。
bool ACourtyardGenerator::ValidatePhysicalLayout()
{
    // プレイヤーキャラクター（Player）を右辺から自動判定した型として用意し、指定プレイヤー番号のキャラクターを取得する（引数：this、0）で初期化する。参照先がない場合もあるポインターとして扱う。
    auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (!Player)
    {
        // エラーを記録して失敗を返す（引数："Player character missing"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Player character missing"));
    }
    // Half（Half）を単精度の実数として用意し、プレイヤーキャラクター（Player）で、キャラクターの衝突カプセルを取得するで、拡大率適用後のカプセル半高を取得するで初期化する。
    const float Half = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // Radius（Radius）を単精度の実数として用意し、プレイヤーキャラクター（Player）で、キャラクターの衝突カプセルを取得するで、拡大率適用後のカプセル半径を取得すると3の和で初期化する。
    const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 3;
    // プレイヤーのカプセル形状（Capsule）をFCollisionShapeとして用意し、プレイヤー相当のカプセル形状を作る（引数：Radius、Half）で初期化する。
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, Half);
    // 判定する衝突オブジェクト種別（Objects）をFCollisionObjectQueryParamsとして宣言する。
    FCollisionObjectQueryParams Objects;
    // 判定する衝突オブジェクト種別（Objects）で、衝突検査に含めるオブジェクト種別を追加する（引数：ECC_WorldStatic）。
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    // 判定する衝突オブジェクト種別（Objects）で、衝突検査に含めるオブジェクト種別を追加する（引数：ECC_WorldDynamic）。
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    // 衝突判定の条件（Query）を、括弧内の値を使って衝突問い合わせの設定として初期化する。
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardWalkability), false);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        // 衝突判定の条件（Query）で、衝突検査から除外するActorを追加する（引数：イテレーターItが現在指しているActor）。
        Query.AddIgnoredActor(*It);
    }
    // Floors（Floors）を重複しない集合として宣言する。
    TSet<FIntPoint> Floors;
    for (const auto& C : Plan.Cells)
    {
        // Floorsで、要素または対応関係を末尾へ追加する（引数：Cのセル座標）。
        Floors.Add(C.Position);
    }
    // At（At）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto At = [&](FIntPoint P)
    {
        // セル座標と高さをワールド座標へ変換する（引数：P、Halfと5の和）を呼び出し元へ返す。
        return GetCellWorldPosition(P, Half + 5);
    };
    // 「所属するワールドを取得するのOverlapAnyTestByObjectType(」に当てはまる場合だけ、続く文を実行する。
    if (GetWorld()->OverlapAnyTestByObjectType(
            // 開始セルのキャラクター中心で、指定種別の障害物にカプセルが重なっていないか調べる。
            At(Plan.StartCell), FQuat::Identity, Objects, Capsule, Query))
    {
        // エラーを記録して失敗を返す（引数："Player start is blocked"というUE文字列）を呼び出し元へ返す。
        return Fail(TEXT("Player start is blocked"));
    }
    // プレイヤーキャラクター（Player）で、Actorの位置を設定する（引数：At(Plan.StartCell)、false（不成立・無効）、nullptr（参照先なし）、ETeleportType::TeleportPhysics）。
    Player->SetActorLocation(At(Plan.StartCell), false, nullptr, ETeleportType::TeleportPhysics);
    // 「autoとController = Playerで、キャラクターを操作するControllerを取得するの積」に当てはまる場合だけ、続く文を実行する。
    if (auto* Controller = Player->GetController())
    {
        // プレイヤーの操作Controller（Controller）で、操作視点の向きを設定する（引数：Actorの回転を取得する）。
        Controller->SetControlRotation(GetActorRotation());
    }
    // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する。
    TSet<FIntPoint> Seen{Plan.StartCell};
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する。
    TArray<FIntPoint> Queue{Plan.StartCell};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (auto D : CourtyardRenderSteps)
        {
            // 次に処理する位置または添字（Next）を右辺から自動判定した型として用意し、幅優先探索の待ち行列（Queue）の「I」に対応する要素とDの和で初期化する。
            const auto Next = Queue[I] + D;
            // 「「「Floorsで、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」が成立しない」または「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：次に処理する位置または添字（Next））」」に当てはまる場合だけ、続く文を実行する。
            if (!Floors.Contains(Next) || Seen.Contains(Next))
            {
                continue;
            }
            // 「「所属するワールドを取得するのSweepTestByObjectType(」が成立しない」に当てはまる場合だけ、続く文を実行する。
            if (!GetWorld()->SweepTestByObjectType(
                    // 現在セルから隣接セルまでのカプセル移動が、指定種別の障害物に遮られないか調べる。
                    At(Queue[I]), At(Next), FQuat::Identity, Objects, Capsule, Query))
            {
                // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                Seen.Add(Next);
                // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する（引数：次に処理する位置または添字（Next））。
                Queue.Add(Next);
            }
        }
    }
    // 実際の衝突形状で到達できた床の件数（PhysicallyReachableCells）を、探索済みの集合（Seen）で、要素数を得るに設定する。
    PhysicallyReachableCells = Seen.Num();
    // 「探索済みの集合（Seen）で、要素数を得るがFloorsで、要素数を得ると異なる」に当てはまる場合だけ、続く文を実行する。
    if (Seen.Num() != Floors.Num())
    {
        // Fail(FString::Printf(を呼び出し元へ返す。
        return Fail(FString::Printf(
            // "Module collision disconnects floor cells (%d/%d)"), Seen.Num(), Floors.Num(というUE文字列。
            TEXT("Module collision disconnects floor cells (%d/%d)"), Seen.Num(), Floors.Num()));
    }
    for (auto P : Floors)
    {
        // 衝突判定の結果（Hit）を衝突判定の結果として宣言する。
        FHitResult Hit;
        // 「「所属するワールドを取得するのLineTraceSingleByObjectType(」が成立しない」に当てはまる場合だけ、続く文を実行する。
        if (!GetWorld()->LineTraceSingleByObjectType(
                // この関数の役割：セル座標と高さをワールド座標へ変換する。ここから本体を定義する。
                Hit, GetCellWorldPosition(P, 60), GetCellWorldPosition(P, -60), Objects, Query))
        {
            // エラーを記録して失敗を返す（引数："Floor support missing"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Floor support missing"));
        }
    }
    if (GoalClass)
    {
        // 「所属するワールドを取得するのOverlapAnyTestByObjectType(」に当てはまる場合だけ、続く文を実行する。
        if (GetWorld()->OverlapAnyTestByObjectType(
                // ゴール位置のキャラクター中心で、配置を妨げる物体とのカプセルの重なりがないか検査する。
                At(Plan.GoalCell), FQuat::Identity, Objects, Capsule, Query))
        {
            // エラーを記録して失敗を返す（引数："Goal position blocked"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Goal position blocked"));
        }
        // Params（Params）をActor生成時の設定として宣言する。
        FActorSpawnParameters Params;
        // Paramsの各項目を、以下で計算・取得した値へまとめて設定する。
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // 探索の到達先（Goal）を右辺から自動判定した型として用意し、所属するワールドを取得するのSpawnActor<AActor>(で初期化する。参照先がない場合もあるポインターとして扱う。
        auto* Goal = GetWorld()->SpawnActor<AActor>(
            // この関数の役割：セル座標と高さをワールド座標へ変換する。ここでは呼び出し方を宣言する。
            GoalClass, GetCellWorldPosition(Plan.GoalCell), GetActorRotation(), Params);
        if (!Goal)
        {
            // エラーを記録して失敗を返す（引数："Goal spawn failed"というUE文字列）を呼び出し元へ返す。
            return Fail(TEXT("Goal spawn failed"));
        }
        // 生成したActorの一覧（GeneratedActors）で、要素または対応関係を末尾へ追加する（引数：探索の到達先（Goal））。
        GeneratedActors.Add(Goal);
    }
    return true;
}
