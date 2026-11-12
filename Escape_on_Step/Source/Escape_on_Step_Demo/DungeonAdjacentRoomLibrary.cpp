#include "DungeonAdjacentRoomLibrary.h"

#include "Components/CapsuleComponent.h"
#include "DungeonAdjacentLayout.h"
#include "DungeonGenerationRetrySubsystem.h"
#include "DungeonWallLayoutLibrary.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UnrealType.h"

namespace AdjacentAdapter
{
// BPのユーザー定義構造体にはGUIDの接尾辞が付くため、表示名の前方一致も認める。
FProperty* FindBlueprintField(UStruct* StructType, const FString& Name)
{
	for (TFieldIterator<FProperty> It(StructType); It; ++It)
	{
		if (It->GetName() == Name || It->GetName().StartsWith(Name + TEXT("_")))
		{
			return *It;
		}
	}
	return nullptr;
}

FNumericProperty* GetNumericProperty(FProperty* Property)
{
	if (auto* Enum = CastField<FEnumProperty>(Property))
	{
		return Enum->GetUnderlyingProperty();
	}
	return CastField<FNumericProperty>(Property);
}

int32 ReadIntegerProperty(AActor* Generator, const TCHAR* Name)
{
	FProperty* Property = FindBlueprintField(Generator->GetClass(), Name);
	FNumericProperty* Number = GetNumericProperty(Property);
	return Number ? int32(Number->GetSignedIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Generator))) : 0;
}

void WriteIntegerField(FProperty* Property, void* Data, int32 Value)
{
	GetNumericProperty(Property)->SetIntPropertyValue(Property->ContainerPtrToValuePtr<void>(Data), int64(Value));
}

// 既存BPの引数なし関数だけを呼ぶ。異なるシグネチャを誤って呼び出さない。
bool CallGeneratorFunction(AActor* Generator, const TCHAR* Name)
{
	UFunction* Function = Generator->FindFunction(Name);
	if (!Function || Function->ParmsSize)
	{
		return false;
	}
	Generator->ProcessEvent(Function, nullptr);
	return true;
}

bool ReportGenerationFailure(AActor* Generator, const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("ADJACENT_ROOMS failed: %s"), Reason);
	if (UGameInstance* GameInstance = Generator->GetWorld()->GetGameInstance())
	{
		if (auto* Retry = GameInstance->GetSubsystem<UDungeonGenerationRetrySubsystem>())
		{
			Retry->RequestRegeneration(Generator->GetWorld());
		}
	}
	return false;
}

// BP配列の型情報。実際の配列データはGeneratorが所有し、この構造体では保持しない。
struct FGeneratorSchema
{
	FArrayProperty* Grids = nullptr;
	FArrayProperty* Rooms = nullptr;
	FArrayProperty* RoomIds = nullptr;
	FArrayProperty* Centers = nullptr;
	FStructProperty* GridPosition = nullptr;
	FProperty* GridType = nullptr;
	FProperty* RoomX = nullptr;
	FProperty* RoomY = nullptr;
	FProperty* RoomWidth = nullptr;
	FProperty* RoomHeight = nullptr;
};

bool ReadGeneratorSchema(AActor* Generator, FGeneratorSchema& Schema, const TCHAR*& FailureReason)
{
	Schema.Grids = CastField<FArrayProperty>(FindBlueprintField(Generator->GetClass(), TEXT("Grids")));
	Schema.Rooms = CastField<FArrayProperty>(FindBlueprintField(Generator->GetClass(), TEXT("Rooms")));
	Schema.RoomIds = CastField<FArrayProperty>(FindBlueprintField(Generator->GetClass(), TEXT("Grid_room_ids")));
	Schema.Centers = CastField<FArrayProperty>(FindBlueprintField(Generator->GetClass(), TEXT("center_poses")));
	auto* GridStruct = Schema.Grids ? CastField<FStructProperty>(Schema.Grids->Inner) : nullptr;
	auto* RoomStruct = Schema.Rooms ? CastField<FStructProperty>(Schema.Rooms->Inner) : nullptr;
	auto* CenterStruct = Schema.Centers ? CastField<FStructProperty>(Schema.Centers->Inner) : nullptr;
	if (!GridStruct || !RoomStruct || !Schema.RoomIds || !GetNumericProperty(Schema.RoomIds->Inner) || !CenterStruct ||
		CenterStruct->Struct != TBaseStructure<FIntPoint>::Get())
	{
		FailureReason = TEXT("Array schema mismatch");
		return false;
	}

	Schema.GridPosition = CastField<FStructProperty>(FindBlueprintField(GridStruct->Struct, TEXT("Grid_Pos")));
	Schema.GridType = FindBlueprintField(GridStruct->Struct, TEXT("Grid_Type"));
	Schema.RoomX = FindBlueprintField(RoomStruct->Struct, TEXT("X"));
	Schema.RoomY = FindBlueprintField(RoomStruct->Struct, TEXT("Y"));
	Schema.RoomWidth = FindBlueprintField(RoomStruct->Struct, TEXT("Width"));
	Schema.RoomHeight = FindBlueprintField(RoomStruct->Struct, TEXT("Height"));
	if (!Schema.GridPosition || Schema.GridPosition->Struct != TBaseStructure<FIntPoint>::Get() ||
		!GetNumericProperty(Schema.GridType) || !GetNumericProperty(Schema.RoomX) ||
		!GetNumericProperty(Schema.RoomY) || !GetNumericProperty(Schema.RoomWidth) ||
		!GetNumericProperty(Schema.RoomHeight))
	{
		FailureReason = TEXT("Grid/Room fields mismatch");
		return false;
	}
	return true;
}

// L字の実床はGridsで表す。既存Rooms構造体には外接矩形と実床上の中心を渡す。
void WriteLayoutToGenerator(AActor* Generator, const FGeneratorSchema& Schema, const DungeonAdjacent::FLayout& Layout)
{
	FScriptArrayHelper Grids(Schema.Grids, Schema.Grids->ContainerPtrToValuePtr<void>(Generator));
	FScriptArrayHelper RoomIds(Schema.RoomIds, Schema.RoomIds->ContainerPtrToValuePtr<void>(Generator));
	RoomIds.Resize(Grids.Num());
	for (int32 Index = 0; Index < Grids.Num(); ++Index)
	{
		void* Data = Grids.GetRawPtr(Index);
		const FIntPoint Position = *Schema.GridPosition->ContainerPtrToValuePtr<FIntPoint>(Data);
		const int32* OwnerId = Layout.Cells.Find(Position);
		WriteIntegerField(Schema.GridType, Data, OwnerId ? 1 : 0);
		// 既存の部屋ID配列では、専用通路も空セルもINDEX_NONEとして扱う。
		GetNumericProperty(Schema.RoomIds->Inner)
			->SetIntPropertyValue(RoomIds.GetRawPtr(Index), int64(OwnerId && *OwnerId >= 0 ? *OwnerId : INDEX_NONE));
	}

	FScriptArrayHelper Rooms(Schema.Rooms, Schema.Rooms->ContainerPtrToValuePtr<void>(Generator));
	Rooms.EmptyValues();
	Rooms.AddValues(Layout.Rooms.Num());
	FScriptArrayHelper Centers(Schema.Centers, Schema.Centers->ContainerPtrToValuePtr<void>(Generator));
	Centers.EmptyValues();
	Centers.AddValues(Layout.Rooms.Num());
	for (int32 Index = 0; Index < Layout.Rooms.Num(); ++Index)
	{
		const auto& Room = Layout.Rooms[Index];
		void* Data = Rooms.GetRawPtr(Index);
		WriteIntegerField(Schema.RoomX, Data, Room.X);
		WriteIntegerField(Schema.RoomY, Data, Room.Y);
		WriteIntegerField(Schema.RoomWidth, Data, Room.W);
		WriteIntegerField(Schema.RoomHeight, Data, Room.H);
		*reinterpret_cast<FIntPoint*>(Centers.GetRawPtr(Index)) = Room.Center();
	}
	for (const TCHAR* Name : {TEXT("Door_edge_keys"), TEXT("Placed_Light_Positions")})
	{
		if (auto* Property = CastField<FArrayProperty>(FindBlueprintField(Generator->GetClass(), Name)))
		{
			FScriptArrayHelper Array(Property, Property->ContainerPtrToValuePtr<void>(Generator));
			Array.EmptyValues();
		}
	}
}

// 計画上のつながりに加え、生成した壁の実コリジョンで移動可能か検査する。
// 家具・敵の配置前に実行する。後から配置する家具は既存のゴール選択側でも判定される。
int32 CountPhysicallyReachableCells(AActor* Generator, const DungeonAdjacent::FLayout& Layout, FIntPoint Start,
									int32 GridSize, ACharacter* Player)
{
	UWorld* World = Generator->GetWorld();
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	FCollisionQueryParams Query(SCENE_QUERY_STAT(AdjacentRoomConnectivity), false, Generator);
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	const auto Capsule =
		FCollisionShape::MakeCapsule(Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 2, HalfHeight);
	const auto WorldPosition = [&](FIntPoint Cell)
	{ return FVector(Cell.X * GridSize, Cell.Y * GridSize, HalfHeight + 5.f); };

	TArray<FIntPoint> Queue;
	TSet<FIntPoint> Seen;
	Queue.Add(Start);
	Seen.Add(Queue[0]);
	const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
	for (int32 Index = 0; Index < Queue.Num(); ++Index)
	{
		for (FIntPoint Direction : Steps)
		{
			const FIntPoint From = Queue[Index], To = From + Direction;
			if (!Layout.Cells.Contains(To) || Seen.Contains(To))
			{
				continue;
			}
			if (!World->SweepTestByObjectType(WorldPosition(From), WorldPosition(To), FQuat::Identity, Objects, Capsule,
											  Query))
			{
				Seen.Add(To);
				Queue.Add(To);
			}
		}
	}
	return Seen.Num();
}
} // namespace AdjacentAdapter

bool UDungeonAdjacentRoomLibrary::GenerateAdjacentDungeon(AActor* Generator, int32 DesiredRooms, int32 Seed)
{
	using namespace AdjacentAdapter;
	if (!IsInGameThread() || !IsValid(Generator) || !Generator->GetWorld() || !Generator->GetWorld()->IsGameWorld())
	{
		return false;
	}
	if (Generator->ActorHasTag(TEXT("AdjacentRoomsGenerated")))
	{
		return true;
	}

	// 1. 既存BPの設定・配列型を確認し、初期グリッドから配置範囲を求める。
	const int32 GridSize = ReadIntegerProperty(Generator, TEXT("Grid_size"));
	if (GridSize < 100 || GridSize > 10000)
	{
		return ReportGenerationFailure(Generator, TEXT("Invalid Grid_size"));
	}
	auto* MeshProperty = CastField<FObjectPropertyBase>(FindBlueprintField(Generator->GetClass(), TEXT("WallBox")));
	auto* WallMesh =
		MeshProperty ? Cast<UStaticMesh>(MeshProperty->GetObjectPropertyValue_InContainer(Generator)) : nullptr;
	if (!WallMesh)
	{
		return ReportGenerationFailure(Generator, TEXT("WallBox mesh missing"));
	}
	if (!CallGeneratorFunction(Generator, TEXT("Init_Grid")))
	{
		return ReportGenerationFailure(Generator, TEXT("Init_Grid unavailable"));
	}
	FGeneratorSchema Schema;
	const TCHAR* FailureReason = nullptr;
	if (!ReadGeneratorSchema(Generator, Schema, FailureReason))
	{
		return ReportGenerationFailure(Generator, FailureReason);
	}
	FScriptArrayHelper Grids(Schema.Grids, Schema.Grids->ContainerPtrToValuePtr<void>(Generator));
	if (Grids.Num() < 64 || Grids.Num() > 16384)
	{
		return ReportGenerationFailure(Generator, TEXT("Unsupported grid bounds"));
	}
	FIntPoint Min(MAX_int32, MAX_int32), Max(MIN_int32, MIN_int32);
	for (int32 Index = 0; Index < Grids.Num(); ++Index)
	{
		const FIntPoint Position = *Schema.GridPosition->ContainerPtrToValuePtr<FIntPoint>(Grids.GetRawPtr(Index));
		Min.X = FMath::Min(Min.X, Position.X);
		Min.Y = FMath::Min(Min.Y, Position.Y);
		Max.X = FMath::Max(Max.X, Position.X);
		Max.Y = FMath::Max(Max.Y, Position.Y);
	}

	// 2. 純粋なグリッドモデルを生成する。旧BPの明示値14は標準設定へ読み替える。
	const int32 ActualSeed = Seed < 0 ? FMath::Rand() : Seed;
	const int32 RoomTarget = DesiredRooms == 14 ? 0 : DesiredRooms;
	const auto Layout = DungeonAdjacent::Build(Min, Max, RoomTarget, ActualSeed);
	if (Layout.Rooms.IsEmpty())
	{
		return ReportGenerationFailure(Generator, TEXT("Layout has no rooms"));
	}
	if (RoomTarget <= 0 && (Layout.Rooms.Num() < 24 || Layout.DeadEnds > 0))
	{
		return ReportGenerationFailure(Generator, TEXT("Layout could not provide 24 rooms with two exits each"));
	}

	// 3. 既存描画BPへデータを渡し、床・天井と、中心補正された壁を配置する。
	WriteLayoutToGenerator(Generator, Schema, Layout);
	Generator->Tags.AddUnique(TEXT("AdjacentRoomsGenerated"));
	if (!CallGeneratorFunction(Generator, TEXT("Generate_Floors")))
	{
		return ReportGenerationFailure(Generator, TEXT("Generate_Floors unavailable"));
	}
	auto Edges = Layout.Walls.Array();
	Edges.Sort([](auto A, auto B) { return A.X != B.X ? A.X < B.X : A.Y != B.Y ? A.Y < B.Y : A.Z < B.Z; });
	for (FIntVector Edge : Edges)
	{
		if (!UDungeonWallLayoutLibrary::SpawnAlignedDungeonWall(Generator, {Edge.X, Edge.Y}, Edge.Z == 0 ? 1 : 2,
																GridSize, WallMesh))
		{
			return ReportGenerationFailure(Generator, TEXT("Wall spawn failed"));
		}
	}
	// 壁と開口は間取りで確定済み。旧Refine処理による追加仕切りを抑止する。
	if (auto* Registry = Generator->FindComponentByClass<UDungeonWallRegistry>())
	{
		Registry->bRefined = true;
	}

	// 4. 開始部屋にプレイヤーを置き、カプセルを隣接セル間で動かして全床を検査する。
	auto* Player = UGameplayStatics::GetPlayerCharacter(Generator, 0);
	if (!Player)
	{
		return ReportGenerationFailure(Generator, TEXT("Player missing"));
	}
	const FIntPoint Start = Layout.Rooms[0].Center();
	const float HalfHeight = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector StartPosition(Start.X * GridSize, Start.Y * GridSize, HalfHeight + 5.f);
	if (!Player->SetActorLocation(StartPosition, false, nullptr, ETeleportType::TeleportPhysics))
	{
		return ReportGenerationFailure(Generator, TEXT("Player placement failed"));
	}
	const int32 ReachableCount = CountPhysicallyReachableCells(Generator, Layout, Start, GridSize, Player);
	if (ReachableCount != Layout.Cells.Num())
	{
		return ReportGenerationFailure(Generator, TEXT("Physical wall collision disconnects floor cells"));
	}
	UE_LOG(LogTemp, Display,
		   TEXT("ADJACENT_ROOMS seed=%d rooms=%d corridors=%d branches=%d corridorRooms=%d deadEnds=%d doors=%d "
				"loops=%d floors=%d reachable=%d walls=%d"),
		   ActualSeed, Layout.Rooms.Num(), Layout.Corridors, Layout.Branches, Layout.CorridorRooms, Layout.DeadEnds,
		   Layout.Doors.Num(), Layout.Doors.Num() - Layout.Rooms.Num() - Layout.Corridors + 1, Layout.Cells.Num(),
		   ReachableCount, Layout.Walls.Num());

	// 5. 敵・家具と既存照明処理を呼ぶ。照明処理内のゴール選択にも既存の干渉・到達検査がある。
	if (!CallGeneratorFunction(Generator, TEXT("Spawn_room_enemies")))
	{
		return ReportGenerationFailure(Generator, TEXT("Enemy function unavailable"));
	}
	if (!CallGeneratorFunction(Generator, TEXT("Generate_Corridor_Lights")))
	{
		return ReportGenerationFailure(Generator, TEXT("Lighting function unavailable"));
	}
	return true;
}
