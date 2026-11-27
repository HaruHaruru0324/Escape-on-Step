#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardLayout.generated.h"

// 列挙型をUEへ登録し、BPでも種類の選択に使えるようにする。
UENUM(BlueprintType)
// 床の種類や導入方法などを、名前付きの選択肢で表す列挙型を宣言する。
enum class ECourtyardCellKind : uint8
{
    // 屋外床を表す種類。広い空間と屋外の細道に使う。
    Street,
    // 短い直線トンネルの床を表す種類。
    Tunnel,
    // 壁と天井に囲まれた屋内部屋の床を表す種類。
    Room
};

/** BPでもBreak/Makeして扱える、セル単位の生成設定。
 * 新方式の主な参照項目: StreetCount / MaxTunnelCells / RoomChance / MaxRooms。
 * 従来方式との互換用に保持している設定も含むため、全項目を新方式で使うわけではない。
 */
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardSettings
{
    // UEが型登録・BP連携・保存のために生成する宣言を挿入する。
    GENERATED_BODY()
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能）。
    UPROPERTY(EditAnywhere,
        // UEの公開設定を続け、BPから読み書きできる範囲や編集可能な場所を指定する。
        BlueprintReadWrite,
        // 詳細パネルやBPの一覧で、この値を表示する分類名を指定する。
        Category = "Layout",
        // 詳細パネルでの入力範囲など、編集補助のメタデータを指定する。実行時の計算式ではない。
        meta = (ClampMin = "9", ClampMax = "24"))
    // 広い空間を何個作るかの基準（StreetCount）を符号付き32ビット整数として用意し、12で初期化する。
    int32 StreetCount = 12;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "2", ClampMax = "3"))
    // 旧方式の広い道の幅（セル）（StreetWidth）を符号付き32ビット整数として用意し、2で初期化する。
    int32 StreetWidth = 2;
    // 道幅は保ち、曲がり道が囲む空白を2セルに詰める。無効なら従来の寸法。
    // この後の値をUEのプロパティとして登録する（既定値・配置済みActorの両方から編集可能、BPで読み書き可能）。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 旧方式で空白を詰める設定（bDensePacking）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bDensePacking = true;
    // Alternate small row/column offsets without widening the short tunnel gaps.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 旧方式で整列を少しずらす設定（bStaggeredLayout）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bStaggeredLayout = true;
    // Prefer hidden exits and return paths through several spaces, with no new long dead ends.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 旧方式で見通しのよすぎる接続を減らす設定（bDisorientingRoutes）を真偽値として用意し、true（成立・有効）で初期化する。
    bool bDisorientingRoutes = true;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "4", ClampMax = "12"))
    // 旧方式の広い道の最短長（セル）（MinStreetLength）を符号付き32ビット整数として用意し、4で初期化する。
    int32 MinStreetLength = 4;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "4", ClampMax = "12"))
    // 旧方式の広い道の最長長（セル）（MaxStreetLength）を符号付き32ビット整数として用意し、6で初期化する。
    int32 MaxStreetLength = 6;
    // 旧周回路版との保存互換用。現在の迷路タイルの寸法には使用しない。
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "1", ClampMax = "8"))
    // 旧方式の空間間隔の最小値（セル）（MinBlockGap）を符号付き32ビット整数として用意し、1で初期化する。
    int32 MinBlockGap = 1;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "1", ClampMax = "8"))
    // 旧方式の空間間隔の最大値（セル）（MaxBlockGap）を符号付き32ビット整数として用意し、3で初期化する。
    int32 MaxBlockGap = 3;
    // 広い道の延長と連結ルームを除いた、実際の狭いトンネルの長さ。
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "1", ClampMax = "3"))
    // トンネルの最短セル数（MinTunnelCells）を符号付き32ビット整数として用意し、1で初期化する。
    int32 MinTunnelCells = 1;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "1", ClampMax = "3"))
    // トンネルの最長セル数（MaxTunnelCells）を符号付き32ビット整数として用意し、1で初期化する。
    int32 MaxTunnelCells = 1;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "2", ClampMax = "4"))
    // 旧方式で1空間を作る最小区画数（MinRoomSections）を符号付き32ビット整数として用意し、2で初期化する。
    int32 MinRoomSections = 2;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "2", ClampMax = "4"))
    // 旧方式で1空間を作る最大区画数（MaxRoomSections）を符号付き32ビット整数として用意し、4で初期化する。
    int32 MaxRoomSections = 4;
    // 最低限の分岐・周回路を保証した上で、残りの短い接続を追加する確率。
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "0", ClampMax = "1"))
    // 旧方式で余分な短い接続を追加する確率（ExtraConnectionChance）を単精度の実数として用意し、0.75fで初期化する。
    float ExtraConnectionChance = 0.75f;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "0", ClampMax = "1"))
    // 屋内部屋を追加・選択する確率（RoomChance）を単精度の実数として用意し、0.35fで初期化する。
    float RoomChance = 0.35f;
    UPROPERTY(EditAnywhere,
        BlueprintReadWrite,
        Category = "Layout",
        meta = (ClampMin = "0", ClampMax = "6"))
    // 屋内部屋の上限数（MaxRooms）を符号付き32ビット整数として用意し、3で初期化する。
    int32 MaxRooms = 3;
};

/** 広い道またはルームの1区画。隣とは直角に曲がり、端の幅×幅の角を共有する。 */
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardRoomSection
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の最小端（Min）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint Min = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の大きさ（Size）を2次元の整数セル座標として用意し、2次元の整数セル座標を組み立てる（引数：2、2）で初期化する。
    FIntPoint Size = FIntPoint(2, 2);
};

// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardNode
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 空間の識別番号（Id）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Id = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // Kind（Kind）をECourtyardCellKindとして用意し、ECourtyardCellKind::Streetで初期化する。
    ECourtyardCellKind Kind = ECourtyardCellKind::Street;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の最小端（Min）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint Min = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の大きさ（Size）を2次元の整数セル座標として用意し、2次元の整数セル座標を組み立てる（引数：1、1）で初期化する。
    FIntPoint Size = FIntPoint(1, 1);
    // Min/Sizeは外接矩形。折れた空間の実床はこの配列で表す。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 実際の床セル一覧（FloorCells）を配列として宣言する。
    TArray<FIntPoint> FloorCells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 曲がる空間を構成する長方形区画（Sections）を配列として宣言する。
    TArray<FCourtyardRoomSection> Sections;
    // 接続を変えず、骨格の側面に足す短い張り出し・部屋の拡幅部分。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 短い張り出しとして追加した床（DetailCells）を配列として宣言する。
    TArray<FIntPoint> DetailCells;
};

// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardCell
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // セルまたは配置位置（Position）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint Position = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // Kind（Kind）をECourtyardCellKindとして用意し、ECourtyardCellKind::Streetで初期化する。
    ECourtyardCellKind Kind = ECourtyardCellKind::Street;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 所属する広い空間のID（NodeId）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
    int32 NodeId = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 所属する接続経路のID（LinkId）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
    int32 LinkId = INDEX_NONE;
    // 0: X方向の通路、1: Y方向の通路。通路以外では使わない。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 長手方向の軸（Axis）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Axis = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 所属する長方形区画の添字（SectionIndex）を符号付き32ビット整数として用意し、INDEX_NONE（未指定を表す-1）で初期化する。
    int32 SectionIndex = INDEX_NONE;
};

// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardLink
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 接続元の部屋ID（From）を符号付き32ビット整数として用意し、0で初期化する。
    int32 From = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 接続先の部屋ID（To）を符号付き32ビット整数として用意し、0で初期化する。
    int32 To = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 長手方向の軸（Axis）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Axis = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 床セルの一覧または索引（Cells）を配列として宣言する。
    TArray<FIntPoint> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 曲がりや分岐を許す細道かの判定（bAlley）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bAlley = false; // true: 曲がり・分岐を許す屋外細道。false: 直線トンネル。
    // 0:東、1:北、2:西、3:南。トンネルはこの一方向だけへ直進する。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 四方向の番号（Direction）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Direction = 0;
};

/** 道の曲がり角に置く小さな独立室。扉は外周のセルと向きで記録する。 */
// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardRoomArea
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の最小端（Min）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint Min = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 範囲の大きさ（Size）を2次元の整数セル座標として用意し、2次元の整数セル座標を組み立てる（引数：2、3）で初期化する。
    FIntPoint Size = FIntPoint(2, 3);
    // 外接矩形ではなく、この実床の輪郭に沿って壁を作る。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 実際の床セル一覧（FloorCells）を配列として宣言する。
    TArray<FIntPoint> FloorCells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 扉を開ける室内セル（DoorCells）を配列として宣言する。
    TArray<FIntPoint> DoorCells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 各扉の外向き方向（DoorDirections）を配列として宣言する。
    TArray<int32> DoorDirections;
};

// 構造体をUEへ登録し、保存やBPでのMake/Breakに使えるようにする。
USTRUCT(BlueprintType)

struct FCourtyardPlan
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 部屋の塊と枝分かれする細道の方式かの識別（bOrganicLayout）を真偽値として用意し、false（不成立・無効）で初期化する。
    bool bOrganicLayout = false; // 新しい部屋＋細道方式か。検査方法の切り替えに使う。
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 再現用Seed（Seed）を符号付き32ビット整数として用意し、0で初期化する。
    int32 Seed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // RoomAreas（RoomAreas）を配列として宣言する。
    TArray<FCourtyardRoomArea> RoomAreas;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 広い空間の一覧（Nodes）を配列として宣言する。
    TArray<FCourtyardNode> Nodes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // Links（Links）を配列として宣言する。
    TArray<FCourtyardLink> Links;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 床セルの一覧または索引（Cells）を配列として宣言する。
    TArray<FCourtyardCell> Cells;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // StartCell（StartCell）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint StartCell = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // GoalCell（GoalCell）を2次元の整数セル座標として用意し、FIntPoint::ZeroValueで初期化する。
    FIntPoint GoalCell = FIntPoint::ZeroValue;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 独立した周回路の件数（LoopCount）を符号付き32ビット整数として用意し、0で初期化する。
    int32 LoopCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
    // 採用する直線トンネルの最長セル数（TunnelCellLimit）を符号付き32ビット整数として用意し、2で初期化する。
    int32 TunnelCellLimit = 2;
};

/** データ生成とUE上の配置を分離する。BP版へ段階的に置き換えるための入口。 */
// クラスをUEへ登録し、指定に応じてBP継承やアセット参照に使えるようにする。
UCLASS()

class ESCAPE_ON_STEP_DEMO_API UCourtyardLayoutLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    /** 新方式の配置データを生成する。部屋・細道・トンネルを作り、接続検査まで行う。 */
    // この後の関数をUEへ登録する。実行順を指定できるBPノードとして公開。
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
    // この関数の役割：部屋の塊と枝分かれする細道の配置データを作る。ここでは呼び出し方を宣言する。
    static bool BuildOrganicCourtyardPlan(
        // 生成設定（Settings）、int32 Seed（int32 Seed）、FCourtyardPlan& Plan（FCourtyardPlan& Plan）、FString& Error)（FString& Error)）を、それぞれ迷路の生成設定として用意する。元の値を参照し、コピーしない。
        const FCourtyardSettings& Settings, int32 Seed, FCourtyardPlan& Plan, FString& Error);
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
    // この関数の役割：部屋と細道の接続・入口・分岐・周回条件を検査する。ここでは呼び出し方を宣言する。
    static bool ValidateOrganicCourtyardPlan(const FCourtyardPlan& Plan, FString& Error);
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
    // この関数の役割：見通しのよい短い周回を減らし、戻り道を残して接続を調整する。ここでは呼び出し方を宣言する。
    static void ImproveMazeConnections(const TArray<FCourtyardNode>& Nodes,
        // 接続や入口の候補（Candidates）を配列として引数で受け取る。元の値を参照し、コピーしない。
        const TArray<FCourtyardLink>& Candidates,
        // BPへ公開する引数を参照渡しとして扱う。呼び出し元の配列などを直接更新できる。
        UPARAM(ref) TArray<int32>& Selected,
        // BPへ公開する引数を参照渡しとして扱う。呼び出し元の配列などを直接更新できる。
        UPARAM(ref) TArray<int32>& Degrees,
        // 生成設定（Settings）を迷路の生成設定として引数で受け取る。元の値を参照し、コピーしない。
        const FCourtyardSettings& Settings,
        // 再現用Seed（Seed）を符号付き32ビット整数として引数で受け取る。
        int32 Seed);
    // この後の関数をUEへ登録する。実行ピンなしで値を返すBPノードとして公開。
    UFUNCTION(BlueprintPure, Category = "Dungeon|Courtyard")
    // この関数の役割：配置の整列を崩す、小さなセル位置のずれを求める。ここでは呼び出し方を宣言する。
    static FIntPoint GetStaggeredOrigin(
        // 配置または計測の基準位置（Origin）、int32 Id（int32 Id）、int32 Columns（int32 Columns）、int32 Seed（int32 Seed）、const FCourtyardSettings& Settings)（const FCourtyardSettings& Settings)）を、それぞれ2次元の整数セル座標として用意する。
        FIntPoint Origin, int32 Id, int32 Columns, int32 Seed, const FCourtyardSettings& Settings);
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
    // この関数の役割：従来方式の折れた空間とトンネルの配置データを作る。ここでは呼び出し方を宣言する。
    static bool BuildCourtyardPlan(
        // 生成設定（Settings）、int32 Seed（int32 Seed）、FCourtyardPlan& Plan（FCourtyardPlan& Plan）、FString& Error)（FString& Error)）を、それぞれ迷路の生成設定として用意する。元の値を参照し、コピーしない。
        const FCourtyardSettings& Settings, int32 Seed, FCourtyardPlan& Plan, FString& Error);
    UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
    // この関数の役割：従来方式の床・部屋形状・トンネル・接続条件を検査する。ここでは呼び出し方を宣言する。
    static bool ValidateCourtyardPlan(const FCourtyardPlan& Plan, FString& Error);
};
