#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"
#include "Generation/Courtyard/Layout/CourtyardRoomShapes.h"
#include "Generation/Courtyard/Layout/CourtyardWallRegions.h"
#include "Components/CapsuleComponent.h"
#include "Components/DecalComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Camera/PlayerCameraManager.h"
#include "Generation/Courtyard/Intro/CourtyardIntroCorridor.h"
#include "Generation/Courtyard/Layout/CourtyardEntranceLayout.h"
#include "UObject/UnrealType.h"

namespace
{
    // const FIntPoint CourtyardRenderSteps[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
    const FIntPoint CourtyardRenderSteps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    // この関数の役割：保存された文字列の値を、対応するActorや部品のプロパティへ設定する。ここから本体を定義する。
    void ApplyProperties(UObject* Object, const TMap<FName, FString>& Values)
    {
        for (const auto& Pair : Values)
        {
            if (FProperty* Property = Object->GetClass()->FindPropertyByName(Pair.Key))
            {
                // 保存してある文字列形式の値を、このオブジェクトの対応するプロパティへ書き戻す。
                Property->ImportText_Direct(
                    // PairのValue, PropertyのContainerPtrToValuePtr<void>(Object), Object, PPF_NoneをUEの文字配列ポインターとして取り出した値。
                    *Pair.Value, Property->ContainerPtrToValuePtr<void>(Object), Object, PPF_None);
            }
        }
    }
}

// このActorを作成するときに、標準部品や初期設定を用意するコンストラクター。
ACourtyardISMGenerator::ACourtyardISMGenerator()
{
    bGenerateOnBeginPlay = false;
}

// この関数の役割：終了時のタイマー解除や移動制限解除。ここから本体を定義する。
void ACourtyardISMGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する
    GetWorldTimerManager().ClearTimer(BuildTimer);
    // ISM生成中のプレイヤー移動を停止または復帰する
    FreezePrototypePlayer(false);
    // 親クラスの処理を呼ぶ：終了時のタイマー解除や移動制限解除
    Super::EndPlay(Reason);
}

/** 再生成・終了時に、生成したISM・Actor・デカールと処理待ちのキューを片付ける。 */
void ACourtyardISMGenerator::ClearCourtyardISM()
{
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する
    GetWorldTimerManager().ClearTimer(BuildTimer);
    // ISM生成中のプレイヤー移動を停止または復帰する
    FreezePrototypePlayer(false);
    for (auto& Group : Groups)
    {
        if (IsValid(Group.Component))
        {
            // 同じ設定でまとめる描画グループ（Group）の描画部品で、生成した部品を破棄する。
            Group.Component->DestroyComponent();
        }
    }
    for (AActor* Actor : PrototypeActors)
    {
        if (IsValid(Actor))
        {
            // 対象のActor（Actor）で、Actorを破棄する。
            Actor->Destroy();
        }
    }
    for (UDecalComponent* Component : PrototypeDecals)
    {
        if (IsValid(Component))
        {
            // 対象のコンポーネント（Component）で、生成した部品を破棄する。
            Component->DestroyComponent();
        }
    }
    Groups.Reset();
    ActorQueue.Reset();
    DecalQueue.Reset();
    PrototypeActors.Reset();
    PrototypeDecals.Reset();
    ModulePlacements.Reset();
    EntranceCells.Reset();
    // 次に描画するグループの添字（GroupIndex）を、ActorIndex = DecalIndex = 0に設定する。
    GroupIndex = ActorIndex = DecalIndex = 0;
    bPrototypeRendering = false;
    // 生成完了フラグ（bGenerationComplete）を、bGenerationFailed = falseに設定する。
    bGenerationComplete = bGenerationFailed = false;
    GenerationError.Reset();
    PhysicallyReachableCells = 0;
    // 追加済みメッシュの総件数（PrototypeInstances）を、PrototypeISMComponents = PrototypeActorCount = PrototypeDecalCount = 0に設定する。
    PrototypeInstances = PrototypeISMComponents = PrototypeActorCount = PrototypeDecalCount =
        PrototypeTickCount = 0;
    // ISM描画にかかった秒数（PrototypeRenderSeconds）を、PrototypePreparationMilliseconds = PeakGenerationTickMilliseconds = 0に設定する。
    PrototypeRenderSeconds = PrototypePreparationMilliseconds = PeakGenerationTickMilliseconds = 0;
}

TSoftObjectPtr<UWorld> ACourtyardISMGenerator::SelectFacadeForNode_Implementation
(int32 NodeId, const TSoftObjectPtr<UWorld>& DefaultFacade)
{
	// BPで選択処理を用意するまでは、従来の壁を使用
	return DefaultFacade;
}

TSoftObjectPtr<UWorld> ACourtyardISMGenerator::SelectPortalForNode_Implementation(
	int32 NodeId,
	const TSoftObjectPtr<UWorld>& DefaultFacade)
{
	// BPで選択処理を用意するまでは従来のトンネルを使用
	return DefaultFacade;
}

/** 失敗状態と理由を記録し、プレイヤーの移動制限・暗転を解除する。 */
bool ACourtyardISMGenerator::PrototypeFail(const FString& Reason)
{
    // 生成失敗の理由（GenerationError）を、終了または失敗の理由（Reason）に設定する。
    GenerationError = Reason;
    bGenerationFailed = true;
    bGenerationComplete = false;
    bPrototypeRendering = false;
    // ワールドのタイマー管理機能を取得するで、指定した処理の継続予約を解除する
    GetWorldTimerManager().ClearTimer(BuildTimer);
    // ISM生成中のプレイヤー移動を停止または復帰する
    FreezePrototypePlayer(false);

    // 「autoとCamera = UGameplayStatics::GetPlayerCameraManager(this, 0)の積」に当てはまる場合だけ実行
    if (auto* Camera = UGameplayStatics::GetPlayerCameraManager(this, 0))
    {
        // プレイヤーカメラ（Camera）で、カメラの暗転を解除する。
        Camera->StopCameraFade();
    }

    UE_LOG(LogTemp, Error, TEXT("COURTYARD_ISM failed: %s"), *Reason);
    return false;
}

/** 同じメッシュ・材質・衝突設定を持つ配置を1つのISMグループへまとめる。 */
void ACourtyardISMGenerator::QueueMesh(const FCourtyardISMMesh& Entry)
{
    // 「「今回の配置データまたは対応表の1件（Entry）のメッシュ参照」が成立しない」に当てはまる場合だけ実行
    if (!Entry.Mesh)
    {
        return;
    }
    // 配置位置はグループ分けに含めず、メッシュ・衝突・描画設定が同じものをまとめる。
    // KeyEntry（KeyEntry）をISMのメッシュ・材質・衝突・配置設定として用意し、今回の配置データまたは対応表の1件（Entry）で初期化する。
    FCourtyardISMMesh KeyEntry = Entry;
    // KeyEntryの各項目を、以下で計算・取得した値へまとめて設定する。
    KeyEntry.LocalTransform = FTransform::Identity;
    KeyEntry.bStartArch = KeyEntry.bEndArch = false;
    // 配列の添字（Index）を符号付き32ビット整数として用意し、共通設定ごとのISM描画グループ一覧（Groups）のIndexOfByPredicateで初期化する。
    int32 Index = Groups.IndexOfByPredicate(
        // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
        [&](const FCourtyardISMGroup& Group)
        {
            // UEへ登録した構造体の型情報を取得するのCompareScriptStruct(を呼び出し元へ返す。
            return FCourtyardISMMesh::StaticStruct()->CompareScriptStruct(
                // 同じ設定でまとめる描画グループ（Group）のSettings,のアドレスとKeyEntry, PPF_NoneのビットAND。
                &Group.Settings, &KeyEntry, PPF_None);
        });
    // 「配列の添字（Index）がINDEX_NONE（未指定を表す-1）と等しい」に当てはまる場合だけ実行
    if (Index == INDEX_NONE)
    {
        // 配列の添字（Index）を、共通設定ごとのISM描画グループ一覧（Groups）で、既定値の要素を1個追加して、その添字を返すに設定する。
        Index = Groups.AddDefaulted();
        // 共通設定ごとのISM描画グループ一覧（Groups）の「配列の添字（Index）」に対応する要素の生成設定を、KeyEntryに設定する。
        Groups[Index].Settings = KeyEntry;
    }
    // 共通設定ごとのISM描画グループ一覧（Groups）の「配列の添字（Index）」に対応する要素の配置姿勢一覧で、要素または対応関係を末尾へ追加する
    Groups[Index].Transforms.Add(Entry.LocalTransform);
    ++PrototypeInstances;
}

/** セル1マス分の床または天井を、使用メッシュの実寸に合わせて配置キューへ加える。 */
void ACourtyardISMGenerator::AddSurface(
    // Cell（Cell）、float Height、UMaterialInterface* Material、bool Ceilingを、それぞれ2次元の整数セル座標として用意する。
    FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling)
{
    // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、壁・床・通路の見た目の
	// 参照データ（Theme）の床に使うメッシュで、メッシュの外接範囲を取得するで初期化する。
    const auto Bounds = Theme->FloorMesh->GetBounds();
    // 「メッシュなどの外接範囲（Bounds）の外接Boxの半サイズで、最小端の座標を取得するがの1より小さい」に当てはまる場合だけ実行
    if (Bounds.BoxExtent.GetMin() < .1)
    {
        return;
    }
    // Depth（Depth）を単精度の実数として用意し、「床でなく天井を作るかの指定（Ceiling）」なら20.f、そうでなければ12.fで初期化
    const float Depth = Ceiling ? 20.f : 12.f;
    // 拡大率（Scale）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
    const FVector Scale(CellSize / (Bounds.BoxExtent.X * 2),
        // この行では1セルの実寸（cm）（CellSize）をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのY成分と2の積で割った値を指定し、前後の行の計算・呼び出しへつなげる。
        CellSize / (Bounds.BoxExtent.Y * 2),
        // Depthをメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのZ成分と2の積で割った値。
        Depth / (Bounds.BoxExtent.Z * 2));
    // 中央位置（Center）を、括弧内の値を使って3次元ベクトルとして初期化する。
    const FVector Center(Cell.X * CellSize, Cell.Y * CellSize, Height - Depth * .5f);
    // 今回の配置データまたは対応表の1件（Entry）をISMのメッシュ・材質・衝突・配置設定として宣言する。
    FCourtyardISMMesh Entry;
    // 今回の配置データまたは対応表の1件（Entry）のメッシュ参照を、壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュに設定する。
    Entry.Mesh = Theme->FloorMesh;
    // 今回の配置データまたは対応表の1件（Entry）のマテリアル一覧で、要素または対応関係を末尾へ追加する
    Entry.Materials.Add(Material);
    // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を設定する。
    Entry.LocalTransform =
        // 回転なしの床・天井姿勢を作り、メッシュ原点のずれを補正したうえで次行のActor姿勢と合成する。
        FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * Scale, Scale) *
        // Actorの位置・回転・拡大率を取得する。
        GetActorTransform();
    // 同じ描画・衝突設定のメッシュをISMグループへまとめる
    QueueMesh(Entry);
}

/** 配置データを検査し、導入通路との接続を反映して、描画用の各キューを準備する。 */
bool ACourtyardISMGenerator::RenderCourtyardISMPlan(const FCourtyardPlan& NewPlan)
{
    // 「「「「所属するワールドを取得する」が成立しない」または
	// 「「所属するワールドを取得するで、実行中ゲームのワールドかを調べる」が成立しない」」
	// または「ISMの分割描画を進めているかのフラグ（bPrototypeRendering）」」に当てはまる場合だけ実行
    if (!GetWorld() || !GetWorld()->IsGameWorld() || bPrototypeRendering)
    {
        return false;
    }
    // 失敗理由（Error）を文字列として宣言する。
    FString Error;
    // 「「従来方式の床・部屋形状・トンネル・接続条件を検査する（引数：今回描画する生成結果（NewPlan）、失敗理由（Error））」が成立しない」に当てはまる場合だけ実行
    if (!UCourtyardLayoutLibrary::ValidateCourtyardPlan(NewPlan, Error))
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する（引数：失敗理由（Error））を呼び出し元へ返す。
        return PrototypeFail(Error);
    }
    // 「「「「「壁・床・通路の見た目の参照データ（Theme）」が成立しない」
	// または「「壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュ」が成立しない」」
	// または「壁・床・通路の見た目の参照データ（Theme）のFacadesで、空かを調べる」」または
	// 「壁・床・通路の見た目の参照データ（Theme）のPortalsで、空かを調べる」」、
	// または次行の条件に当てはまる場合だけ実行
    if (!Theme || !Theme->FloorMesh || Theme->Facades.IsEmpty() || Theme->Portals.IsEmpty() || !ModuleCatalog)
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する
        return PrototypeFail(TEXT("Prototype theme/catalog is incomplete"));
    }
    // 「「「「NaNや無限大でないかを確認する
    if (!FMath::IsFinite(CellSize) || CellSize < 200 || CellSize > 800)
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する（引数："Invalid CellSize"というUE文字列）を呼び出し元へ返す。
        return PrototypeFail(TEXT("Invalid CellSize"));
    }
    // 別マップの導入から遷移済みの場合は、導入メッシュを二度表示しない。
    // 「数字の文字列を整数へ変換する（引数：所属するワールドを取得するのURLで、
	// 文字列から指定項目を読む（引数："SkipIntro="というUE文字列、"0"というUE文字列））が0と異なる」、
	// かつ次行の条件に当てはまる場合だけ実行
    if (FCString::Atoi(GetWorld()->URL.GetOption(TEXT("SkipIntro="), TEXT("0"))) != 0 &&
        // 自作の導入エリアActorが有効なら、生成中にプレイヤーの移動を停止する対象とする。
        IsValid(EntranceArea))
    {
        // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
        EntranceArea->SetActorHiddenInGame(true);
        EntranceArea->SetActorEnableCollision(false);
        EntranceArea = nullptr;
    }
    // 生成済みISM・小物・作業待ちを片付ける。
    ClearCourtyardISM();
    // ISM描画を開始した実時間（秒）（BuildStarted）を、高精度な実時間を秒で取得するに設定する。
    BuildStarted = FPlatformTime::Seconds();
    // 生成結果（Plan）を、今回描画する生成結果（NewPlan）に設定する。
    Plan = NewPlan;
    // 部屋の中に閉じ込められた空白だけを埋める。外へ開くU字のくぼみは残す。
    const int32 FilledHoles = UCourtyardRoomShapes::FillEnclosedRoomHoles(Plan);
    const int32 ReshapedRooms = UCourtyardRoomShapes::RemovePlainRectangularRooms(Plan,
        bBlueprintEntranceReady ? BlueprintEntrance.Cells : TArray<FIntPoint>());
    if ((FilledHoles > 0 || ReshapedRooms > 0) && !UCourtyardLayoutLibrary::ValidateCourtyardPlan(Plan, Error))
    {
        return PrototypeFail(Error);
    }

    if (IsValid(EntranceArea))
    {
        // 「「自作導入エリアを抽選入口へ接続し、開始位置を整える」が成立しない」に当てはまる場合だけ実行
        if (!PrepareCustomEntrance())
        {
            return false;
        }
    }
    // 前の条件が不成立で、さらに「オブジェクトが存在し、破棄待ちでもないかを調べる
    else if (IsValid(IntroCorridor))
    {
        // 「「従来の導入通路Actor（IntroCorridor）で、従来の導入通路へつながる入口を探し、
		// 開始とゴールを更新する（引数：this、生成結果（Plan））」が成立しない」に当てはまる場合だけ実行
        if (!IntroCorridor->PrepareInlinePlan(this, Plan))
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            return PrototypeFail(IntroCorridor->ConnectionError);
        }
    }
    // 独立マップとして開始する場合も、左端固定にせず開始セルと遠いゴールを選ぶ。
    if (!IsValid(EntranceArea) && !IsValid(IntroCorridor))
    {
        // StartSelection（StartSelection）を入口候補の情報として宣言する。
        FCourtyardEntranceSelection StartSelection;
        // 「「CourtyardEntranceLayout::SelectEntrance(」が成立しない」に当てはまる場合だけ実行
        if (!CourtyardEntranceLayout::SelectEntrance(
                // この行ではPlan, CellSize, CellSize, 0, PlanのSeed, StartSelection, Errorを指定し、
				// 前後の行の計算・呼び出しへつなげる。
                Plan, CellSize, CellSize, 0, Plan.Seed, StartSelection, Error))
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する（引数：失敗理由（Error））を呼び出し元へ返す。
            return PrototypeFail(Error);
        }
    }
    // BP側のゴール・敵配置も、入口決定後の同じPlanを使う。
    // 「autoとProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("ResultPlan"))の積」に当てはまる場合だけ実行
    if (auto* Property = FindFProperty<FStructProperty>(GetClass(), TEXT("ResultPlan"));
        // この行では「Property」かつ「PropertyのStructがUEへ登録した構造体の型情報を取得すると等しい」を指定し、
		// 前後の行の計算・呼び出しへつなげる。
        Property && Property->Struct == FCourtyardPlan::StaticStruct())
    {
        // Propertyで、構造体などの値をまとめてコピーする（引数：Propertyで、
		// オブジェクト内のプロパティの格納位置を取得する（引数：this）、生成結果（Plan）のアドレス）。
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(this), &Plan);
    }

    // 今回実際に使ったSeed（ActualSeed）を、生成結果（Plan）の再現用Seedに設定する。
    ActualSeed = Plan.Seed;
    bPrototypeRendering = true;
    // ISM生成中のプレイヤー移動を停止または復帰する（引数：true（成立・有効））。
    FreezePrototypePlayer(true);
    for (const auto& Cell : Plan.Cells)
    {
        // 現在の床セルに、種別に合った床マテリアルを使う描画データを追加する。
        AddSurface(Cell.Position,
            // 床は生成ActorのローカルZ=0の高さへ配置する。
            0,
            // 屋内部屋以外は地面用材質を使う。トンネルにも滑らかな屋内床材を使わない。
            Cell.Kind == ECourtyardCellKind::Room ? Theme->RoomMaterial : Theme->StreetMaterial);
        // 「Cellの床の種別がECourtyardCellKind::Roomと等しい」に当てはまる場合だけ実行
        //if (Cell.Kind == ECourtyardCellKind::Room)
        //{
        //    // 指定セルに床または天井の描画データを追加する
        //    AddSurface(Cell.Position, 350.f * CellSize / 400.f, Theme->CeilingMaterial, true);
        //}
    }
    // 各床セルの境界へ壁・出入口・通路などの部品配置を計画する。
    PlanModules();
    // セル幅の開口の両端を実際の通路幅まで狭める。
    QueueEntranceJambs();
    for (const auto& Placement : ModulePlacements)
    {
        // 配置する1種類の部品データ（Module）を右辺から自動判定した型として用意し、
		// レベルから抽出した配置部品のカタログ（ModuleCatalog）のModules.FindByPredicateで初期化する
        const auto* Module = ModuleCatalog->Modules.FindByPredicate(
            // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る
            [&](const FCourtyardISMModule& Item)
            {
                // Itemの元になった部品レベルの参照パスが配置する姿勢と部品情報（Placement）の配置する部品レベルで、
				// 参照先アセットのパスを取得すると等しいを呼び出し元へ返す。
                return Item.SourceLevel == Placement.Level.ToSoftObjectPath();
            });
        if (!Module)
        {
            // PrototypeFail(を呼び出し元へ返す。
            return PrototypeFail(
                // "Module missing from prototype catalog: ") + Placement.Level.ToString(というUE文字列。
                TEXT("Module missing from prototype catalog: ") + Placement.Level.ToString());
        }
        for (const auto& Item : Module->Meshes)
        {
            // 「「配置する姿勢と部品情報（Placement）のこの配置が直線トンネルかの識別」かつ
			// 「((Placement.bHideStartArch && Item.bStartArch)」」、または次行の条件に当てはまる場合だけ実行
            if (Placement.bPassage && ((Placement.bHideStartArch && Item.bStartArch) ||
                                          // トンネルの奥アーチを隠す指定があり、そのメッシュが奥アーチ用なら描画対象から外す。
                                          (Placement.bHideEndArch && Item.bEndArch)))
            {
                continue;
            }
            // 今回の配置データまたは対応表の1件（Entry）を右辺から自動判定した型として用意し、Itemで初期化する。
            auto Entry = Item;
            // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を、
			// Itemのローカル配置姿勢と配置する姿勢と部品情報（Placement）の位置・回転・拡大率の積に設定する。
            Entry.LocalTransform = Placement.LocalAdjustment.Equals(FTransform::Identity)
                ? Item.LocalTransform * Placement.Transform
                : FTransform(Item.LocalTransform.ToMatrixWithScale() *
                    Placement.LocalAdjustment.ToMatrixWithScale() * Placement.Transform.ToMatrixWithScale());
            // 同じ描画・衝突設定のメッシュをISMグループへまとめる
            QueueMesh(Entry);
        }
        for (const auto& Item : Module->Actors)
        {
            // 今回の配置データまたは対応表の1件（Entry）を右辺から自動判定した型として用意し、Itemで初期化する。
            auto Entry = Item;
            // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を、
			// Itemのローカル配置姿勢と配置する姿勢と部品情報（Placement）の位置・回転・拡大率の積に設定する。
            Entry.LocalTransform = Placement.LocalAdjustment.Equals(FTransform::Identity)
                ? Item.LocalTransform * Placement.Transform
                : FTransform(Item.LocalTransform.ToMatrixWithScale() *
                    Placement.LocalAdjustment.ToMatrixWithScale() * Placement.Transform.ToMatrixWithScale());
            // Actorの生成待ち一覧（ActorQueue）で、要素または対応関係を末尾へ追加する
            ActorQueue.Add(MoveTemp(Entry));
        }
        for (const auto& Item : Module->Decals)
        {
            // 今回の配置データまたは対応表の1件（Entry）を右辺から自動判定した型として用意し、Itemで初期化する
            auto Entry = Item;
            // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を、
			// Itemのローカル配置姿勢と配置する姿勢と部品情報（Placement）の位置・回転・拡大率の積に設定する
            Entry.LocalTransform = Placement.LocalAdjustment.Equals(FTransform::Identity)
                ? Item.LocalTransform * Placement.Transform
                : FTransform(Item.LocalTransform.ToMatrixWithScale() *
                    Placement.LocalAdjustment.ToMatrixWithScale() * Placement.Transform.ToMatrixWithScale());
            // デカールの生成待ち一覧（DecalQueue）で、要素または対応関係を末尾へ追加する
            DecalQueue.Add(MoveTemp(Entry));
        }
    }
    // 描画開始前の準備時間（ms）（PrototypePreparationMilliseconds）を、
	// 高精度な実時間を秒で取得するからISM描画を開始した実時間（秒）（BuildStarted）を引いた値と1000の積に設定する。
    PrototypePreparationMilliseconds = (FPlatformTime::Seconds() - BuildStarted) * 1000;
    // 次のフレームから小分けに配置し、1フレームで全ての生成を実行しない。
    // ISM描画の継続予約（BuildTimer）を、に設定する。
    BuildTimer =
        // ワールドのタイマー管理機能を取得するで、次フレームに処理を継続するよう予約する
        GetWorldTimerManager().SetTimerForNextTick(this, &ACourtyardISMGenerator::BuildNextBatch);
    return true;
}

/** 1フレームの時間・個数予算内でISM・Actor・デカールを追加し、完了時に通行検査する。 */
void ACourtyardISMGenerator::BuildNextBatch()
{
    if (!bPrototypeRendering)
    {
        return;
    }
    // ISM生成中のプレイヤー移動を停止または復帰する（引数：true（成立・有効））。
    FreezePrototypePlayer(true);
    ++PrototypeTickCount;
    // TickStart（TickStart）を倍精度の実数として用意し、高精度な実時間を秒で取得するで初期化する。
    const double TickStart = FPlatformTime::Seconds();
    // InBudget（InBudget）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto InBudget = [&]()
    {
        // 高精度な実時間を秒で取得するからTickStartを引いた値と1000 <の積を呼び出し元へ返す。
        return (FPlatformTime::Seconds() - TickStart) * 1000 < FMath::Max(.5f, FrameBudgetMilliseconds);
    };
    // 追加したメッシュインスタンスの件数（Instances）に0を格納する。
	// 生成するActorの件数または一覧（Actors）に0を格納する。生成するデカールの件数または一覧（Decals）に0を格納する。
    int32 Instances = 0, Actors = 0, Decals = 0;
    // 「「「次に描画するグループの添字（GroupIndex）が共通設定ごとのISM描画グループ一覧（Groups）で、
	// 要素数を得るより小さい」かつ「追加したメッシュインスタンスの件数（Instances）が1フレームに追加する
	// メッシュの上限数（MaxInstancesPerFrame）より小さい」」かつ「InBudget()」」が成り立つ間、続く文を繰り返す。
    while (GroupIndex < Groups.Num() && Instances < MaxInstancesPerFrame && InBudget())
    {
        // 同じ設定でまとめる描画グループ（Group）を右辺から自動判定した型として用意し、
		// 共通設定ごとのISM描画グループ一覧（Groups）の「次に描画するグループの添字（GroupIndex）」に
		// 対応する要素で初期化する
        auto& Group = Groups[GroupIndex];
        // SettingsEntry（SettingsEntry）を右辺から自動判定した型として用意し、
		// 同じ設定でまとめる描画グループ（Group）の生成設定で初期化する
        const auto& SettingsEntry = Group.Settings;
        // 「「同じ設定でまとめる描画グループ（Group）の描画部品」が成立しない」に当てはまる場合だけ実行
        if (!Group.Component)
        {
            // 対象のコンポーネント（Component）を右辺から自動判定した型として用意し、
			// 指定型のUEオブジェクトを新規作成する（引数：this）で初期化する。
            auto* Component = NewObject<UInstancedStaticMeshComponent>(this);
            // 同じ設定でまとめる描画グループ（Group）の描画部品を、対象のコンポーネント（Component）に設定する。
            Group.Component = Component;
            // 対象のコンポーネント（Component）で、移動可能かなどのMobilityを設定する
            Component->SetMobility(EComponentMobility::Movable);
            // 対象のコンポーネント（Component）で、親部品との取り付け関係を設定する
            Component->SetupAttachment(RootComponent);
            // 対象のコンポーネント（Component）で、表示・衝突に使う静的メッシュを設定する
            Component->SetStaticMesh(SettingsEntry.Mesh);
            for (int32 Slot = 0; Slot < SettingsEntry.Materials.Num(); ++Slot)
            {
                // 対象のコンポーネント（Component）で、指定スロットへマテリアルを設定する
                Component->SetMaterial(Slot, SettingsEntry.Materials[Slot]);
            }
            // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
            Component->SetCollisionProfileName(SettingsEntry.CollisionProfile);
            Component->SetCollisionEnabled(SettingsEntry.CollisionEnabled);
            Component->SetCollisionObjectType(SettingsEntry.ObjectType);
            Component->SetCollisionResponseToChannels(SettingsEntry.Responses);
            Component->SetCastShadow(SettingsEntry.bCastShadow);
            Component->SetReceivesDecals(SettingsEntry.bReceivesDecals);
            Component->SetVisibility(SettingsEntry.bVisible);
            Component->SetHiddenInGame(SettingsEntry.bHiddenInGame);
            Component->SetRenderCustomDepth(SettingsEntry.bCustomDepth);
            Component->SetCustomDepthStencilValue(SettingsEntry.Stencil);
            // 対象のコンポーネント（Component）のこの部品でNaniteを使わない設定を、SettingsEntryのこの部品でNaniteを使わない設定に設定する。
            Component->bDisallowNanite = SettingsEntry.bDisallowNanite;
            // 対象のコンポーネント（Component）の距離場ライティングへ影響する設定を、SettingsEntryの距離場ライティングへ影響する設定に設定する。
            Component->bAffectDistanceFieldLighting = SettingsEntry.bAffectDistanceFieldLighting;
            // 対象のコンポーネント（Component）のレイトレーシングで表示する設定を、SettingsEntryのレイトレーシングで表示する設定に設定する。
            Component->bVisibleInRayTracing = SettingsEntry.bVisibleInRayTracing;
            // 対象のコンポーネント（Component）の非表示でも影を落とす設定を、SettingsEntryの非表示でも影を落とす設定に設定する。
            Component->bCastHiddenShadow = SettingsEntry.bCastHiddenShadow;
            // 対象のコンポーネント（Component）で、半透明描画の優先順位を設定する（引数：SettingsEntryの半透明描画の優先順位）。
            Component->SetTranslucentSortPriority(SettingsEntry.TranslucencySortPriority);
            // 対象のコンポーネント（Component）のこの部品に影響するライトのチャンネルを、SettingsEntryのこの部品に影響するライトのチャンネルに設定する。
            Component->LightingChannels = SettingsEntry.LightingChannels;
            // 対象のコンポーネント（Component）で、NavMeshの計算に影響するか設定する（引数：SettingsEntryのNavMesh計算へ影響する設定）。
            Component->SetCanEverAffectNavigation(SettingsEntry.bNavigation);
            // 対象のコンポーネント（Component）で、作成した部品をワールドへ登録し表示・衝突を有効にする。
            Component->RegisterComponent();
            ++PrototypeISMComponents;
        }
        // 対象の件数（Count）を符号付き32ビット整数として用意し、で初期化する。
        const int32 Count =
            // 1回の追加を最大64個・フレーム内の残り予算・グループの未追加件数のうち最小の件数に制限する。
            FMath::Min3(64, MaxInstancesPerFrame - Instances, Group.Transforms.Num() - Group.Added);
        // Batch（Batch）を配列として宣言する。
        TArray<FTransform> Batch;
        // Batchで、一覧の要素をまとめて追加する（引数：同じ設定でまとめる描画グループ（Group）の
		// 配置姿勢一覧で、配列の先頭要素を指すポインターを取得すると同じ設定でまとめる描画グループ（Group）の
		// 追加済み件数の和、対象の件数（Count）
        Batch.Append(Group.Transforms.GetData() + Group.Added, Count);
        // 同じ設定でまとめる描画グループの描画部品で、
		// 複数の配置姿勢をまとめてISMへ追加する
        Group.Component->AddInstances(Batch, false, true, true);
        // 同じ設定でまとめる描画グループの追加済み件数へ、対象の件数を加算する。
        Group.Added += Count;
        // 追加したメッシュインスタンスの件数へ、対象の件数を加算する。
        Instances += Count;
        // 「同じ設定でまとめる描画グループ（Group）の追加済み件数が同じ設定でまとめる描画グループ
		// の配置姿勢一覧で、要素数を得ると等しい」に当てはまる場合だけ、続く文を実行する。
        if (Group.Added == Group.Transforms.Num())
        {
            ++GroupIndex;
        }
    }
    // 「「次に描画するグループの添字（GroupIndex）が共通設定ごとのISM描画グループ一覧（Groups）で、
	// 要素数を得ると等しい」かつ「次に生成するActorの添字（ActorIndex）がActorの生成待ち一覧（ActorQueue）で、
	// 要素数を得るより小さい」」、かつ次行の条件が成り立つ間、続く文を繰り返す。
    while (GroupIndex == Groups.Num() && ActorIndex < ActorQueue.Num() &&
           // この行では「生成するActorの件数または一覧（Actors）が1フレームに生成する小物Actorの
		   // 上限数（MaxActorsPerFrame）より小さい」かつ「InBudget()」を指定し、前後の行の計算・呼び出しへつなげる
           Actors < MaxActorsPerFrame && InBudget())
    {
        // 今回の配置データまたは対応表の1件（Entry）を右辺から自動判定した型として用意し、
		// Actorの生成待ち一覧（ActorQueue）の「ActorIndex++」に対応する要素で初期化する
        const auto& Entry = ActorQueue[ActorIndex++];
        ++Actors;
        // 「「今回の配置データまたは対応表の1件（Entry）の生成する小物Actorのクラス」が成立しない」
		// に当てはまる場合だけ、続く文を実行する。
        if (!Entry.ActorClass)
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            PrototypeFail(TEXT("Missing behavioural Blueprint copy"));
            return;
        }
        // 対象のActor（Actor）を右辺から自動判定した型として用意し、
		// 所属するワールドを取得するのSpawnActorDeferred<AActor>(Entry.ActorClassで初期化する。
        auto* Actor = GetWorld()->SpawnActorDeferred<AActor>(Entry.ActorClass,
            // この行では今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を指定し、
			// 前後の行の計算・呼び出しへつなげる。
            Entry.LocalTransform,
            // 生成する小物Actorの所有者として、この迷路生成Actorを渡す。
            this,
            // この行ではnullptr（参照先なし）を指定し、前後の行の計算・呼び出しへつなげる。
            nullptr,
            // 配置位置の重なりによる自動中止は行わず、小物Actorを生成する設定を指定する。
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Actor)
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            PrototypeFail(TEXT("Behavioural actor spawn failed"));
            return;
        }
        // 保存された文字列の値を、対応するActorや部品のプロパティへ設定する
        ApplyProperties(Actor, Entry.Properties);
        // 対象のActor（Actor）で、事前に参照を設定したActorの生成とConstructionを完了する
        Actor->FinishSpawning(Entry.LocalTransform);
        for (const auto& Override : Entry.Components)
        {
            // Actorが持つコンポーネント一覧（Components）をTInlineComponentArray<UActorComponent*>として宣言する
            TInlineComponentArray<UActorComponent*> Components;
            // 対象のActor（Actor）で、Actorの部品一覧を取得する（引数：Actorが持つコンポーネント一覧（Components））
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                // 「対象のコンポーネント（Component）で、比較用の名前を取得するがOverrideの元の部品名と等しい」に
				// 当てはまる場合だけ実行
                if (Component->GetFName() == Override.ComponentName)
                {
                    // 保存された文字列の値を、対応するActorや部品のプロパティへ設定する
                    ApplyProperties(Component, Override.Properties);
                    // 対象のコンポーネント（Component）で、描画設定の変更をレンダラーへ反映するよう通知する。
                    Component->MarkRenderStateDirty();
                }
            }
        }
        // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
        Actor->SetActorHiddenInGame(Entry.bHidden);
        Actor->SetActorEnableCollision(Entry.bCollision);
        // 生成した小物Actorの管理一覧（PrototypeActors）で、要素または対応関係を末尾へ追加する
        PrototypeActors.Add(Actor);
        ++PrototypeActorCount;
    }
    // 「「次に描画するグループの添字（GroupIndex）が共通設定ごとのISM描画グループ一覧（Groups）で、
	// 要素数を得ると等しい」かつ「次に生成するActorの添字（ActorIndex）がActorの生成待ち一覧（ActorQueue）で、
	// 要素数を得ると等しい」」、かつ次行の条件が成り立つ間、続く文を繰り返す
    while (GroupIndex == Groups.Num() && ActorIndex == ActorQueue.Num() &&
           // この行では「「次に生成するデカールの添字（DecalIndex）がデカールの生成待ち一覧（DecalQueue）で、
		   // 要素数を得るより小さい」かつ「生成するデカールの件数または一覧（Decals）が
		   // 1フレームに生成するデカールの上限数（MaxDecalsPerFrame）より小さい」」かつ「InBudget()」を指定し、
		   // 前後の行の計算・呼び出しへつなげる
           DecalIndex < DecalQueue.Num() && Decals < MaxDecalsPerFrame && InBudget())
    {
        // 今回の配置データまたは対応表の1件（Entry）を右辺から自動判定した型として用意し、
		// デカールの生成待ち一覧（DecalQueue）の「DecalIndex++」に対応する要素で初期化する
        const auto& Entry = DecalQueue[DecalIndex++];
        ++Decals;
        // 対象のコンポーネント（Component）を右辺から自動判定した型として用意し、指定型のUEオブジェクトを新規作成する
        auto* Component = NewObject<UDecalComponent>(this);
        // 対象のコンポーネント（Component）で、移動可能かなどのMobilityを設定する
        Component->SetMobility(EComponentMobility::Movable);
        // 対象のコンポーネント（Component）で、親部品との取り付け関係を設定する
        Component->SetupAttachment(RootComponent);
        // 対象のコンポーネント（Component）で、デカール用マテリアルを設定する
        Component->SetDecalMaterial(Entry.Material);
        // 対象のコンポーネント（Component）のDecalSizeを、今回の配置データまたは対応表の1件（Entry）の大きさに設定する。
        Component->DecalSize = Entry.Size;
        // 保存・計算した設定を、以下の呼び出しで同じ部品へまとめて反映する。
        Component->SetSortOrder(Entry.SortOrder);
        Component->SetFadeScreenSize(Entry.FadeScreenSize);
        Component->SetVisibility(Entry.bVisible);
        Component->SetHiddenInGame(Entry.bHiddenInGame);
        Component->SetWorldTransform(Entry.LocalTransform);
        // 対象のコンポーネント（Component）で、作成した部品をワールドへ登録し表示・衝突を有効にする。
        Component->RegisterComponent();
        // 生成したデカールの管理一覧（PrototypeDecals）で、要素または対応関係を末尾へ追加する
        PrototypeDecals.Add(Component);
        ++PrototypeDecalCount;
    }
    // 「「次に描画するグループの添字（GroupIndex）が共通設定ごとのISM描画グループ一覧（Groups）で、
	// 要素数を得ると等しい」かつ「次に生成するActorの添字（ActorIndex）がActorの生成待ち一覧（ActorQueue）で、
	// 要素数を得ると等しい」」、かつ次行の条件に当てはまる場合だけ、続く文を実行する。
    if (GroupIndex == Groups.Num() && ActorIndex == ActorQueue.Num() &&
        // この行では次に生成するデカールの添字（DecalIndex）がデカールの生成待ち一覧（DecalQueue）で、
		// 要素数を得ると等しいを指定し、前後の行の計算・呼び出しへつなげる。
        DecalIndex == DecalQueue.Num())
    {
        // 「「実際の床支持と衝突で全床へ到達できるか検査する」が成立しない」に当てはまる場合だけ実行
        if (!ValidatePhysicalLayout())
        {
            return;
        }
        bPrototypeRendering = false;
        bGenerationComplete = true;
        // ISM生成中のプレイヤー移動を停止または復帰する
        FreezePrototypePlayer(false);
        // ISM描画にかかった秒数（PrototypeRenderSeconds）を、
		// 高精度な実時間を秒で取得するからISM描画を開始した実時間（秒）（BuildStarted）を引いた値に設定する。
        PrototypeRenderSeconds = FPlatformTime::Seconds() - BuildStarted;
        // 描画作業1回の最大処理時間（ms）（PeakGenerationTickMilliseconds）を、FMath::Max(に設定する。
        PeakGenerationTickMilliseconds = FMath::Max(
            // PeakGenerationTickMilliseconds, (FPlatformTime::Seconds() - TickStart)と1000の積。
            PeakGenerationTickMilliseconds, (FPlatformTime::Seconds() - TickStart) * 1000);
        // ログへ計測結果・警告・失敗理由を記録する。引数の値を指定書式へ埋め込む。
        UE_LOG(LogTemp,
            // 生成時間、配置件数、到達件数を、通常表示レベルのログへ出す。
            Display,
            // ログの前半の書式を指定する。Seed・生成秒数・準備ms・最大作業ms・メッシュ件数の値を埋め込む。
            TEXT("COURTYARD_ISM_READY seed=%d seconds=%.3f prep_ms=%.2f peak_ms=%.2f instances=%d "
                 // ログの後半の書式を指定する。ISMグループ・Actor・デカール・到達床/全床の件数を埋め込む。
                 "groups=%d actors=%d decals=%d reachable=%d/%d"),
            // この行では今回実際に使ったSeed（ActualSeed）を指定し、前後の行の計算・呼び出しへつなげる。
            ActualSeed,
            // この行ではISM描画にかかった秒数を指定し、前後の行の計算・呼び出しへつなげる。
            PrototypeRenderSeconds,
            // この行では描画開始前の準備時間（ms）を指定し、前後の行の計算・呼び出しへつなげる。
            PrototypePreparationMilliseconds,
            // この行では描画作業1回の最大処理時間（ms）を指定し、前後の行の計算・呼び出しへつなげる。
            PeakGenerationTickMilliseconds,
            // ログへ出すメッシュインスタンス総件数を渡す。
            PrototypeInstances,
            // ログへ出すISM部品の総件数を渡す。同じメッシュを何グループにまとめたかが分かる。
            PrototypeISMComponents,
            // ログへ出す生成済み小物Actorの総件数を渡す。
            PrototypeActorCount,
            // ログへ出す生成済みデカールの総件数を渡す。
            PrototypeDecalCount,
            // この行では実際の衝突形状で到達できた床の件数を指定し、前後の行の計算・呼び出しへつなげる。
            PhysicallyReachableCells,
            // 生成結果（Plan）のセル列で、要素数を得る。
            Plan.Cells.Num());
        // 迷路の生成完了をBPへ通知する。
        OnCourtyardReady();
        return;
    }
    // 描画作業1回の最大処理時間（ms）を、に設定する。
    PeakGenerationTickMilliseconds =
        // 大きい方の値を選ぶ（引数：描画作業1回の最大処理時間（ms）、
		// 高精度な実時間を秒で取得するからTickStartを引いた値と1000の積）。
        FMath::Max(PeakGenerationTickMilliseconds, (FPlatformTime::Seconds() - TickStart) * 1000);
    // ISM描画の継続予約（BuildTimer）を、に設定する。
    BuildTimer =
        // ワールドのタイマー管理機能を取得するで、次フレームに処理を継続するよう予約する
        GetWorldTimerManager().SetTimerForNextTick(this, &ACourtyardISMGenerator::BuildNextBatch);
}

/** 床セルの隣接関係から外壁・出入口・直線トンネル・屋内壁の配置を組み立てる。 */
void ACourtyardISMGenerator::PlanModules()
{
    // BPによる抽選は残し、壁・入口へ渡すIDだけを実際につながった空間単位へ統一する。
    const FCourtyardWallRegions Regions = bSeparateWallStylesWithTunnels
        ? CourtyardWallRegions::Build(Plan, EntranceCells) : FCourtyardWallRegions();
    WallStyleOwners = Regions.StyleOwnerByCell;
    WallStyleRegionCount = Regions.RegionCount;
    WallStyleAddedTunnelCount = Regions.AddedTunnels.Num();
    const auto StyleOwner = [&](FIntPoint Position, int32 OriginalNodeId)
    {
        const int32* Owner = WallStyleOwners.Find(Position);
        return Owner ? *Owner : OriginalNodeId;
    };

    ModulePlacements.Reset();
    // 床セルの一覧または索引（Cells）をキーで値を引く表として宣言する。
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
                         // 直線トンネルに使う部品レベル（Passage）を真偽値として用意し、falseで初期化する。
                         bool Passage = false,
                         // HideStart（HideStart）を真偽値として用意し、falseで初期化する。
                         bool HideStart = false,
                         // HideEnd（HideEnd）を真偽値として用意し、falseで初期化する。
                         bool HideEnd = false,
                         FTransform LocalAdjustment = FTransform::Identity)
    {
        // P（P）を配置するレベル部品と姿勢として宣言する。
        FCourtyardModulePlacement P;
        // Pの各項目を、以下で計算・取得した値へまとめて設定する。
        P.Level = Level;
        P.Transform = FTransform(FRotator(0, Yaw, 0), Local, FVector(Scale)) * GetActorTransform();
        P.bPassage = Passage;
        P.bHideStartArch = HideStart;
        P.bHideEndArch = HideEnd;
        P.LocalAdjustment = LocalAdjustment;
        // 配置する部品と姿勢の一覧（ModulePlacements）で、要素または対応関係を末尾へ追加する
        ModulePlacements.Add(P);
    };
    // 外壁のローカル範囲はX=0、Y=-400～0。建物側が+X、道側が-Xになるよう回す。
    // Boundary（Boundary）を、周囲の値を使って判定・計算する小さな関数として定義する。
    auto Boundary = [&](FIntPoint Cell, int32 Direction, TSoftObjectPtr<UWorld> Level)
    {
        if (IsValid(EntranceArea) && Direction == EntranceOutwardDirection &&
            // この行では壁を開ける入口セルの集合（EntranceCells）で、
			// 指定値が登録されているか調べる（引数：Cell）を指定し、前後の行の計算・呼び出しへつなげる。
            EntranceCells.Contains(Cell))
        {
            return;
        }
        if (!IsValid(EntranceArea) && IsValid(IntroCorridor) &&
            // この行では従来の導入通路Actor（IntroCorridor）で、
			// この壁境界が導入通路の開口か判定する（引数：Cell、四方向の番号（Direction））を指定し、
			// 前後の行の計算・呼び出しへつなげる。
            IntroCorridor->IsEntranceBoundary(Cell, Direction))
        {
            return;
        }
        // 「壁部品が自作導入エリアへ重なるか調べる
        const FTransform LocalAdjustment = BoundaryOverlapsEntrance(Cell, Direction, Level)
            ? FitEntranceModule(Level) : FTransform::Identity;
        // 水平回転（Yaw）を単精度の実数として用意し、四方向の番号（Direction）と90.fの積で初期化する。
        const float Yaw = Direction * 90.f;
        // 中央位置（Center）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
        const FVector Center(Cell.X * CellSize + CourtyardRenderSteps[Direction].X * CellSize * .5f,
            // この行ではCellのY成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの
			// 「四方向の番号（Direction）」に対応する要素のY成分と1セルの実寸（cm）（CellSize）
			// の積との5fの積の和を指定し、前後の行の計算・呼び出しへつなげる。
            Cell.Y * CellSize + CourtyardRenderSteps[Direction].Y * CellSize * .5f,
            // 境界に置く部品のローカルZ座標を0にし、床面を基準にする。
            0);
        // 選んだ壁や出入口の部品を、補正した位置と回転で配置予定一覧へ追加する。
		// Level, Centerから各軸の回転角を組み立てる（引数：0、水平回転（Yaw）、0）
		// のRotateVector(FVector(0, -CellSize * .5f, 0)), Yawを引いた値。
        AddModule(Level, Center - FRotator(0, Yaw, 0).RotateVector(FVector(0, -CellSize * .5f, 0)),
            Yaw, false, false, false, LocalAdjustment);
    };
    for (const auto& C : Plan.Cells)
    {
        // 「Cの床の種別がECourtyardCellKind::Tunnelと等しい」に当てはまる場合だけ実行
        const FCourtyardLink* TunnelLink = C.Kind == ECourtyardCellKind::Tunnel
            ? &Plan.Links[C.LinkId] : Regions.AddedTunnels.Find(C.Position);
        if (TunnelLink)
        {
            // Link（Link）を右辺から自動判定した型として用意し、生成結果（Plan）のLinksの
			// 「Cの接続ID」に対応する要素で初期化する
            const auto& Link = *TunnelLink;
            // First（First）にCのセル座標がLinkのセル列の「0」に対応する要素と等しいを格納する。
			// Last（Last）にCのセル座標がLinkのセル列で、末尾の要素を取得すると等しいを格納する。
            const bool First = C.Position == Link.Cells[0], Last = C.Position == Link.Cells.Last();
            // RoadStart（RoadStart）を真偽値として用意し、で初期化する。
            const bool RoadStart =
                // トンネルの先頭セルから進行方向と逆へ1セル戻り、接続元の床種別を取得する。
                Cells.FindChecked(Link.Cells[0] - CourtyardRenderSteps[Link.Direction])->Kind ==
                // 接続元が屋外床なら、屋外同士のつながりとしてアーチの扱いを判断する。
                ECourtyardCellKind::Street;
            // RoadEnd（RoadEnd）を真偽値として用意し、で初期化する。
            const bool RoadEnd =
                // トンネルの末尾セルから進行方向へ1セル進み、接続先の床種別を取得する。
                Cells.FindChecked(Link.Cells.Last() + CourtyardRenderSteps[Link.Direction])->Kind ==
                // 接続先が屋外床なら、屋外同士のつながりとしてアーチの扱いを判断する。
                ECourtyardCellKind::Street;
            // 四方向の番号（Direction）を符号付き32ビット整数として用意し、Linkの方向番号で初期化する。
            const int32 Direction = Link.Direction;
            // 水平回転（Yaw）を単精度の実数として用意し、四方向の番号（Direction）と90.fの積で初期化する。
            const float Yaw = Direction * 90.f;
            // 中央位置（Center）を、括弧内の値を使って3次元ベクトルとして初期化する。
            const FVector Center(C.Position.X * CellSize, C.Position.Y * CellSize, 0);
            // テーマで指定された直線トンネル用の部品を、配置予定一覧へ追加する。
            AddModule(Theme->Passage,
                // トンネルの中央位置から、元の部品レベルの原点に合わせる分の補正を引く。
                Center -
                // この行では各軸の回転角を組み立てる（引数：0、水平回転（Yaw）、0）で、指定ベクトルを回転させる
                FRotator(0, Yaw, 0).RotateVector(FVector(CellSize * .5f, -CellSize * .5f, 0)),
                // この行では水平回転（Yaw）を指定し、前後の行の計算・呼び出しへつなげる。
                Yaw,
                // この行ではtrueを指定し、前後の行の計算・呼び出しへつなげる。
                true,
                // この行では「「First」が成立しない」または「RoadStart」を指定し、前後の行の計算・呼び出しへつなげる。
                !First || RoadStart,	// 「Last」かつ「RoadEnd」。
                Last && RoadEnd);
            // 「「First」かつ「RoadStart」」に当てはまる場合だけ、続く文を実行する。
            if (First && RoadStart)
            {
				// トンネル手前側に接している部屋の床セル
				const FIntPoint StartCell = Link.Cells[0] - CourtyardRenderSteps[Direction];

				const FCourtyardCell* StartFloor = Cells.FindChecked(StartCell);

				// 従来の入り口候補を選び、部屋タイプに合わせてBPで置き換える
				const auto DefaultPortal = Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)];

				const auto SelectedPortal = SelectPortalForNode(StyleOwner(StartCell, StartFloor->NodeId), DefaultPortal);

				Boundary(StartCell, Direction, SelectedPortal);
            }
            // 「「Last」かつ「RoadEnd」」に当てはまる場合だけ、続く文を実行する。
            if (Last && RoadEnd)
            {
				// トンネル奥川に接している部屋の床セル
				const FIntPoint EndCell = Link.Cells.Last() + CourtyardRenderSteps[Direction];

				const FCourtyardCell* EndFloor = Cells.FindChecked(EndCell);

				// 奥川の部屋タイプで入り口を選ぶ
				const auto DefaultPortal = Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)];

				const auto SelectedPortal = SelectPortalForNode(StyleOwner(EndCell, EndFloor->NodeId), DefaultPortal);

				// 0～3の方向番号を逆にする
				Boundary(EndCell, (Direction + 2) % 4, SelectedPortal);
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
                    // 現在セルが屋内部屋の床かどうかで、外周に使う壁部品を選ぶ。
                    C.Kind == ECourtyardCellKind::Room
                        // 屋内部屋なら、テーマの屋内用外壁・扉部品を選ぶ。
						// : ThemeのFacadesの「再現可能な乱数生成器（Random）で、
						// 指定範囲の整数をSeedに従って抽選する
                        ? Theme->RoomFacade : Theme->Facades[Random.RandRange(0, Theme->Facades.Num() - 1)];
                // 部屋単位の壁タイプをBPで反映してから配置する
				const auto SelectedFacade = SelectFacadeForNode(StyleOwner(C.Position, C.NodeId), Level);
				Boundary(C.Position, D, SelectedFacade);

				// このセルのこの方向の境界へ、選んだレベル部品または必要な薄い壁を計画する。
                //Boundary(C.Position, D, Level);
            }
        }
    }
    // Box（Box）を、周囲の値を使って判定・計算する小さな関数として定義する。
    const auto Box = [&](FVector Center, FVector Size)
    {
        // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、
		// 壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュで、メッシュの外接範囲を取得するで初期化する。
        const auto Bounds = Theme->FloorMesh->GetBounds();
        // MeshScale（MeshScale）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
        FVector MeshScale(Size.X / (Bounds.BoxExtent.X * 2),
            // この行では範囲の大きさ（Size）のY成分をメッシュなどの外接範囲（Bounds）
			// の外接Boxの半サイズのY成分と2の積で割った値を指定し、前後の行の計算・呼び出しへつなげる。
            Size.Y / (Bounds.BoxExtent.Y * 2),
            // 範囲の大きさ（Size）のZ成分をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズのZ成分と2の積で割った値。
            Size.Z / (Bounds.BoxExtent.Z * 2));
        // 今回の配置データまたは対応表の1件（Entry）をISMのメッシュ・材質・衝突・配置設定として宣言する。
        FCourtyardISMMesh Entry;
        // 今回の配置データまたは対応表の1件（Entry）のメッシュ参照を、
		// 壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュに設定する。
        Entry.Mesh = Theme->FloorMesh;
        // 今回の配置データまたは対応表の1件（Entry）のマテリアル一覧で、
		// 要素または対応関係を末尾へ追加する（引数：壁・床・通路の見た目の参照データ（Theme）の天井の材質）。
        Entry.Materials.Add(Theme->CeilingMaterial);
        // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を、に設定する。
        Entry.LocalTransform =
            // 指定寸法に合わせた拡大率とメッシュ原点補正で姿勢を作り、次行の迷路Actor姿勢と合成する。
            FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * MeshScale, MeshScale) *
            // Actorの位置・回転・拡大率を取得する。
            GetActorTransform();
        // 同じ描画・衝突設定のメッシュをISMグループへまとめる（引数：今回の配置データまたは対応表の1件（Entry））。
        QueueMesh(Entry);
    };
    // 高さ（Height）に350.fと拡大率（Scale）の積を格納する。DoorHeight（DoorHeight）に
	// 260.fと拡大率（Scale）の積を格納する。Opening（Opening）に180.fと拡大率（Scale）の積を格納する。
    const float Height = 350.f * Scale, DoorHeight = 260.f * Scale, Opening = 180.f * Scale, Thickness = 16.f * Scale;
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
                    // P（P）にRの最小端と2次元の整数セル座標を組み立てる（引数：X、Y）の和を格納する。
					// Q（Q）にPとCourtyardRenderStepsの「D」に対応する要素の和を格納する。
                    const auto P = R.Min + FIntPoint(X, Y), Q = P + CourtyardRenderSteps[D];
                    // 「「対象の部屋（Room）の実床セル一覧で、指定値が登録されているか調べる（引数：P）」が成立しない」
					// に当てはまる場合だけ実行
                    if (!Room.FloorCells.Contains(P))
                    {
                        continue;
                    }
                    // 「対象の部屋（Room）の実床セル一覧で、指定値が登録されているか調べる（引数：Q）」
					// に当てはまる場合だけ実行
                    if (Room.FloorCells.Contains(Q))
                    {
                        continue;
                    }
                    bool Door = false;
                    for (int32 I = 0; I < Room.DoorCells.Num(); ++I)
                    {
                        // Doorへ、「対象の部屋（Room）の扉を開ける室内セルの「I」に対応する要素がPと等しい」
						// かつ「対象の部屋（Room）の各扉の外向き方向の「I」に対応する要素がDと等しい」
						// とのビットORで該当するビットを残す。
                        Door |= Room.DoorCells[I] == P && Room.DoorDirections[I] == D;
                    }
                    // 中央位置（Center）を3次元ベクトルとして作る。括弧内に並べる値を初期値として使う。
                    const FVector Center(
                        // この行ではPのX成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの
						// 「D」に対応する要素のX成分と1セルの実寸（cm）（CellSize）の積との5fの積の和を指定し、
						// 前後の行の計算・呼び出しへつなげる。
                        P.X * CellSize + CourtyardRenderSteps[D].X * CellSize * .5f,
                        // この行ではPのY成分と1セルの実寸（cm）（CellSize）の積とCourtyardRenderStepsの
						// 「D」に対応する要素のY成分と1セルの実寸（cm）（CellSize）の積との5fの積の和を指定し、
						// 前後の行の計算・呼び出しへつなげる。
                        P.Y * CellSize + CourtyardRenderSteps[D].Y * CellSize * .5f,
                        // 屋内部屋の壁の基準中央を、まず床面のローカルZ=0で求める。
                        0);
                    // 入口の幅方向（Tangent）を、括弧内の値を使って3次元ベクトルとして初期化する。
                    const FVector Tangent(-CourtyardRenderSteps[D].Y, CourtyardRenderSteps[D].X, 0);
                    // Wall（Wall）を、周囲の値を使って判定・計算する小さな関数として定義する。
                    const auto Wall = [&](float Offset, float Length, float Z, float H)
                    {
                        // 範囲の大きさ（Size）を3次元ベクトルとして用意し、
						// Dを2で割った余りが0 ? FVector(Thickness, Length, H)と等しいで初期化する。
						// この関数の役割：3次元の位置・方向・寸法を組み立てる。ここでは呼び出し方を宣言する。
                        FVector Size = D % 2 == 0 ? FVector(Thickness, Length, H) : FVector(Length, Thickness, H);
                        // 壁の横方向のずれと中心高さを基準中央へ足し、指定寸法のBox状メッシュを配置する。
                        Box(Center + Tangent * Offset + FVector(0, 0, Z), Size);
                    };
                    if (!Door)
                    {
                        // 開口がない境界では、幅1セル・指定された壁高の全面壁を作る。
                        Wall(0, CellSize, Height * .5f, Height);
                    }
                    // 直前の条件に当てはまらなかった場合の処理へ進む。
                    else
                    {
                        // 横方向または盤面の一辺（Side）を単精度の実数として用意し、
						// 1セルの実寸（cm）（CellSize）からOpeningを引いた値との5fの積で初期化する。
                        const float Side = (CellSize - Opening) * .5f;
                        // 開口の一方の端に、余った幅の半分をふさぐ扉脇の壁を作る。
                        Wall(-(Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
                        // 反対側の端にも同じ幅の扉脇の壁を作り、中央に開口を残す。
                        Wall((Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
                        // 扉の上から壁の上端までをふさぐ、上側の壁を作る。
                        Wall(0, CellSize, (Height + DoorHeight) * .5f, Height - DoorHeight);
                    }
                }
            }
        }
    }
}

/** プレイヤーと同寸法のカプセルで全床への通行を調べ、各床の支持も確認する。 */
bool ACourtyardISMGenerator::ValidatePhysicalLayout()
{
    // プレイヤーキャラクター（Player）を右辺から自動判定した型として用意し、
	// 指定プレイヤー番号のキャラクターを取得する（引数：this、0）で初期化する。
    auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
    if (!Player)
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する
        return PrototypeFail(TEXT("Player character missing"));
    }
    // Half（Half）を単精度の実数として用意し、プレイヤーキャラクター（Player）で、
	// キャラクターの衝突カプセルを取得するで、拡大率適用後のカプセル半高を取得するで初期化する。
    const float Half = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    // Radius（Radius）を単精度の実数として用意し、プレイヤーキャラクター（Player）で、
	// キャラクターの衝突カプセルを取得するで、拡大率適用後のカプセル半径を取得すると3の和で初期化する。
    const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 3;
    // プレイヤーのカプセル形状（Capsule）をFCollisionShapeとして用意し、
	// プレイヤー相当のカプセル形状を作る（引数：Radius、Half）で初期化する。
    const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, Half);
    // 判定する衝突オブジェクト種別をFCollisionObjectQueryParamsとして宣言する。
    FCollisionObjectQueryParams Objects;
    // 判定する衝突オブジェクト種別で、衝突検査に含めるオブジェクト種別を追加する
    Objects.AddObjectTypesToQuery(ECC_WorldStatic);
    // 判定する衝突オブジェクト種別で、衝突検査に含めるオブジェクト種別を追加する
    Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
    // 衝突判定の条件（Query）を、括弧内の値を使って衝突問い合わせの設定として初期化する。
    FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardWalkability), false);
    for (TActorIterator<APawn> It(GetWorld()); It; ++It)
    {
        // 衝突判定の条件（Query）で、衝突検査から除外するActorを追加する（引数：イテレーターItが現在指しているActor）。
        Query.AddIgnoredActor(*It);
    }
    // 迷路側の床だけでなく、導入通路から中央・左右を通って入れることも確認する。
    if (IsValid(EntranceArea))
    {
        const FVector Forward = EntranceTransform.GetUnitAxis(EAxis::X);
        const FVector Right = EntranceTransform.GetUnitAxis(EAxis::Y);
        const FVector Origin = EntranceTransform.GetLocation() + FVector(0, 0, Half + 5);
        const float Lane = FMath::Min(65.f, ActualEntranceWidth * .2f);
        for (float Offset : {-Lane, 0.f, Lane})
        {
            FHitResult Hit;
            if (GetWorld()->SweepSingleByChannel(Hit,
                Origin - Forward * 160 + Right * Offset,
                Origin + Forward * 180 + Right * Offset,
                FQuat::Identity, ECC_Pawn, Capsule, Query))
            {
                const auto* MeshComponent = Cast<UStaticMeshComponent>(Hit.GetComponent());
                return PrototypeFail(FString::Printf(
                    TEXT("Introduction opening blocked lane=%.0f actor=%s component=%s mesh=%s instance=%d"),
                    Offset, *GetNameSafe(Hit.GetActor()), *GetNameSafe(Hit.GetComponent()),
                    MeshComponent ? *GetNameSafe(MeshComponent->GetStaticMesh()) : TEXT("none"), Hit.Item));
            }
        }
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
    // 「所属するワールドを取得するのOverlapBlockingTestByChannel(」に当てはまる場合だけ、続く文を実行する。
    if (GetWorld()->OverlapBlockingTestByChannel(
            // 開始セルのキャラクター中心に、回転なしのカプセルを置き、Pawnを阻む衝突がないか調べる。
            At(Plan.StartCell), FQuat::Identity, ECC_Pawn, Capsule, Query))
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する
        return PrototypeFail(TEXT("Player start is blocked"));
    }
    if (!IsValid(EntranceArea) && !IsValid(IntroCorridor))
    {
        // 導入通路を使わない開始では、プレイヤーを生成結果の開始セルへ移動する。
		// 開始セルの判定中心へ、移動経路のSweepなし・TeleportPhysicsで瞬間移動する。
        Player->SetActorLocation(At(Plan.StartCell), false, nullptr, ETeleportType::TeleportPhysics);
        // 「autoとController = Playerで、キャラクターを操作するControllerを取得するの積」
		// に当てはまる場合だけ実行
        if (auto* Controller = Player->GetController())
        {
            // プレイヤーの操作Controller（Controller）で、操作視点の向きを設定する
            Controller->SetControlRotation(GetActorRotation());
        }
    }
    // 探索済みの集合（Seen）を重複しない集合として作り、波括弧内の値で初期化する
    TSet<FIntPoint> Seen{Plan.StartCell};
    // 幅優先探索の待ち行列（Queue）を配列として作り、波括弧内の値で初期化する
    TArray<FIntPoint> Queue{Plan.StartCell};
    for (int32 I = 0; I < Queue.Num(); ++I)
    {
        for (auto D : CourtyardRenderSteps)
        {
            // 次に処理する位置または添字（Next）を右辺から自動判定した型として用意し、
			// 幅優先探索の待ち行列（Queue）の「I」に対応する要素とDの和で初期化する。
            const auto Next = Queue[I] + D;
            // 「「「Floorsで、指定値が登録されているか調べる（引数：次に処理する位置または添字）」が成立しない」
			// または「探索済みの集合で、指定値が登録されているか調べる（引数：次に処理する位置または添字）」」
			// に当てはまる場合だけ実行
            if (!Floors.Contains(Next) || Seen.Contains(Next))
            {
                continue;
            }
            // 「「所属するワールドを取得するのSweepTestByChannel(」が成立しない」に当てはまる場合だけ実行
            if (!GetWorld()->SweepTestByChannel(
                    // 現在セルから隣接セルまでの間に、Pawnを阻むカプセル衝突がないか調べる。
                    At(Queue[I]), At(Next), FQuat::Identity, ECC_Pawn, Capsule, Query))
            {
                // 探索済みの集合（Seen）で、要素または対応関係を末尾へ追加する
                Seen.Add(Next);
                // 幅優先探索の待ち行列（Queue）で、要素または対応関係を末尾へ追加する
                Queue.Add(Next);
            }
        }
    }
    // PawnをBlockしない導入エリアのOverlap用Boxは、通行を遮断する壁として扱わない
    // 実際の衝突形状で到達できた床の件数を、探索済みの集合で、要素数を得るに設定する
    PhysicallyReachableCells = Seen.Num();
    // 「探索済みの集合（Seen）で、要素数を得るがFloorsで、要素数を得ると異なる」に当てはまる場合だけ実行
    if (Seen.Num() != Floors.Num())
    {
        // Detail（Detail）を文字列として宣言する
        FString Detail;
        for (const auto Missing : Floors)
        {
            // 「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：Missing）」
			// に当てはまる場合だけ、続く文を実行する
            if (Seen.Contains(Missing))
            {
                continue;
            }
            for (const auto Step : CourtyardRenderSteps)
            {
                // Neighbor（Neighbor）を2次元の整数セル座標として用意し、
				// Missingと隣接セルへ進む差分（Step）の和で初期化する
                const FIntPoint Neighbor = Missing + Step;
                // 「「探索済みの集合（Seen）で、指定値が登録されているか調べる（引数：Neighbor）」が成立しない」
				// に当てはまる場合だけ実行
                if (!Seen.Contains(Neighbor))
                {
                    continue;
                }
                // 衝突判定の結果（Hit）を衝突判定の結果として宣言する
                FHitResult Hit;
                // 到達できなかったセルに隣接する箇所へSweepを行い、通行を阻む部品の情報を得る
                GetWorld()->SweepSingleByChannel(
                    // 隣接する到達済みセルから未到達セルまで、同じカプセル形状・Pawnチャンネルで原因を調べる
                    Hit, At(Neighbor), At(Missing), FQuat::Identity, ECC_Pawn, Capsule, Query);
                // Detailを、FString::Printf(TEXT(" cell=%s from=%s actor=%s component=%s instance=%d"),に設定する
                Detail = FString::Printf(TEXT(" cell=%s from=%s actor=%s component=%s instance=%d"),
                    // この行ではMissingで、値を読みやすい文字列へ変換するをUEの文字配列ポインターとして
					// 取り出した値を指定し、前後の行の計算・呼び出しへつなげる
                    *Missing.ToString(),
                    // この行ではNeighborで、値を読みやすい文字列へ変換するをUEの文字配列ポインターとして
					// 取り出した値を指定し、前後の行の計算・呼び出しへつなげる
                    *Neighbor.ToString(),
                    // この行ではGetNameSafe(Hit.GetActor())をUEの文字配列ポインターとして取り出した値を指定し、
					// 前後の行の計算・呼び出しへつなげる
                    *GetNameSafe(Hit.GetActor()),
                    // この行ではGetNameSafe(Hit.GetComponent())をUEの文字配列ポインターとして取り出した値を指定し、
					// 前後の行の計算・呼び出しへつなげる
                    *GetNameSafe(Hit.GetComponent()),
                    // 衝突判定の結果（Hit）のItem
                    Hit.Item);
                break;
            }
            // 「「Detailで、空かを調べる」が成立しない」に当てはまる場合だけ実行
            if (!Detail.IsEmpty())
            {
                break;
            }
        }
        // PrototypeFail(を呼び出し元へ返す
        return PrototypeFail(
            // 到達できた床数・全床数と、衝突した部品の情報を含む生成失敗の説明を作る
            FString::Printf(TEXT("Module collision disconnects floor cells (%d/%d)%s"),
                // この行では探索済みの集合（Seen）で、要素数を得るを指定し、前後の行の計算・呼び出しへつなげる
                Seen.Num(),
                // この行ではFloorsで、要素数を得るを指定し、前後の行の計算・呼び出しへつなげる
                Floors.Num(),
                // DetailをUEの文字配列ポインターとして取り出した値
                *Detail));
    }
    for (auto P : Floors)
    {
        // 衝突判定の結果（Hit）を衝突判定の結果として宣言する
        FHitResult Hit;
        // 「「所属するワールドを取得するのLineTraceSingleByChannel(」が成立しない」
		// に当てはまる場合だけ、続く文を実行する。
        if (!GetWorld()->LineTraceSingleByChannel(
                // この関数の役割：セル座標と高さをワールド座標へ変換する。
				// ここから本体を定義する。
                Hit, GetCellWorldPosition(P, 60), GetCellWorldPosition(P, -60), ECC_Pawn, Query))
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
			// （引数："Floor support missing"というUE文字列）を呼び出し元へ返す。
            return PrototypeFail(TEXT("Floor support missing"));
        }
    }
    if (IsValid(EntranceArea) && bMoveEntranceAreaToMaze)
    {
        // 通路を移動した生成中は移動を停止し、検査後に通路内の開始位置へ配置する。
        // SpawnLocationを3次元ベクトルとして用意し、
		// 導入エリア内でのプレイヤー配置姿勢で、位置を取得するで初期化する。
        const FVector SpawnLocation = EntranceSpawnTransform.GetLocation();
        // 「所属するワールドを取得するのOverlapBlockingTestByChannel」に当てはまる場合だけ、続く文を実行する。
        if (GetWorld()->OverlapBlockingTestByChannel(
                // 導入通路内の開始地点にプレイヤーのカプセルを置き、通行を阻む衝突がないか検査する。
                SpawnLocation, FQuat::Identity, ECC_Pawn, Capsule, Query))
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            return PrototypeFail(TEXT("Introductory PlayerStart is inside blocking collision"));
        }
        // この行ではプレイヤーキャラクター（Player）のSetActorLocationAndRotationに
		// SpawnLocationを指定し、前後の行の計算・呼び出しへつなげる
        Player->SetActorLocationAndRotation(SpawnLocation,
            // この行では導入エリア内でのプレイヤー配置姿勢（EntranceSpawnTransform）で、
			// 姿勢の回転をオイラー角で取得するを指定し、前後の行の計算・呼び出しへつなげる
            EntranceSpawnTransform.Rotator(),
            // この行ではfalseを指定し、前後の行の計算・呼び出しへつなげる
            false,
            // この行ではnullptrを指定し、前後の行の計算・呼び出しへつなげる
            nullptr,
            // 開始地点へ移す際に不要な物理速度を与えないよう、TeleportPhysicsで姿勢を設定する。
            ETeleportType::TeleportPhysics);
        // 「autoとController = Playerで、キャラクターを操作するControllerを取得するの積」に当てはまる場合だけ実行
        if (auto* Controller = Player->GetController())
        {
            // プレイヤーの操作Controller（Controller）で、操作視点の向きを設定する
            Controller->SetControlRotation(EntranceSpawnTransform.Rotator());
        }
    }
    if (GoalClass)
    {
        // 「所属するワールドを取得するのOverlapAnyTestByObjectType(」に当てはまる場合だけ実行
        if (GetWorld()->OverlapAnyTestByObjectType(
                // ゴールの配置位置で、対象オブジェクト種別とのカプセルの重なりを検査する
                At(Plan.GoalCell), FQuat::Identity, Objects, Capsule, Query))
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            return PrototypeFail(TEXT("Goal position blocked"));
        }
        // Params（Params）をActor生成時の設定として宣言する
        FActorSpawnParameters Params;
        // Paramsの各項目を、以下で計算・取得した値へまとめて設定する
        Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        // 探索の到達先（Goal）を右辺から自動判定した型として用意し、所属するワールドを取得するの
		// SpawnActor<AActor>(で初期化する。参照先がない場合もあるポインターとして扱う
        auto* Goal = GetWorld()->SpawnActor<AActor>(
            // この関数の役割：セル座標と高さをワールド座標へ変換する。ここでは呼び出し方を宣言する。
            GoalClass, GetCellWorldPosition(Plan.GoalCell), GetActorRotation(), Params);
        if (!Goal)
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する
            return PrototypeFail(TEXT("Goal spawn failed"));
        }
        // 生成した小物Actorの管理一覧（PrototypeActors）で、要素または対応関係を末尾へ追加する
        PrototypeActors.Add(Goal);
    }
    return true;
}

/** 導入通路がない生成中は移動を一時停止し、生成後に元の移動モードへ戻す。 */
void ACourtyardISMGenerator::FreezePrototypePlayer(bool Frozen)
{
    // The player remains free to walk through the introduction while the maze builds.
    // 「「「移動を停止するかの指定（Frozen）」かつ「「オブジェクトが存在し、
	// 破棄待ちでもないかを調べる（引数：自作の導入エリアActor（EntranceArea））」が成立しない」」かつ
	// 「オブジェクトが存在し、破棄待ちでもないかを調べる（引数：従来の導入通路Actor（IntroCorridor））」」
	// に当てはまる場合だけ実行
    if (Frozen && !IsValid(EntranceArea) && IsValid(IntroCorridor))
    {
        return;
    }
    if (Frozen)
    {
        // 「PrototypeFrozenPlayerで、保持している参照先が有効か調べる」に当てはまる場合だけ実行
        if (PrototypeFrozenPlayer.IsValid())
        {
            return;
        }
        // 「autoとPlayer = UGameplayStatics::GetPlayerCharacter(this, 0)の積」に当てはまる場合だけ実行
        if (auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
        {
            // PrototypeFrozenPlayerを、プレイヤーキャラクター（Player）に設定する。
            PrototypeFrozenPlayer = Player;
            // PrototypeMovementModeを、0〜255の符号なし整数へ変換する
            PrototypeMovementMode = uint8(Player->GetCharacterMovement()->MovementMode);
            // プレイヤーキャラクター（Player）で、キャラクターの移動制御部品を取得するで、キャラクターの移動を停止する。
            Player->GetCharacterMovement()->DisableMovement();
        }
    }
    // 前の条件が不成立で、さらに「autoとPlayer = PrototypeFrozenPlayerで、
	// 保持している参照先を取得するの積」に当てはまる場合だけ、続く文を実行する。
    else if (auto* Player = PrototypeFrozenPlayer.Get())
    {
        // プレイヤーキャラクター（Player）で、キャラクターの移動制御部品を取得するで、
		// キャラクターの移動モードを設定する（引数：EMovementMode(PrototypeMovementMode)）。
        Player->GetCharacterMovement()->SetMovementMode(EMovementMode(PrototypeMovementMode));
        PrototypeFrozenPlayer.Reset();
    }
}

/** BPから呼ぶ新方式の入口。Seedを決め、生成結果をBPにも渡してISM描画を始める。 */
bool ACourtyardISMGenerator::GenerateOrganicCourtyard()
{
    // 今回描画する生成結果（NewPlan）を迷路全体の生成結果として宣言する
    FCourtyardPlan NewPlan;
    // 失敗理由（Error）を文字列として宣言する
    FString Error;
    // 配置用Seed（LayoutSeed）を符号付き32ビット整数として用意し、
	// 「再現用Seed（Seed）が0より小さい」なら今回のランダムSeed用の整数を取得する、
	// そうでなければ再現用Seed（Seed）で初期化する
    const int32 LayoutSeed = Seed < 0 ? FMath::Rand() : Seed;
    // 「「部屋の塊と枝分かれする細道の配置データを作る
	// （引数：生成設定（Settings）、配置用Seed（LayoutSeed）、
	// 今回描画する生成結果（NewPlan）、失敗理由（Error））」が成立しない」に当てはまる場合だけ実行
    if (!UCourtyardLayoutLibrary::BuildOrganicCourtyardPlan(Settings, LayoutSeed, NewPlan, Error))
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する
        return PrototypeFail(Error);
    }
    // 既存BPのゲーム処理が参照するResultPlanへ、今回の生成結果を渡す
    // 「autoとProperty = FindFProperty<FStructProperty>(GetClass(), TEXT("ResultPlan"))の積」に当てはまる場合だけ実行
    if (auto* Property = FindFProperty<FStructProperty>(GetClass(), TEXT("ResultPlan"));
        // この行では「Property」かつ「PropertyのStructがUEへ登録した構造体の型情報を取得すると等しい」を指定し、
		// 前後の行の計算・呼び出しへつなげる。
        Property && Property->Struct == FCourtyardPlan::StaticStruct())
    {
        // Propertyで、構造体などの値をまとめてコピーする（引数：Propertyで、
		// オブジェクト内のプロパティの格納位置を取得する（引数：this）、今回描画する生成結果（NewPlan）のアドレス）
        Property->CopyCompleteValue(Property->ContainerPtrToValuePtr<void>(this), &NewPlan);
    }
    // 配置データを受け取りISMによる描画を開始する（引数：今回描画する生成結果（NewPlan））を呼び出し元へ返す
    return RenderCourtyardISMPlan(NewPlan);
}
