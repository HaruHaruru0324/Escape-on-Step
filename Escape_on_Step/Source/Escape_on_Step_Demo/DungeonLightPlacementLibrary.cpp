#include "DungeonLightPlacementLibrary.h"

#include <limits>

namespace DungeonLightPlacement
{
	bool IsFinite(const FVector& P)
	{
		return FMath::IsFinite(P.X) && FMath::IsFinite(P.Y) && FMath::IsFinite(P.Z);
	}

	double DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const double DX = A.X - B.X;
		const double DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}
}

TArray<FVector> UDungeonLightPlacementLibrary::PlanCorridorLightPositions(
	const TArray<FVector>& CandidateWorldPositions,
	const TArray<FVector>& ExistingLightWorldPositions,
	double MinLightDistance, int32 MaxNewLights, bool& bBudgetExhausted,
	int64 MaxDistanceEvaluations)
{
	using namespace DungeonLightPlacement;
	TArray<FVector> Result;
	bBudgetExhausted = false;
	const int32 Limit = FMath::Clamp(MaxNewLights, 0, 256);
	if (Limit == 0 || CandidateWorldPositions.IsEmpty() || !FMath::IsFinite(MinLightDistance))
	{
		return Result;
	}
	const double Spacing = FMath::Max(0.0, MinLightDistance);
	const double MinDistanceSquared = Spacing * Spacing;
	if (!FMath::IsFinite(MinDistanceSquared))
	{
		return Result;
	}

	TArray<FVector> Candidates;
	for (const FVector& P : CandidateWorldPositions)
	{
		if (IsFinite(P)) { Candidates.Add(P); }
	}
	Candidates.Sort([](const FVector& A, const FVector& B)
	{
		if (A.X != B.X) { return A.X < B.X; }
		if (A.Y != B.Y) { return A.Y < B.Y; }
		return A.Z < B.Z;
	});
	int32 UniqueCount = 0;
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		if (UniqueCount == 0 || Candidates[I].X != Candidates[UniqueCount - 1].X
			|| Candidates[I].Y != Candidates[UniqueCount - 1].Y)
		{
			Candidates[UniqueCount++] = Candidates[I];
		}
	}
	Candidates.SetNum(UniqueCount, EAllowShrinking::No);
	if (Candidates.IsEmpty()) { return Result; }

	TArray<FVector> Existing;
	for (const FVector& P : ExistingLightWorldPositions)
	{
		if (IsFinite(P)) { Existing.Add(P); }
	}
	int64 RemainingBudget = FMath::Max<int64>(0, MaxDistanceEvaluations);
	const int64 InitialCost = int64(Candidates.Num()) * int64(Existing.Num());
	if (InitialCost > RemainingBudget)
	{
		bBudgetExhausted = true;
		return Result;
	}
	RemainingBudget -= InitialCost;
	TArray<double> NearestSquared;
	NearestSquared.Init(std::numeric_limits<double>::infinity(), Candidates.Num());
	for (int32 I = 0; I < Candidates.Num(); ++I)
	{
		for (const FVector& Light : Existing)
		{
			NearestSquared[I] = FMath::Min(NearestSquared[I], DistanceSquaredXY(Candidates[I], Light));
		}
	}

	Result.Reserve(FMath::Min(Limit, Candidates.Num()));
	for (int32 Iteration = 0; Iteration < Limit; ++Iteration)
	{
		int32 BestIndex = INDEX_NONE;
		double BestDistanceSquared = -1.0;
		for (int32 I = 0; I < Candidates.Num(); ++I)
		{
			// Zero also excludes existing/coincident positions when spacing is zero.
			if (NearestSquared[I] > 0.0 && NearestSquared[I] > BestDistanceSquared)
			{
				BestDistanceSquared = NearestSquared[I];
				BestIndex = I;
			}
		}
		if (BestIndex == INDEX_NONE || BestDistanceSquared < MinDistanceSquared) { break; }
		Result.Add(Candidates[BestIndex]);
		NearestSquared[BestIndex] = -1.0; // Permanently selected, never reconsidered.
		if (Result.Num() == Limit) { break; }

		int64 UpdateCost = 0;
		for (double D : NearestSquared)
		{
			if (D > 0.0 && D >= MinDistanceSquared) { ++UpdateCost; }
		}
		if (UpdateCost == 0) { break; }
		if (UpdateCost > RemainingBudget)
		{
			bBudgetExhausted = true;
			break; // Never select using a partially updated cache.
		}
		RemainingBudget -= UpdateCost;
		for (int32 I = 0; I < Candidates.Num(); ++I)
		{
			// Distances only decrease: already-rejected candidates need no more work.
			if (NearestSquared[I] > 0.0 && NearestSquared[I] >= MinDistanceSquared)
			{
				NearestSquared[I] = FMath::Min(NearestSquared[I],
					DistanceSquaredXY(Candidates[I], Candidates[BestIndex]));
			}
		}
	}
	return Result;
}
