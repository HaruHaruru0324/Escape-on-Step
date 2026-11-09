#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonWallLayoutLibrary.generated.h"
class AStaticMeshActor;
class UStaticMesh;

/** Transient ownership/edge registry; no tick, no global state, no saved map data. */
UCLASS(Transient)
class UDungeonWallRegistry : public UActorComponent
{
 GENERATED_BODY()
public:
 TMap<FIntVector,TWeakObjectPtr<AStaticMeshActor>> Walls;
 bool bRefined=false;
};

UCLASS()
class ESCAPE_ON_STEP_DEMO_API UDungeonWallLayoutLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Canonical grid edge, centred mesh pivot, corner overlap and duplicate prevention. */
 UFUNCTION(BlueprintCallable,Category="Dungeon|Walls",meta=(DefaultToSelf="Generator"))
 static AStaticMeshActor* SpawnAlignedDungeonWall(AActor* Generator,FIntPoint GridPosition,int32 WallDirection,int32 GridSize,UStaticMesh* Mesh,float WallThickness=20.0f);

 /** Run after route/goal geometry and before lights. Only adds wall-attached baffles when
  * the current floor-cell connectivity is preserved. It cannot repair pre-existing disconnections.
  * Floor positions use the same world XY lattice as Generate_Floors (origin 0).
  */
 UFUNCTION(BlueprintCallable,Category="Dungeon|Walls",meta=(DefaultToSelf="Generator"))
 static void RefineDungeonWallLayout(AActor* Generator,const TArray<FVector>& FloorWorldPositions,int32 GridSize,
  int32& RemovedStubs,int32& AddedBaffles,int32 MaxBaffles=8,int32 MaxVisibleCells=7);
};
