#pragma once
#include "DungeonLightRegions.h"
namespace DungeonGoalRegions
{
 struct FCentre
 {
  FVector Position,Centroid;
  int32 Region=INDEX_NONE,Area=0;
  double Clearance=0;
 };
 // Uses completed physical topology, not original room rectangles. Result order is
 // centroid first, then nearby positions with substantial wall clearance.
 TArray<FCentre> CentralCandidates(const TArray<DungeonLightRegions::FCell>& Cells,
  const TArray<int32>& Labels,const TArray<DungeonLightRegions::FRegion>& Regions,double GridSize);
}
