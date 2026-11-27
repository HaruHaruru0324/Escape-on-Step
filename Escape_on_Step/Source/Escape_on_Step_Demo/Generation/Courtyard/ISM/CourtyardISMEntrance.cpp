#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"
#include "Generation/Courtyard/Layout/CourtyardEntranceLayout.h"
#include "Components/SceneComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"

/** 自作Actor内の出口を取得し、実際の開口幅に合う迷路の入口をランダムに選ぶ。 */
bool ACourtyardISMGenerator::PrepareCustomEntrance()
{
    // Dimensions（Dimensions）を3次元ベクトルとして用意し、導入通路の内幅・奥行・外幅を求めるで初期化する。
    const FVector Dimensions = GetBlueprintEntranceDimensions();
    // 「DimensionsのX成分が0以下」に当てはまる場合だけ、続く文を実行する。
    if (Dimensions.X <= 0)
    {
        // PrototypeFail(を呼び出し元へ返す。
        return PrototypeFail(
            // "Entrance Area needs MazeConnectionPoint or a MazeEntrance tagged component"というUE文字列。
            TEXT("Entrance Area needs MazeConnectionPoint or a MazeEntrance tagged component"));
    }
    // Depth（Depth）にDimensionsのY成分を格納する。OuterWidth（OuterWidth）にDimensionsのZ成分を格納する。
    const float Depth = Dimensions.Y, OuterWidth = Dimensions.Z;
    // Actorが持つコンポーネント一覧（Components）をTInlineComponentArray<USceneComponent*>として宣言する。参照先がない場合もあるポインターとして扱う。
    TInlineComponentArray<USceneComponent*> Components;
    // 自作の導入エリアActor（EntranceArea）で、Actorの部品一覧を取得する（引数：Actorが持つコンポーネント一覧（Components））。
    EntranceArea->GetComponents(Components);
    // Selection（Selection）を入口候補の情報として宣言する。
    FCourtyardEntranceSelection Selection;
    // 失敗理由（Error）を文字列として宣言する。
    FString Error;
    if (bBlueprintEntranceReady)
    {
        // BPが全候補の寸法・空間と到達性を検査済み。ここで再抽選しない。
        // Selectionの各項目を、以下で計算・取得した値へまとめて設定する。
        Selection.Cells = BlueprintEntrance.Cells;
        Selection.OutwardDirection = BlueprintEntrance.Direction;
        Selection.bInterior = BlueprintEntrance.bAlley;
        // 「「Selectionのセル列で、空かを調べる」または「Selectionの入口の外向き方向番号が0より小さい」」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (Selection.Cells.IsEmpty() || Selection.OutwardDirection < 0 ||
            // この行ではSelectionの入口の外向き方向番号が3より大きいを指定し、前後の行の計算・呼び出しへつなげる。
            Selection.OutwardDirection > 3)
        {
            // ISM生成を失敗状態にして移動制限と暗転を解除する（引数："Invalid Blueprint entrance selection"というUE文字列）を呼び出し元へ返す。
            return PrototypeFail(TEXT("Invalid Blueprint entrance selection"));
        }
        // const FIntPoint Directions[]を、{{1, 0}, {0, 1}, {-1, 0}, {0, -1}}に設定する。
        const FIntPoint Directions[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
        // 呼び出し元へ返す結果（Out）を右辺から自動判定した型として用意し、Directionsの「Selectionの入口の外向き方向番号」に対応する要素で初期化する。
        const auto Out = Directions[Selection.OutwardDirection];
        // 横方向または盤面の一辺（Side）を、括弧内の値を使って2次元の整数セル座標として初期化する。
        const FIntPoint Side(-Out.Y, Out.X);
        // Selectionの開口の境界中央を、FVector(に設定する。
        Selection.BoundaryCenter = FVector(
            // 入口セル列の先頭Xに横方向の半幅と外向き半セルを足し、境界中央のXをセル単位で求める。
            (Selection.Cells[0].X + Side.X * (Selection.Cells.Num() - 1) * .5f + Out.X * .5f) *
                // この行では1セルの実寸（cm）（CellSize）を指定し、前後の行の計算・呼び出しへつなげる。
                CellSize,
            // 同じ計算をY成分にも行い、開口の中央位置を求める。次行のCellSizeでcm単位へ変換する。
            (Selection.Cells[0].Y + Side.Y * (Selection.Cells.Num() - 1) * .5f + Out.Y * .5f) *
                // この行では1セルの実寸（cm）（CellSize）を指定し、前後の行の計算・呼び出しへつなげる。
                CellSize,
            // 開口境界のローカルZを0にし、床面を基準とする中央位置を完成させる。
            0);
    }
    // 前の条件が不成立で、さらに「「CourtyardEntranceLayout::SelectEntrance(Plan,」が成立しない」に当てはまる場合だけ、続く文を実行する。
    else if (!CourtyardEntranceLayout::SelectEntrance(Plan,
                 // この行では1セルの実寸（cm）（CellSize）を指定し、前後の行の計算・呼び出しへつなげる。
                 CellSize,
                 // この行では測定・採用した入口の実幅（ActualEntranceWidth）を指定し、前後の行の計算・呼び出しへつなげる。
                 ActualEntranceWidth,
                 // 導入通路を置くのに必要な奥行き（cm）を、入口候補の空間検査へ渡す。
                 Depth,
                 // この行では生成結果（Plan）の再現用Seedを指定し、前後の行の計算・呼び出しへつなげる。
                 Plan.Seed,
                 // 抽選された入口のセル列・方向・中央位置を、このSelectionへ受け取る。
                 Selection,
                 // この行では失敗理由（Error）を指定し、前後の行の計算・呼び出しへつなげる。
                 Error,
                 // 内幅に加えて壁を含む外幅も渡し、通路の外壁が別の床へ食い込む場所を候補から外す。
                 OuterWidth))
    {
        // ISM生成を失敗状態にして移動制限と暗転を解除する（引数：失敗理由（Error））を呼び出し元へ返す。
        return PrototypeFail(Error);
    }

    EntranceCells.Reset();
    for (const auto Cell : Selection.Cells)
    {
        // 壁を開ける入口セルの集合（EntranceCells）で、要素または対応関係を末尾へ追加する（引数：Cell）。
        EntranceCells.Add(Cell);
    }
    // 入口の外向き方向番号（EntranceOutwardDirection）を、Selectionの入口の外向き方向番号に設定する。
    EntranceOutwardDirection = Selection.OutwardDirection;
    // 内側の入口を選んだかの結果（bEntranceIsInterior）を、Selectionの全床の範囲の内側にある入口かの判定に設定する。
    bEntranceIsInterior = Selection.bInterior;
    // 入口境界のローカル中央座標（EntranceBoundaryCenter）を、Selectionの開口の境界中央に設定する。
    EntranceBoundaryCenter = Selection.BoundaryCenter;
    // const FVector Steps[]を、{に設定する。
    const FVector Steps[] = {
        // 3次元の東・北・西・南の単位ベクトルを、方向番号0〜3の順に並べる。
        FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0)};
    // InwardYaw（InwardYaw）を単精度の実数として用意し、東・北・西・南の移動差分（Steps）の「入口の外向き方向番号（EntranceOutwardDirection）」に対応する要素の符号を反転した値で、方向ベクトルを向く回転を求めるの水平角度で初期化する。
    const float InwardYaw = (-Steps[EntranceOutwardDirection]).Rotation().Yaw;
    // 回転（Rotation）を、括弧内の値を使って回転角として初期化する。
    const FRotator Rotation(0, EntranceTransform.Rotator().Yaw - InwardYaw, 0);
    if (bMoveEntranceAreaToMaze)
    {
        // 迷路は配置済みの位置・向きを保ち、導入エリアを抽選された入口へ動かす。
        // OldAreaTransform（OldAreaTransform）を位置・回転・拡大率として用意し、自作の導入エリアActor（EntranceArea）で、Actorの位置・回転・拡大率を取得するで初期化する。
        const FTransform OldAreaTransform = EntranceArea->GetActorTransform();
        // WorldBoundary（WorldBoundary）を3次元ベクトルとして用意し、Actorの位置・回転・拡大率を取得するで、ローカル位置を姿勢に従って変換する（引数：入口境界のローカル中央座標（EntranceBoundaryCenter））で初期化する。
        const FVector WorldBoundary = GetActorTransform().TransformPosition(EntranceBoundaryCenter);
        // WorldInward（WorldInward）を3次元ベクトルとして用意し、で初期化する。
        const FVector WorldInward =
            // Actorの位置・回転・拡大率を取得するで、拡大率を適用せず方向だけを変換する（引数：東・北・西・南の移動差分（Steps）の「入口の外向き方向番号（EntranceOutwardDirection）」に対応する要素の符号を反転した値）。
            GetActorTransform().TransformVectorNoScale(-Steps[EntranceOutwardDirection]);
        // ExitRotation（ExitRotation）を、括弧内の値を使って回転角として初期化する。
        const FRotator ExitRotation(0, WorldInward.Rotation().Yaw, 0);
        // NewAreaRotation（NewAreaRotation）を回転角として作る。括弧内に並べる値を初期値として使う。
        const FRotator NewAreaRotation(0,
            // この行ではOldAreaTransformで、姿勢の回転をオイラー角で取得するの水平角度とExitRotationの水平角度の和から導入通路出口の位置と向き（EntranceTransform）で、姿勢の回転をオイラー角で取得するの水平角度を引いた値を指定し、前後の行の計算・呼び出しへつなげる。
            OldAreaTransform.Rotator().Yaw + ExitRotation.Yaw - EntranceTransform.Rotator().Yaw,
            // 導入エリアのRollを0にする。水平回転だけを変更し、通路全体を横倒しにしない。
            0);

        // 通路内にある既存PlayerStartを優先し、通路に対する開始位置の関係を維持する。
        // SpawnTransform（SpawnTransform）を位置・回転・拡大率として用意し、OldAreaTransformで初期化する。
        FTransform SpawnTransform = OldAreaTransform;
        for (USceneComponent* Component : Components)
        {
            // 「対象のコンポーネント（Component）で、比較用の名前を取得するがFName(TEXT("PlayerSpawnPoint"))と等しい」に当てはまる場合だけ、続く文を実行する。
            if (Component->GetFName() == FName(TEXT("PlayerSpawnPoint")))
            {
                // SpawnTransformを、対象のコンポーネント（Component）で、部品のワールド姿勢を取得するに設定する。
                SpawnTransform = Component->GetComponentTransform();
                break;
            }
        }
        // 配置済みのPlayerStart Actor（Start）をAActorとして用意し、指定クラスのActorをワールドから1個取得する（引数：this、UEへ登録されたクラス情報を取得する）で初期化する。参照先がない場合もあるポインターとして扱う。
        AActor* Start = UGameplayStatics::GetActorOfClass(this, APlayerStart::StaticClass());
        // UseStart（UseStart）を真偽値として用意し、で初期化する。
        const bool UseStart =
            // 既設PlayerStartが有効で、導入エリアの外接範囲を150cm広げた内側にある場合だけ、その相対開始位置を優先する。
            IsValid(Start) && EntranceArea->GetComponentsBoundingBox().ExpandBy(150).IsInsideOrOn(
                                  // 配置済みのPlayerStart Actor（Start）で、Actorの位置を取得する。
                                  Start->GetActorLocation());
        if (UseStart)
        {
            // SpawnTransformを、配置済みのPlayerStart Actor（Start）で、Actorの位置・回転・拡大率を取得するに設定する。
            SpawnTransform = Start->GetActorTransform();
        }
        // RelativeSpawn（RelativeSpawn）を位置・回転・拡大率として用意し、SpawnTransformで、指定姿勢を基準にした相対姿勢を求める（引数：OldAreaTransform）で初期化する。
        const FTransform RelativeSpawn = SpawnTransform.GetRelativeTransform(OldAreaTransform);
        // LocalExit（LocalExit）を3次元ベクトルとして用意し、で初期化する。
        const FVector LocalExit =
            // OldAreaTransformで、ワールド位置をローカルへ逆変換する（引数：導入通路出口の位置と向き（EntranceTransform）で、位置を取得する）。
            OldAreaTransform.InverseTransformPosition(EntranceTransform.GetLocation());
        // NewAreaLocation（NewAreaLocation）を3次元ベクトルとして用意し、で初期化する。
        const FVector NewAreaLocation =
            // WorldBoundaryからNewAreaRotationで、指定ベクトルを回転させる（引数：LocalExitとOldAreaTransformで、3軸の拡大率を取得するの積）を引いた値。
            WorldBoundary - NewAreaRotation.RotateVector(LocalExit * OldAreaTransform.GetScale3D());
        // 導入エリアActorを、抽選した迷路入口に合う新しい位置と回転へ移動する。
        EntranceArea->SetActorLocationAndRotation(
            // 経路のSweep判定は行わず、物理状態の不要な速度変化を避けるTeleportPhysicsで配置する。
            NewAreaLocation, NewAreaRotation, false, nullptr, ETeleportType::TeleportPhysics);
        // 導入通路出口の位置と向き（EntranceTransform）を、位置・回転・拡大率をまとめた姿勢を組み立てる（引数：ExitRotation、WorldBoundary）に設定する。
        EntranceTransform = FTransform(ExitRotation, WorldBoundary);
        // 導入エリア内でのプレイヤー配置姿勢（EntranceSpawnTransform）を、RelativeSpawnと自作の導入エリアActor（EntranceArea）で、Actorの位置・回転・拡大率を取得するの積に設定する。
        EntranceSpawnTransform = RelativeSpawn * EntranceArea->GetActorTransform();
        if (UseStart)
        {
            // 配置済みのPlayerStart Actor（Start）で、基準となるルート部品を取得するで、移動可能かなどのMobilityを設定する（引数：EComponentMobility::Movable）。
            Start->GetRootComponent()->SetMobility(EComponentMobility::Movable);
            // 導入エリアと一緒に動かした開始位置を、配置済みPlayerStartにも反映する。
            Start->SetActorTransform(
                // 計算した開始姿勢を、移動経路のSweepなし・TeleportPhysicsでPlayerStartへ設定する。
                EntranceSpawnTransform, false, nullptr, ETeleportType::TeleportPhysics);
        }
    }
    // 直前の条件に当てはまらなかった場合の処理へ進む。
    else
    {
        // 導入エリアの位置を固定する設定の場合は、迷路Actorの方を出口に合う姿勢へ移動する。
        SetActorLocationAndRotation(
            // この行では導入通路出口の位置と向き（EntranceTransform）で、位置を取得するから回転（Rotation）で、指定ベクトルを回転させる（引数：入口境界のローカル中央座標（EntranceBoundaryCenter））を引いた値を指定し、前後の行の計算・呼び出しへつなげる。
            EntranceTransform.GetLocation() - Rotation.RotateVector(EntranceBoundaryCenter),
            // 入口の外向き方向と出口方向の差から求めた、迷路全体の回転を適用する。
            Rotation);
    }
    // ログへ計測結果・警告・失敗理由を記録する。引数の値を指定書式へ埋め込む。
    UE_LOG(LogTemp,
        // 入口のSeed・方向・実幅などの確認情報を、通常表示レベルのログへ出す。
        Display,
        // この行では"COURTYARD_ENTRANCE seed=%d side=%d width=%.1f measured=%d cells=%d"というUE文字列を指定し、前後の行の計算・呼び出しへつなげる。
        TEXT("COURTYARD_ENTRANCE seed=%d side=%d width=%.1f measured=%d cells=%d"),
        // この行では生成結果（Plan）の再現用Seedを指定し、前後の行の計算・呼び出しへつなげる。
        Plan.Seed,
        // この行では入口の外向き方向番号（EntranceOutwardDirection）を指定し、前後の行の計算・呼び出しへつなげる。
        EntranceOutwardDirection,
        // この行では測定・採用した入口の実幅（ActualEntranceWidth）を指定し、前後の行の計算・呼び出しへつなげる。
        ActualEntranceWidth,
        // この行では内壁から幅を測定できたかの結果（bEntranceWidthMeasured）を指定し、前後の行の計算・呼び出しへつなげる。
        bEntranceWidthMeasured,
        // 壁を開ける入口セルの集合（EntranceCells）で、要素数を得る。
        EntranceCells.Num());
    return true;
}

/** セル幅に丸めて開けた部分の端数だけを塞ぎ、自作通路と同じ実幅の開口にする。 */
void ACourtyardISMGenerator::QueueEntranceJambs()
{
    if (!IsValid(EntranceArea) || EntranceCells.IsEmpty())
    {
        return;
    }
    const float Extra = (EntranceCells.Num() * CellSize - ActualEntranceWidth) * .5f;
    if (Extra < 1)
    {
        return;
    }
    const FVector Directions[] = {FVector(1,0,0), FVector(0,1,0), FVector(-1,0,0), FVector(0,-1,0)};
    const FVector Outward = Directions[EntranceOutwardDirection];
    const FVector Tangent(-Outward.Y, Outward.X, 0);
    const FRotator Rotation(0, EntranceOutwardDirection * 90.f, 0);
    const int32 OriginalPlacementCount = ModulePlacements.Num();
    for (float Sign : {-1.f, 1.f})
    {
        FIntPoint SideCell = *EntranceCells.CreateConstIterator();
        double Best = -DBL_MAX;
        for (const auto P : EntranceCells)
        {
            const double Projection = Sign * (P.X * Tangent.X + P.Y * Tangent.Y);
            if (Projection > Best)
            {
                Best = Projection;
                SideCell = P;
            }
        }
        const int32* StyleId = WallStyleOwners.Find(SideCell);
        const auto* Floor = Plan.Cells.FindByPredicate([&](const FCourtyardCell& Cell) { return Cell.Position == SideCell; });
        const int32 NodeId = StyleId ? *StyleId : Floor ? Floor->NodeId : 0;
        auto Level = SelectFacadeForNode(NodeId, Theme->Facades[0]);
        const FVector Center = EntranceBoundaryCenter + Tangent * (Sign * (ActualEntranceWidth + Extra) * .5f);
        const FVector Origin = Center - Rotation.RotateVector(FVector(0, -Extra * .5f, 0));
        // 同じ高さの既定壁だけでなく、接続脇に実際に配置した壁の種類も引き継ぐ。
        const bool NormalStyle = Theme->Facades.Contains(Level);
        double BestDistance = DBL_MAX;
        for (int32 Index = 0; Index < OriginalPlacementCount; ++Index)
        {
            const auto& Candidate = ModulePlacements[Index];
            if (Candidate.bPassage || (NormalStyle ? !Theme->Facades.Contains(Candidate.Level) : Candidate.Level != Level))
            {
                continue;
            }
            const double RelativeYaw = Candidate.Transform.Rotator().Yaw - GetActorRotation().Yaw;
            if (FMath::Abs(FMath::FindDeltaAngleDegrees(RelativeYaw, Rotation.Yaw)) > 1)
            {
                continue;
            }
            const FVector CandidateCenter = GetActorTransform().InverseTransformPosition(Candidate.Transform.GetLocation())
                + Rotation.RotateVector(FVector(0, -CellSize * .5f, 0));
            if (Sign * FVector::DotProduct(CandidateCenter - EntranceBoundaryCenter, Tangent) <= 0)
            {
                continue;
            }
            const double Distance = FVector::DistSquared2D(CandidateCenter, Center);
            if (Distance < BestDistance)
            {
                BestDistance = Distance;
                Level = Candidate.Level;
            }
        }

        FCourtyardModulePlacement Placement;
        Placement.Level = Level;
        Placement.Transform = FTransform(Rotation, Origin, FVector(CellSize / 400.f)) * GetActorTransform();
        // 床メッシュの箱へ置き換えず、同じNormal/Low壁を開口脇の幅へ合わせる。
        Placement.LocalAdjustment = FitEntranceModule(Level, Extra / CellSize);
        ModulePlacements.Add(MoveTemp(Placement));
    }
}

/** 接続部分だけに使う、メッシュを差し替えない薄い壁のISM配置。 */
void ACourtyardISMGenerator::QueueEntranceBox(const FVector& Center, const FVector& Size)
{
    // メッシュなどの外接範囲（Bounds）を右辺から自動判定した型として用意し、壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュで、メッシュの外接範囲を取得するで初期化する。
    const auto Bounds = Theme->FloorMesh->GetBounds();
    // 拡大率（Scale）を3次元ベクトルとして用意し、範囲の大きさ（Size）をメッシュなどの外接範囲（Bounds）の外接Boxの半サイズと2の積で割った値で初期化する。
    const FVector Scale = Size / (Bounds.BoxExtent * 2);
    // 今回の配置データまたは対応表の1件（Entry）をISMのメッシュ・材質・衝突・配置設定として宣言する。
    FCourtyardISMMesh Entry;
    // 今回の配置データまたは対応表の1件（Entry）のメッシュ参照を、壁・床・通路の見た目の参照データ（Theme）の床に使うメッシュに設定する。
    Entry.Mesh = Theme->FloorMesh;
    // 今回の配置データまたは対応表の1件（Entry）のマテリアル一覧で、要素または対応関係を末尾へ追加する（引数：壁・床・通路の見た目の参照データ（Theme）の天井の材質）。
    Entry.Materials.Add(Theme->CeilingMaterial);
    // 今回の配置データまたは対応表の1件（Entry）のローカル配置姿勢を、に設定する。
    Entry.LocalTransform =
        // 回転なし・求めた位置・必要な拡大率で壁の姿勢を作り、次行で迷路Actorの姿勢と合成する。
        FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * Scale, Scale) *
        // Actorの位置・回転・拡大率を取得する。
        GetActorTransform();
    // 同じ描画・衝突設定のメッシュをISMグループへまとめる（引数：今回の配置データまたは対応表の1件（Entry））。
    QueueMesh(Entry);
}

/** 内側の部屋へ導入通路を置く際、建物の背面が導入メッシュと交差する配置を検出する。 */
bool ACourtyardISMGenerator::BoundaryOverlapsEntrance(
    // Cell（Cell）、int32 Direction（int32 Direction）、TSoftObjectPtr<UWorld> Level) const（TSoftObjectPtr<UWorld> Level) const）を、それぞれ2次元の整数セル座標として用意する。
    FIntPoint Cell, int32 Direction, TSoftObjectPtr<UWorld> Level) const
{
    if (!IsValid(EntranceArea))
    {
        return false;
    }
    // 配置する1種類の部品データ（Module）を右辺から自動判定した型として用意し、レベルから抽出した配置部品のカタログ（ModuleCatalog）のModules.FindByPredicate(で初期化する。参照先がない場合もあるポインターとして扱う。
    const auto* Module = ModuleCatalog->Modules.FindByPredicate(
        // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
        [&](const FCourtyardISMModule& Item)
        {
            // Itemの元になった部品レベルの参照パスが配置する部品レベル（Level）で、参照先アセットのパスを取得すると等しいを呼び出し元へ返す。
            return Item.SourceLevel == Level.ToSoftObjectPath();
        });
    if (!Module)
    {
        return false;
    }
    // const FVector Steps[]を、{に設定する。
    const FVector Steps[] = {
        // 外壁部品を置く境界位置を計算するため、四方向の単位ベクトルを用意する。
        FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0)};
    // 水平回転（Yaw）を、括弧内の値を使って回転角として初期化する。
    const FRotator Yaw(0, Direction * 90.f, 0);
    // 中央位置（Center）を3次元ベクトルとして用意し、で初期化する。
    const FVector Center =
        // 3次元の位置・方向・寸法を組み立てる（引数：CellのX成分と1セルの実寸（cm）（CellSize）の積、CellのY成分と1セルの実寸（cm）（CellSize）の積、0）と東・北・西・南の移動差分（Steps）の「四方向の番号（Direction）」に対応する要素と1セルの実寸（cm）（CellSize）との5fの積の積の和。
        FVector(Cell.X * CellSize, Cell.Y * CellSize, 0) + Steps[Direction] * (CellSize * .5f);
    // 配置または計測の基準位置（Origin）を3次元ベクトルとして用意し、中央位置（Center）から水平回転（Yaw）で、指定ベクトルを回転させる（引数：3次元の位置・方向・寸法を組み立てる（引数：0、1セルの実寸（cm）（CellSize）の符号を反転した値との5fの積、0））を引いた値で初期化する。
    const FVector Origin = Center - Yaw.RotateVector(FVector(0, -CellSize * .5f, 0));
    // 配置する姿勢と部品情報（Placement）を位置・回転・拡大率として用意し、で初期化する。
    const FTransform Placement =
        // 位置・回転・拡大率をまとめた姿勢を組み立てる（引数：水平回転（Yaw）、配置または計測の基準位置（Origin）、3次元の位置・方向・寸法を組み立てる（引数：1セルの実寸（cm）（CellSize）を400.fで割った値））とActorの位置・回転・拡大率を取得するの積。
        FTransform(Yaw, Origin, FVector(CellSize / 400.f)) * GetActorTransform();
    // Introduction（Introduction）をFBoxとして用意し、自作の導入エリアActor（EntranceArea）で、部品全体を含む外接Boxを取得するで初期化する。
    FBox Introduction = EntranceArea->GetComponentsBoundingBox();
    // 通路メッシュの端だけで判定すると、出口の数cm先にある壁の張り出しを見逃す。
    // プレイヤーが迷路へ踏み込む領域まで、同じ壁モデルの幅・奥行き補正を適用する。
    const FVector Forward = EntranceTransform.GetUnitAxis(EAxis::X);
    const FVector Right = EntranceTransform.GetUnitAxis(EAxis::Y);
    for (float Distance : {-80.f, CellSize * .5f})
    {
        for (float Side : {-ActualEntranceWidth * .5f, ActualEntranceWidth * .5f})
        {
            for (float Height : {0.f, 210.f})
            {
                Introduction += EntranceTransform.GetLocation() + Forward * Distance + Right * Side + FVector(0, 0, Height);
            }
        }
    }
    for (const auto& Mesh : Module->Meshes)
    {
        // 「「静的メッシュ（Mesh）のメッシュ参照」かつ「静的メッシュ（Mesh）のメッシュ参照のGetBoundingBox(」」に当てはまる場合だけ、続く文を実行する。
        if (Mesh.Mesh && Mesh.Mesh->GetBoundingBox()
                             // この行では外接範囲へ配置姿勢の変換を適用する（引数：静的メッシュ（Mesh）のローカル配置姿勢と配置する姿勢と部品情報（Placement）の積で、拡大率も含めた変換行列を取得する）を指定し、前後の行の計算・呼び出しへつなげる。
                             .TransformBy((Mesh.LocalTransform * Placement).ToMatrixWithScale())
                             // この行では2つの範囲が重なるか調べる（引数：Introduction）を指定し、前後の行の計算・呼び出しへつなげる。
                             .Intersect(Introduction))
        {
            return true;
        }
    }
    return false;
}

/** 建物背面を省いた境界も、床の端には壁を残し、迷路の外へ落ちる穴を作らない。 */
void ACourtyardISMGenerator::QueueThinEntranceBoundary(FIntPoint Cell, int32 Direction)
{
    // const FVector Steps[]を、{に設定する。
    const FVector Steps[] = {
        // 薄い壁や扉枠の位置と寸法を向きに応じて変えるため、四方向の基準を用意する。
        FVector(1, 0, 0), FVector(0, 1, 0), FVector(-1, 0, 0), FVector(0, -1, 0)};
    // 高さ（Height）を単精度の実数として用意し、350.fと1セルの実寸（cm）（CellSize）の積を400.fで割った値で初期化する。
    const float Height = 350.f * CellSize / 400.f;
    // Thickness（Thickness）を単精度の実数として用意し、16.fと1セルの実寸（cm）（CellSize）の積を400.fで割った値で初期化する。
    const float Thickness = 16.f * CellSize / 400.f;
    // 中央位置（Center）を3次元ベクトルとして用意し、FVector(Cell.X * CellSize, Cell.Y * CellSize, Height * .5f) +で初期化する。
    const FVector Center = FVector(Cell.X * CellSize, Cell.Y * CellSize, Height * .5f) +
                           // 東・北・西・南の移動差分（Steps）の「四方向の番号（Direction）」に対応する要素と1セルの実寸（cm）（CellSize）との5fの積の積。
                           Steps[Direction] * (CellSize * .5f);
    // 範囲の大きさ（Size）を3次元ベクトルとして用意し、四方向の番号（Direction）を2で割った余りが0 ? FVector(Thickness, CellSize, Height)と等しいで初期化する。
    const FVector Size = Direction % 2 == 0 ? FVector(Thickness, CellSize, Height)
                                            // この関数の役割：3次元の位置・方向・寸法を組み立てる。ここでは呼び出し方を宣言する。
                                            : FVector(CellSize, Thickness, Height);
    // Neighbor（Neighbor）を2次元の整数セル座標として用意し、で初期化する。
    const FIntPoint Neighbor =
        // Cellと2次元の整数セル座標を組み立てる（引数：符号付き32ビット整数へ変換する（引数：東・北・西・南の移動差分（Steps）の「四方向の番号（Direction）」に対応する要素のX成分）、符号付き32ビット整数へ変換する（引数：東・北・西・南の移動差分（Steps）の「四方向の番号（Direction）」に対応する要素のY成分））の和。
        Cell + FIntPoint(int32(Steps[Direction].X), int32(Steps[Direction].Y));
    // IsPortal（IsPortal）を真偽値として用意し、生成結果（Plan）のセル列のContainsByPredicate(で初期化する。
    const bool IsPortal = Plan.Cells.ContainsByPredicate(
        // ここから、並び替え条件や座標変換を返す小さな関数を定義する。引数はこの括弧で受け取る。
        [&](const FCourtyardCell& Floor)
        {
            // 歩ける床の集合（Floor）のセル座標がNeighborと等しいを呼び出し元へ返す。
            return Floor.Position == Neighbor;
        });
    if (IsPortal)
    {
        // トンネルへの既存の開口を、薄い壁へ置き換える際に塞がない。
        // Opening（Opening）を単精度の実数として用意し、180.fと1セルの実寸（cm）（CellSize）の積を400.fで割った値で初期化する。
        const float Opening = 180.f * CellSize / 400.f;
        // DoorHeight（DoorHeight）を単精度の実数として用意し、260.fと1セルの実寸（cm）（CellSize）の積を400.fで割った値で初期化する。
        const float DoorHeight = 260.f * CellSize / 400.f;
        // 横方向または盤面の一辺（Side）を単精度の実数として用意し、1セルの実寸（cm）（CellSize）からOpeningを引いた値との5fの積で初期化する。
        const float Side = (CellSize - Opening) * .5f;
        // 入口の幅方向（Tangent）を、括弧内の値を使って3次元ベクトルとして初期化する。
        const FVector Tangent(-Steps[Direction].Y, Steps[Direction].X, 0);
        for (float Sign : {-1.f, 1.f})
        {
            // SideCenter（SideCenter）を3次元ベクトルとして用意し、中央位置（Center）と入口の幅方向（Tangent）の和と(Sign * (Opening + Side) * .5f) +の積で初期化する。
            const FVector SideCenter = Center + Tangent * (Sign * (Opening + Side) * .5f) +
                                       // 3次元の位置・方向・寸法を組み立てる（引数：0、0、DoorHeightから高さ（Height）を引いた値との5fの積）。
                                       FVector(0, 0, (DoorHeight - Height) * .5f);
            // SideSize（SideSize）を3次元ベクトルとして用意し、四方向の番号（Direction）を2で割った余りが0 ? FVector(Thickness, Side, DoorHeight)と等しいで初期化する。
            const FVector SideSize = Direction % 2 == 0 ? FVector(Thickness, Side, DoorHeight)
                                                        // この関数の役割：3次元の位置・方向・寸法を組み立てる。ここでは呼び出し方を宣言する。
                                                        : FVector(Side, Thickness, DoorHeight);
            // 入口周辺の薄い壁をISMの描画待ちへ追加する（引数：SideCenter、SideSize）。
            QueueEntranceBox(SideCenter, SideSize);
        }
        // 扉の上部をふさぐ壁を、扉高の半分だけ上へずらした中央位置へ追加する。
        QueueEntranceBox(Center + FVector(0, 0, DoorHeight * .5f),
            // この行では四方向の番号（Direction）を2で割った余りが0 ? FVector(Thickness, CellSize, Height - DoorHeight)と等しいを指定し、前後の行の計算・呼び出しへつなげる。
            Direction % 2 == 0 ? FVector(Thickness, CellSize, Height - DoorHeight)
                               // この関数の役割：3次元の位置・方向・寸法を組み立てる。ここでは呼び出し方を宣言する。
                               : FVector(CellSize, Thickness, Height - DoorHeight));
        return;
    }
    // 入口周辺の薄い壁をISMの描画待ちへ追加する（引数：中央位置（Center）、範囲の大きさ（Size））。
    QueueEntranceBox(Center, Size);
}

/** メッシュの物理寸法だけを共有。候補の列挙・抽選はBP生成版が担当する。 */
FVector ACourtyardISMGenerator::GetBlueprintEntranceDimensions()
{
    if (!IsValid(EntranceArea) ||
        // 移動先のGameModeがあれば、そこに保存されたレベル移動オプションからSkipIntroの値を読む。
        UGameplayStatics::ParseOption(UGameplayStatics::GetGameMode(this)
                                          // GameModeが存在する場合は、そのOptionsStringをオプション解析へ渡す。
                                          ? UGameplayStatics::GetGameMode(this)->OptionsString
                                          // GameModeがないエディター等では空文字列を渡し、無効な参照を読まないようにする。
                                          : FString(),
            // この行ではTEXT("SkipIntro")) == TEXT("1"を指定し、前後の行の計算・呼び出しへつなげる。
            TEXT("SkipIntro")) == TEXT("1"))
    {
        // FVector::ZeroVectorを呼び出し元へ返す。
        return FVector::ZeroVector;
    }
    // Point（Point）をUSceneComponentとして用意し、nullptr（参照先なし）で初期化する。参照先がない場合もあるポインターとして扱う。
    USceneComponent* Point = nullptr;
    // Actorが持つコンポーネント一覧（Components）をTInlineComponentArray<USceneComponent*>として宣言する。参照先がない場合もあるポインターとして扱う。
    TInlineComponentArray<USceneComponent*> Components;
    // 自作の導入エリアActor（EntranceArea）で、Actorの部品一覧を取得する（引数：Actorが持つコンポーネント一覧（Components））。
    EntranceArea->GetComponents(Components);
    for (USceneComponent* Component : Components)
    {
        // 「対象のコンポーネント（Component）で、部品に指定タグがあるか調べる（引数："MazeEntrance"というUE文字列）」、または次行の条件に当てはまる場合だけ、続く文を実行する。
        if (Component->ComponentHasTag(TEXT("MazeEntrance")) ||
            // この行では対象のコンポーネント（Component）で、比較用の名前を取得するが"MazeConnectionPoint"というUE文字列と等しいを指定し、前後の行の計算・呼び出しへつなげる。
            Component->GetFName() == TEXT("MazeConnectionPoint"))
        {
            // Pointを、対象のコンポーネント（Component）に設定する。
            Point = Component;
            break;
        }
    }
    if (!Point)
    {
        // 3次元の位置・方向・寸法を組み立てる（引数：-1、0、0）を呼び出し元へ返す。
        return FVector(-1, 0, 0);
    }

    // 導入通路出口の位置と向き（EntranceTransform）を、FTransform(に設定する。
    EntranceTransform = FTransform(
        // FRotator(0, Point->GetComponentRotation().Yaw, 0), Pointで、部品のワールド位置を取得する。
        FRotator(0, Point->GetComponentRotation().Yaw, 0), Point->GetComponentLocation());
    // 測定・採用した入口の実幅（ActualEntranceWidth）を、測れなかった場合に使う入口幅（EntranceWidth）に設定する。
    ActualEntranceWidth = EntranceWidth;
    bEntranceWidthMeasured = false;
    if (bMeasureEntranceWidth)
    {
        // 前方向の単位ベクトル（Forward）を3次元ベクトルとして用意し、導入通路出口の位置と向き（EntranceTransform）で、指定軸の単位方向を取得する（引数：EAxis::X）で初期化する。
        const FVector Forward = EntranceTransform.GetUnitAxis(EAxis::X);
        // 右方向の単位ベクトル（Right）を3次元ベクトルとして用意し、導入通路出口の位置と向き（EntranceTransform）で、指定軸の単位方向を取得する（引数：EAxis::Y）で初期化する。
        const FVector Right = EntranceTransform.GetUnitAxis(EAxis::Y);
        // 出口から少し内側、床より1.1m上で左右の内壁を測る。床だけのBoxには反応しない。
        // 配置または計測の基準位置（Origin）を3次元ベクトルとして用意し、導入通路出口の位置と向き（EntranceTransform）で、位置を取得するから前方向の単位ベクトル（Forward）と80の積を引いた値と3次元の位置・方向・寸法を組み立てる（引数：0、0、110）の和で初期化する。
        const FVector Origin = EntranceTransform.GetLocation() - Forward * 80 + FVector(0, 0, 110);
        // LeftHit（LeftHit）、RightHit（RightHit）を、それぞれ衝突判定の結果として用意する。
        FHitResult LeftHit, RightHit;
        // 衝突判定の条件（Query）を、括弧内の値を使って衝突問い合わせの設定として初期化する。
        FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardEntranceWidth), true);
        // 「自作の導入エリアActor（EntranceArea）のActorLineTraceSingle(」に当てはまる場合だけ、続く文を実行する。
        if (EntranceArea->ActorLineTraceSingle(
                // この行ではLeftHit, Origin, Originから右方向の単位ベクトル（Right）と4000, ECC_Visibility, Queryの積を引いた値を指定し、前後の行の計算・呼び出しへつなげる。
                LeftHit, Origin, Origin - Right * 4000, ECC_Visibility, Query) &&
            // 出口の内側から右方向へ線を飛ばし、この導入エリアActorの内壁との衝突位置を調べる。
            EntranceArea->ActorLineTraceSingle(
                // この行ではRightHit, Origin, Originと右方向の単位ベクトル（Right）と4000, ECC_Visibility, Queryの積の和を指定し、前後の行の計算・呼び出しへつなげる。
                RightHit, Origin, Origin + Right * 4000, ECC_Visibility, Query))
        {
            // 幅（Width）を単精度の実数として用意し、で初期化する。
            const float Width =
                // 3次元の内積で指定方向への距離を求める（引数：RightHitのImpactPointからLeftHitのImpactPointを引いた値、右方向の単位ベクトル（Right））。
                FVector::DotProduct(RightHit.ImpactPoint - LeftHit.ImpactPoint, Right);
            // 「「幅（Width）が140以上」かつ「幅（Width）が4000以下」」に当てはまる場合だけ、続く文を実行する。
            if (Width >= 140 && Width <= 4000)
            {
                // 測定・採用した入口の実幅（ActualEntranceWidth）を、幅（Width）に設定する。
                ActualEntranceWidth = Width;
                bEntranceWidthMeasured = true;
                // 中央位置（Center）を3次元ベクトルとして用意し、LeftHitのImpactPointとRightHitのImpactPointの和との5fの積で初期化する。
                const FVector Center = (LeftHit.ImpactPoint + RightHit.ImpactPoint) * .5f;
                // 左右の内壁の中央から求めた横方向の補正を、出口姿勢の位置へ加える。
                EntranceTransform.AddToTranslation(
                    // 右方向の単位ベクトル（Right）と3次元の内積で指定方向への距離を求める（引数：中央位置（Center）から配置または計測の基準位置（Origin）を引いた値、右方向の単位ベクトル（Right））の積。
                    Right * FVector::DotProduct(Center - Origin, Right));
            }
        }
    }

    // 外接範囲の中央（BoundsOrigin）、外接範囲の半サイズ（BoundsExtent）を、それぞれ3次元ベクトルとして用意する。
    FVector BoundsOrigin, BoundsExtent;
    // 自作の導入エリアActor（EntranceArea）で、Actor全体の外接範囲を取得する（引数：false（不成立・無効）、外接範囲の中央（BoundsOrigin）、外接範囲の半サイズ（BoundsExtent））。
    EntranceArea->GetActorBounds(false, BoundsOrigin, BoundsExtent);
    // 前方向の単位ベクトル（Forward）を3次元ベクトルとして用意し、導入通路出口の位置と向き（EntranceTransform）で、指定軸の単位方向を取得する（引数：EAxis::X）で初期化する。
    const FVector Forward = EntranceTransform.GetUnitAxis(EAxis::X);
    // 右方向の単位ベクトル（Right）を3次元ベクトルとして用意し、導入通路出口の位置と向き（EntranceTransform）で、指定軸の単位方向を取得する（引数：EAxis::Y）で初期化する。
    const FVector Right = EntranceTransform.GetUnitAxis(EAxis::Y);
    // OuterWidth（OuterWidth）を単精度の実数として用意し、で初期化する。
    const float OuterWidth =
        // 2と符号を取り除いた絶対値を求める（引数：右方向の単位ベクトル（Right）のX成分）と外接範囲の半サイズ（BoundsExtent）のX成分の積と符号を取り除いた絶対値を求める（引数：右方向の単位ベクトル（Right）のY成分）と外接範囲の半サイズ（BoundsExtent）のY成分の積の和の積。
        2 * (FMath::Abs(Right.X) * BoundsExtent.X + FMath::Abs(Right.Y) * BoundsExtent.Y);
    // Depth（Depth）を単精度の実数として用意し、で初期化する。
    const float Depth =
        // 2と符号を取り除いた絶対値を求める（引数：前方向の単位ベクトル（Forward）のX成分）と外接範囲の半サイズ（BoundsExtent）のX成分の積と符号を取り除いた絶対値を求める（引数：前方向の単位ベクトル（Forward）のY成分）と外接範囲の半サイズ（BoundsExtent）のY成分の積の和の積。
        2 * (FMath::Abs(Forward.X) * BoundsExtent.X + FMath::Abs(Forward.Y) * BoundsExtent.Y);
    // 3次元の位置・方向・寸法を組み立てる（引数：測定・採用した入口の実幅（ActualEntranceWidth）、Depth、OuterWidth）を呼び出し元へ返す。
    return FVector(ActualEntranceWidth, Depth, OuterWidth);
}

/** 同じ壁モデルの高さ・材質を保ち、導入接続に必要な幅と奥行きへ合わせる。 */
FTransform ACourtyardISMGenerator::FitEntranceModule(TSoftObjectPtr<UWorld> Level, float WidthScale) const
{
    const auto* Module = ModuleCatalog->Modules.FindByPredicate(
        [&](const FCourtyardISMModule& Item) { return Item.SourceLevel == Level.ToSoftObjectPath(); });
    FBox Bounds(ForceInit);
    if (Module)
    {
        for (const auto& Item : Module->Meshes)
        {
            if (Item.Mesh)
            {
                Bounds += Item.Mesh->GetBoundingBox().TransformBy(Item.LocalTransform.ToMatrixWithScale());
            }
        }
    }
    const float Depth = Bounds.IsValid ? FMath::Max(1.f, float(Bounds.GetSize().X)) : 400.f;
    const float DepthScale = FMath::Min(1.f, 16.f / Depth);
    const float Offset = Bounds.IsValid ? -Bounds.Min.X * DepthScale : 0.f;
    // 壁LVには1セル幅を超える屋根・脇壁も含まれる。開口脇では実際の全幅を測って収める。
    const bool FitWidth = Bounds.IsValid != 0;
    const float TargetWidth = 400.f * WidthScale;
    const float YScale = FitWidth ? TargetWidth / FMath::Max(1.f, float(Bounds.GetSize().Y)) : WidthScale;
    const float YOffset = FitWidth ? -TargetWidth - Bounds.Min.Y * YScale : 0.f;
    return FTransform(FQuat::Identity, FVector(Offset, YOffset, 0), FVector(DepthScale, YScale, 1));
}
