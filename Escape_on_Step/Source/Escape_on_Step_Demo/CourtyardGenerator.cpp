#include "CourtyardGenerator.h"
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
const FIntPoint Steps[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};
}

ACourtyardGenerator::ACourtyardGenerator()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}
void ACourtyardGenerator::BeginPlay()
{
	Super::BeginPlay();
	if (bGenerateOnBeginPlay)
	{
		GenerateCourtyard();
	}
}
void ACourtyardGenerator::EndPlay(const EEndPlayReason::Type Reason)
{
	GetWorldTimerManager().ClearTimer(LoadingTimer);
	SetPlayerFrozen(false);
	Super::EndPlay(Reason);
}
FVector ACourtyardGenerator::GetCellWorldPosition(FIntPoint Cell, float Height) const
{
	return GetActorTransform().TransformPosition(FVector(Cell.X * CellSize, Cell.Y * CellSize, Height));
}

void ACourtyardGenerator::SetPlayerFrozen(bool Frozen)
{
	if (Frozen)
	{
		if (FrozenPlayer.IsValid())
		{
			return;
		}
		if (auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0))
		{
			FrozenPlayer = Player;
			PreviousMovementMode = uint8(Player->GetCharacterMovement()->MovementMode);
			Player->GetCharacterMovement()->DisableMovement();
		}
	}
	else if (auto* Player = FrozenPlayer.Get())
	{
		Player->GetCharacterMovement()->SetMovementMode(EMovementMode(PreviousMovementMode));
		FrozenPlayer.Reset();
	}
}
bool ACourtyardGenerator::Fail(const FString& Reason)
{
	GenerationError = Reason;
	bGenerationFailed = true;
	bGenerationComplete = false;
	bRendering = false;
	GetWorldTimerManager().ClearTimer(LoadingTimer);
	SetPlayerFrozen(false);
	UE_LOG(LogTemp, Error, TEXT("COURTYARD_GENERATION failed: %s"), *Reason);
	return false;
}
void ACourtyardGenerator::ClearCourtyard()
{
	GetWorldTimerManager().ClearTimer(LoadingTimer);
	SetPlayerFrozen(false);
	for (UStaticMeshComponent* Component : GeneratedMeshes)
	{
		if (IsValid(Component))
		{
			Component->DestroyComponent();
		}
	}
	GeneratedMeshes.Reset();
	for (AActor* Actor : GeneratedActors)
	{
		if (IsValid(Actor))
		{
			Actor->Destroy();
		}
	}
	GeneratedActors.Reset();
	for (ULevelStreamingDynamic* Module : LoadedModules)
	{
		if (IsValid(Module))
		{
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
bool ACourtyardGenerator::GenerateCourtyard()
{
	if (!GetWorld() || !GetWorld()->IsGameWorld())
	{
		return false;
	}
	if (bRendering)
	{
		return false;
	}
	ActualSeed = Seed < 0 ? FMath::Rand() : Seed;
	FCourtyardPlan NewPlan;
	FString Error;
	if (!UCourtyardLayoutLibrary::BuildCourtyardPlan(Settings, ActualSeed, NewPlan, Error))
	{
		return Fail(Error);
	}
	return RenderCourtyardPlan(NewPlan);
}

void ACourtyardGenerator::PlanModules()
{
	ModulePlacements.Reset();
	TMap<FIntPoint, const FCourtyardCell*> Cells;
	for (const auto& C : Plan.Cells)
	{
		Cells.Add(C.Position, &C);
	}
	FRandomStream Random(Plan.Seed ^ 0x31f7);
	const float Scale = CellSize / 400.f;
	auto AddModule = [&](TSoftObjectPtr<UWorld> Level, FVector Local, float Yaw, bool Passage = false,
						 bool HideStart = false, bool HideEnd = false)
	{
		FCourtyardModulePlacement P;
		P.Level = Level;
		P.Transform = FTransform(FRotator(0, Yaw, 0), Local, FVector(Scale)) * GetActorTransform();
		P.bPassage = Passage;
		P.bHideStartArch = HideStart;
		P.bHideEndArch = HideEnd;
		ModulePlacements.Add(P);
	};
	// 外壁のローカル範囲はX=0、Y=-400～0。建物側が+X、道側が-Xになるよう回す。
	auto Boundary = [&](FIntPoint Cell, int32 Direction, TSoftObjectPtr<UWorld> Level)
	{
		const float Yaw = Direction * 90.f;
		const FVector Center(Cell.X * CellSize + Steps[Direction].X * CellSize * .5f,
							 Cell.Y * CellSize + Steps[Direction].Y * CellSize * .5f, 0);
		AddModule(Level, Center - FRotator(0, Yaw, 0).RotateVector(FVector(0, -CellSize * .5f, 0)), Yaw);
	};
	for (const auto& C : Plan.Cells)
	{
		if (C.Kind == ECourtyardCellKind::Tunnel)
		{
			const auto& Link = Plan.Links[C.LinkId];
			const bool First = C.Position == Link.Cells[0], Last = C.Position == Link.Cells.Last();
			const bool RoadStart =
				Cells.FindChecked(Link.Cells[0] - Steps[Link.Direction])->Kind == ECourtyardCellKind::Street;
			const bool RoadEnd =
				Cells.FindChecked(Link.Cells.Last() + Steps[Link.Direction])->Kind == ECourtyardCellKind::Street;
			const int32 Direction = Link.Direction;
			const float Yaw = Direction * 90.f;
			const FVector Center(C.Position.X * CellSize, C.Position.Y * CellSize, 0);
			AddModule(Theme->Passage,
					  Center - FRotator(0, Yaw, 0).RotateVector(FVector(CellSize * .5f, -CellSize * .5f, 0)), Yaw, true,
					  !First || RoadStart, Last && RoadEnd);
			if (First && RoadStart)
			{
				Boundary(C.Position - Steps[Direction], Direction,
						 Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)]);
			}
			if (Last && RoadEnd)
			{
				Boundary(C.Position + Steps[Direction], (Direction + 2) % 4,
						 Theme->Portals[Random.RandRange(0, Theme->Portals.Num() - 1)]);
			}
		}
		else if (C.Kind != ECourtyardCellKind::Room)
		{
			for (int32 D = 0; D < 4; ++D)
			{
				if (Cells.Contains(C.Position + Steps[D]))
				{
					continue;
				}
				const auto Level = C.Kind == ECourtyardCellKind::Room
									   ? Theme->RoomFacade
									   : Theme->Facades[Random.RandRange(0, Theme->Facades.Num() - 1)];
				Boundary(C.Position, D, Level);
			}
		}
	} // 小部屋の境界は高さ3.5mの壁。出入口は幅1.8m・高さ2.6mにする。
	const auto Box = [&](FVector Center, FVector Size)
	{
		const auto Bounds = Theme->FloorMesh->GetBounds();
		FVector MeshScale(Size.X / (Bounds.BoxExtent.X * 2), Size.Y / (Bounds.BoxExtent.Y * 2),
						  Size.Z / (Bounds.BoxExtent.Z * 2));
		auto* C = NewObject<UStaticMeshComponent>(this);
		C->SetMobility(EComponentMobility::Movable);
		C->SetStaticMesh(Theme->FloorMesh);
		C->SetMaterial(0, Theme->CeilingMaterial);
		C->SetCollisionProfileName(TEXT("BlockAll"));
		C->SetupAttachment(RootComponent);
		C->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * MeshScale, MeshScale));
		C->RegisterComponent();
		GeneratedMeshes.Add(C);
	};
	const float Height = 350.f * Scale, DoorHeight = 260.f * Scale, Opening = 180.f * Scale, Thickness = 16.f * Scale;
	for (const auto& Room : Plan.RoomAreas)
	{
		FCourtyardRoomSection R;
		R.Min = Room.Min;
		R.Size = Room.Size;
		for (int32 X = 0; X < R.Size.X; ++X)
		{
			for (int32 Y = 0; Y < R.Size.Y; ++Y)
			{
				for (int32 D = 0; D < 4; ++D)
				{
					const auto P = R.Min + FIntPoint(X, Y), Q = P + Steps[D];
					if (Q.X >= R.Min.X && Q.Y >= R.Min.Y && Q.X < R.Min.X + R.Size.X && Q.Y < R.Min.Y + R.Size.Y)
					{
						continue;
					}
					bool Door = false;
					for (int32 I = 0; I < Room.DoorCells.Num(); ++I)
					{
						Door |= Room.DoorCells[I] == P && Room.DoorDirections[I] == D;
					}
					const FVector Center(P.X * CellSize + Steps[D].X * CellSize * .5f,
										 P.Y * CellSize + Steps[D].Y * CellSize * .5f, 0);
					const FVector Tangent(-Steps[D].Y, Steps[D].X, 0);
					const auto Wall = [&](float Offset, float Length, float Z, float H)
					{
						FVector Size = D % 2 == 0 ? FVector(Thickness, Length, H) : FVector(Length, Thickness, H);
						Box(Center + Tangent * Offset + FVector(0, 0, Z), Size);
					};
					if (!Door)
					{
						Wall(0, CellSize, Height * .5f, Height);
					}
					else
					{
						const float Side = (CellSize - Opening) * .5f;
						Wall(-(Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
						Wall((Opening + Side) * .5f, Side, DoorHeight * .5f, DoorHeight);
						Wall(0, CellSize, (Height + DoorHeight) * .5f, Height - DoorHeight);
					}
				}
			}
		}
	}
}

void ACourtyardGenerator::AddSurface(FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling)
{
	UStaticMesh* Mesh = Theme->FloorMesh;
	const auto Bounds = Mesh->GetBounds();
	if (Bounds.BoxExtent.X < .1 || Bounds.BoxExtent.Y < .1 || Bounds.BoxExtent.Z < .1)
	{
		return;
	}
	// メッシュ原点に依存せず、上面を床Z=0へ合わせる。天井は厚さ20cmの板とする。
	const float Depth = Ceiling ? 20.f : 12.f;
	const FVector Scale(CellSize / (Bounds.BoxExtent.X * 2), CellSize / (Bounds.BoxExtent.Y * 2),
						Depth / (Bounds.BoxExtent.Z * 2));
	const FVector Center(Cell.X * CellSize, Cell.Y * CellSize, Height - Depth * .5f);
	auto* Component = NewObject<UStaticMeshComponent>(this);
	Component->SetMobility(EComponentMobility::Movable);
	Component->SetStaticMesh(Mesh);
	if (Material)
	{
		Component->SetMaterial(0, Material);
	}
	Component->SetCollisionProfileName(TEXT("BlockAll"));
	Component->SetupAttachment(RootComponent);
	Component->SetRelativeTransform(FTransform(FRotator::ZeroRotator, Center - Bounds.Origin * Scale, Scale));
	Component->RegisterComponent();
	GeneratedMeshes.Add(Component);
}
bool ACourtyardGenerator::RenderCourtyardPlan(const FCourtyardPlan& NewPlan)
{
	if (!GetWorld() || !GetWorld()->IsGameWorld() || bRendering)
	{
		return false;
	}
	FString Error;
	if (!UCourtyardLayoutLibrary::ValidateCourtyardPlan(NewPlan, Error))
	{
		return Fail(Error);
	}
	if (!Theme || !Theme->FloorMesh || Theme->Facades.IsEmpty() || Theme->Portals.IsEmpty() ||
		Theme->Passage.IsNull() || Theme->RoomFacade.IsNull())
	{
		return Fail(TEXT("Courtyard theme is incomplete"));
	}
	if (!FMath::IsFinite(CellSize) || CellSize < 200 || CellSize > 800)
	{
		return Fail(TEXT("CellSize must be 200..800 cm"));
	}
	// 旧ストリーミングの除去は同期して終わらせ、次の物理検査へ残さない。
	ClearCourtyard();
	GetWorld()->FlushLevelStreaming(EFlushLevelStreamingType::Full);
	Plan = NewPlan;
	ActualSeed = Plan.Seed;
	bRendering = true;
	++GenerationSerial;
	SetPlayerFrozen(true);
	for (const auto& C : Plan.Cells)
	{
		AddSurface(C.Position, 0, C.Kind == ECourtyardCellKind::Street ? Theme->StreetMaterial : Theme->RoomMaterial);
		if (C.Kind == ECourtyardCellKind::Room)
		{
			AddSurface(C.Position, 350.f * CellSize / 400.f, Theme->CeilingMaterial, true);
		}
	}
	PlanModules();
	for (int32 I = 0; I < ModulePlacements.Num(); ++I)
	{
		const auto& P = ModulePlacements[I];
		bool Success = false;
		const FString Name = FString::Printf(TEXT("Courtyard_%s_%d_%d"), *GetName(), GenerationSerial, I);
		auto* Module =
			ULevelStreamingDynamic::LoadLevelInstanceBySoftObjectPtr(this, P.Level, P.Transform, Success, Name);
		if (!Success || !Module)
		{
			return Fail(TEXT("Failed to load module: ") + P.Level.ToString());
		}
		LoadedModules.Add(Module);
	}
	LoadingStarted = FPlatformTime::Seconds();
	GetWorldTimerManager().SetTimer(LoadingTimer, this, &ACourtyardGenerator::PollModules, .05f, true);
	return true;
}

void ACourtyardGenerator::PollModules()
{
	// BeginPlay時にPawnが未生成でも、見つかったフレームから移動を止める。
	SetPlayerFrozen(true);
	if (FPlatformTime::Seconds() - LoadingStarted > 90)
	{
		return (void)Fail(TEXT("Courtyard modules timed out"));
	}
	bool AllVisible = true;
	for (int32 I = 0; I < LoadedModules.Num(); ++I)
	{
		ULevelStreamingDynamic* Module = LoadedModules[I];
		if (!Module || !Module->IsLevelLoaded() || !Module->IsLevelVisible())
		{
			AllVisible = false;
			continue;
		}
		if (PreparedModules.Contains(I))
		{
			continue;
		}
		const auto& P = ModulePlacements[I];
		// 通路を連結した境界でアーチを二重に重ねない。元アセットは編集せず、このインスタンスだけ除外する。
		if (P.bPassage)
		{
			for (AActor* Actor : Module->GetLoadedLevel()->Actors)
			{
				if (!IsValid(Actor))
				{
					continue;
				}
				TInlineComponentArray<UStaticMeshComponent*> Components;
				Actor->GetComponents(Components);
				for (auto* C : Components)
				{
					if (!C->GetStaticMesh() || C->GetStaticMesh()->GetName() != TEXT("SM_archT1"))
					{
						continue;
					}
					const FVector Local = P.Transform.InverseTransformPosition(Actor->GetActorLocation());
					if ((P.bHideStartArch && FMath::Abs(Local.X) < 2) ||
						(P.bHideEndArch && FMath::Abs(Local.X - 400) < 2))
					{
						Actor->SetActorHiddenInGame(true);
						Actor->SetActorEnableCollision(false);
					}
				}
			}
		}
		PreparedModules.Add(I);
	}
	if (!AllVisible)
	{
		return;
	}
	GetWorldTimerManager().ClearTimer(LoadingTimer);
	if (!ValidatePhysicalLayout())
	{
		return;
	}
	bRendering = false;
	bGenerationComplete = true;
	SetPlayerFrozen(false);
	UE_LOG(LogTemp, Display,
		   TEXT("COURTYARD_READY seed=%d nodes=%d links=%d loops=%d floors=%d reachable=%d modules=%d"), ActualSeed,
		   Plan.Nodes.Num(), Plan.Links.Num(), Plan.LoopCount, Plan.Cells.Num(), PhysicallyReachableCells,
		   LoadedModules.Num());
	OnCourtyardReady();
}

bool ACourtyardGenerator::ValidatePhysicalLayout()
{
	auto* Player = UGameplayStatics::GetPlayerCharacter(this, 0);
	if (!Player)
	{
		return Fail(TEXT("Player character missing"));
	}
	const float Half = Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const float Radius = Player->GetCapsuleComponent()->GetScaledCapsuleRadius() + 3;
	const FCollisionShape Capsule = FCollisionShape::MakeCapsule(Radius, Half);
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic);
	Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Query(SCENE_QUERY_STAT(CourtyardWalkability), false);
	for (TActorIterator<APawn> It(GetWorld()); It; ++It)
	{
		Query.AddIgnoredActor(*It);
	}
	TSet<FIntPoint> Floors;
	for (const auto& C : Plan.Cells)
	{
		Floors.Add(C.Position);
	}
	const auto At = [&](FIntPoint P) { return GetCellWorldPosition(P, Half + 5); };
	if (GetWorld()->OverlapAnyTestByObjectType(At(Plan.StartCell), FQuat::Identity, Objects, Capsule, Query))
	{
		return Fail(TEXT("Player start is blocked"));
	}
	Player->SetActorLocation(At(Plan.StartCell), false, nullptr, ETeleportType::TeleportPhysics);
	if (auto* Controller = Player->GetController())
	{
		Controller->SetControlRotation(GetActorRotation());
	}
	TSet<FIntPoint> Seen{Plan.StartCell};
	TArray<FIntPoint> Queue{Plan.StartCell};
	for (int32 I = 0; I < Queue.Num(); ++I)
	{
		for (auto D : Steps)
		{
			const auto Next = Queue[I] + D;
			if (!Floors.Contains(Next) || Seen.Contains(Next))
			{
				continue;
			}
			if (!GetWorld()->SweepTestByObjectType(At(Queue[I]), At(Next), FQuat::Identity, Objects, Capsule, Query))
			{
				Seen.Add(Next);
				Queue.Add(Next);
			}
		}
	}
	PhysicallyReachableCells = Seen.Num();
	if (Seen.Num() != Floors.Num())
	{
		return Fail(
			FString::Printf(TEXT("Module collision disconnects floor cells (%d/%d)"), Seen.Num(), Floors.Num()));
	}
	for (auto P : Floors)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, GetCellWorldPosition(P, 60), GetCellWorldPosition(P, -60),
													 Objects, Query))
		{
			return Fail(TEXT("Floor support missing"));
		}
	}
	if (GoalClass)
	{
		if (GetWorld()->OverlapAnyTestByObjectType(At(Plan.GoalCell), FQuat::Identity, Objects, Capsule, Query))
		{
			return Fail(TEXT("Goal position blocked"));
		}
		FActorSpawnParameters Params;
		Params.Owner = this;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		auto* Goal =
			GetWorld()->SpawnActor<AActor>(GoalClass, GetCellWorldPosition(Plan.GoalCell), GetActorRotation(), Params);
		if (!Goal)
		{
			return Fail(TEXT("Goal spawn failed"));
		}
		GeneratedActors.Add(Goal);
	}
	return true;
}
