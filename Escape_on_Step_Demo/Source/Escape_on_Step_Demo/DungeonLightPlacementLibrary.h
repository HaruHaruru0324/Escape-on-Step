#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DungeonLightPlacementLibrary.generated.h"

/** Position-only planning; never creates components or modifies the input arrays. */
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UDungeonLightPlacementLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** One light per open floor region. Run after ALL wall/route changes, before spawning lights.
	 * Uses WorldStatic/WorldDynamic collision at wall height; excludes pawns.
	 * A 3x3 open core joins broad openings; 1-2 cell necks separate cores.
	 * CandidateRegionIds aligns with input (-1 for obstructed/invalid cells).
	 * Radii aligns with returned positions. Existing lights suppress their own region only.
	 * Single, axis-aligned floor lattice; max 16384 candidates and 256 new lights.
	 */
	UFUNCTION(BlueprintCallable, Category="Dungeon|Lighting", meta=(WorldContext="WorldContextObject"))
	static TArray<FVector> PlanRegionLightPositions(
		const UObject* WorldContextObject,
		const TArray<FVector>& CandidateWorldPositions,
		const TArray<FVector>& ExistingLightWorldPositions,
		double GridSize,
		TArray<int32>& CandidateRegionIds,
		TArray<double>& AttenuationRadii,
		bool& bLimitReached,
		int32 MaxNewLights = 256);

	/**
	 * Greedy farthest-point sampling in world XY. Returned Z values are preserved.
	 * MaxNewLights counts NEW corridor lights, excluding ExistingLightWorldPositions.
	 * Exact XY duplicates are collapsed (lowest Z wins); existing XY positions are excluded.
	 * Ties, including the first choice with no existing lights, use ascending X/Y/Z.
	 * MaxNewLights is capped at 256. The evaluation budget bounds distance calculations;
	 * budget exhaustion returns a valid prefix (or empty if initialization cannot finish).
	 * Negative spacing becomes zero. Non-finite positions are ignored; invalid spacing
	 * (non-finite or too large to square) returns empty. No visibility/collision tests.
	 */
	UFUNCTION(BlueprintCallable, Category = "Dungeon|Lighting",
		meta = (DisplayName = "Plan Corridor Light Positions", AdvancedDisplay = "MaxDistanceEvaluations"))
	static TArray<FVector> PlanCorridorLightPositions(
		const TArray<FVector>& CandidateWorldPositions,
		const TArray<FVector>& ExistingLightWorldPositions,
		double MinLightDistance,
		int32 MaxNewLights,
		bool& bBudgetExhausted,
		int64 MaxDistanceEvaluations = 2000000);
};
