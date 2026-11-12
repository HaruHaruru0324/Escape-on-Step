#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardLayout.generated.h"

UENUM(BlueprintType)
enum class ECourtyardCellKind : uint8
{
	Street,
	Tunnel,
	Room
};

/** 後でBPへ移植する際も同じデータをBreak/Makeして扱える、セル単位の設定。 */
USTRUCT(BlueprintType)
struct FCourtyardSettings
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "9", ClampMax = "24"))
	int32 StreetCount = 12;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "2", ClampMax = "3"))
	int32 StreetWidth = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "4", ClampMax = "12"))
	int32 MinStreetLength = 6;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "4", ClampMax = "12"))
	int32 MaxStreetLength = 9;
	// 旧周回路版との保存互換用。現在の迷路タイルの寸法には使用しない。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MinBlockGap = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "8"))
	int32 MaxBlockGap = 3;
	// 広い道の延長と連結ルームを除いた、実際の狭いトンネルの長さ。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MinTunnelCells = 1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "1", ClampMax = "3"))
	int32 MaxTunnelCells = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MinRoomSections = 2;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "2", ClampMax = "4"))
	int32 MaxRoomSections = 4;
	// 最低限の分岐・周回路を保証した上で、残りの短い接続を追加する確率。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float ExtraConnectionChance = 0.75f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0", ClampMax = "1"))
	float RoomChance = 0.35f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout", meta = (ClampMin = "0", ClampMax = "6"))
	int32 MaxRooms = 3;
};

/** 広い道またはルームの1区画。隣とは直角に曲がり、端の幅×幅の角を共有する。 */
USTRUCT(BlueprintType)
struct FCourtyardRoomSection
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Min = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Size = FIntPoint(2, 2);
};

USTRUCT(BlueprintType)
struct FCourtyardNode
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 Id = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ECourtyardCellKind Kind = ECourtyardCellKind::Street;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Min = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Size = FIntPoint(1, 1);
	// Min/Sizeは外接矩形。折れた空間の実床はこの配列で表す。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FIntPoint> FloorCells;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FCourtyardRoomSection> Sections;
};

USTRUCT(BlueprintType)
struct FCourtyardCell
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Position = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout")
	ECourtyardCellKind Kind = ECourtyardCellKind::Street;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 NodeId = INDEX_NONE;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 LinkId = INDEX_NONE;
	// 0: X方向の通路、1: Y方向の通路。通路以外では使わない。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 Axis = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 SectionIndex = INDEX_NONE;
};

USTRUCT(BlueprintType)
struct FCourtyardLink
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 From = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 To = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 Axis = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FIntPoint> Cells;
	// 0:東、1:北、2:西、3:南。トンネルはこの一方向だけへ直進する。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 Direction = 0;
};

/** 道の曲がり角に置く小さな独立室。扉は外周のセルと向きで記録する。 */
USTRUCT(BlueprintType)
struct FCourtyardRoomArea
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Min = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint Size = FIntPoint(2,3);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FIntPoint> DoorCells;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<int32> DoorDirections;
};

USTRUCT(BlueprintType)
struct FCourtyardPlan
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 Seed = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FCourtyardRoomArea> RoomAreas;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FCourtyardNode> Nodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FCourtyardLink> Links;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") TArray<FCourtyardCell> Cells;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint StartCell = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") FIntPoint GoalCell = FIntPoint::ZeroValue;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 LoopCount = 0;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Layout") int32 TunnelCellLimit = 2;
};

/** データ生成とUE上の配置を分離する。BP版へ段階的に置き換えるための入口。 */
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UCourtyardLayoutLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
  public:
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
	static bool BuildCourtyardPlan(const FCourtyardSettings& Settings, int32 Seed, FCourtyardPlan& Plan,
								   FString& Error);
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Courtyard")
	static bool ValidateCourtyardPlan(const FCourtyardPlan& Plan, FString& Error);
};

