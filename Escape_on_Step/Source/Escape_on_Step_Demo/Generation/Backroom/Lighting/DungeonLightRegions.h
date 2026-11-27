#pragma once
#include "CoreMinimal.h"

// Pure grid topology, separate from physics so region behaviour can be tested.
namespace DungeonLightRegions
{
	struct FCell
	{
		FIntPoint Key;
		FVector Position;
		int32 Neighbors[4] = {INDEX_NONE, INDEX_NONE, INDEX_NONE, INDEX_NONE};
	};
	struct FRegion
	{
		TArray<int32> Cells;
		int32 Representative = INDEX_NONE;
	};
	void Partition(const TArray<FCell>& Cells, TArray<int32>& Labels, TArray<FRegion>& Regions);
}
