#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonGoalPlacementLibrary.generated.h"

class UStaticMesh;

UCLASS()
class ESCAPE_ON_STEP_DEMO_API UDungeonGoalPlacementLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 /** Adapter for BP_Dugeon_Generator Grids/ST_Grid; run after all wall changes.
  * Finds a clear central position in a relatively large reachable physical space.
  * SelectedRoomIndex is now the reconstructed space index, not an original Rooms index.
  * Uses the generator's existing world-origin XY grid, without modifying geometry.
  * Failure schedules a fresh level load (at most five retries per generation).
  */
 UFUNCTION(BlueprintCallable,Category="Dungeon|Goal",meta=(DefaultToSelf="Generator"))
 static bool SelectLargeRoomGoalLocation(AActor* Generator,UStaticMesh* TableMesh,
  FVector& GoalBaseLocation,int32& SelectedRoomIndex,int32& SelectedRoomArea);
};
