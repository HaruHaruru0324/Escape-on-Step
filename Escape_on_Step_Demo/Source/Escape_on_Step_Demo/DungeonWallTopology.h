#pragma once
#include "CoreMinimal.h"

// Edge (x,y,0) separates (x,y) and (x+1,y); edge (x,y,1) separates (x,y) and (x,y+1).
namespace DungeonWallTopology
{
 struct FPlan { TArray<FIntVector> Edges; int64 BeforeScore=0,AfterScore=0; };
 FIntVector Edge(const FIntPoint& A,const FIntPoint& B);
 int32 Components(const TSet<FIntPoint>& Floors,const TSet<FIntVector>& Blocked);
 int64 SightScore(const TSet<FIntPoint>& Floors,const TSet<FIntVector>& Blocked,int32 Limit);
 FPlan FindBaffle(const TSet<FIntPoint>& Floors,const TSet<FIntVector>& Blocked,
  const TSet<FIntVector>& Walls,int32 Limit,TFunctionRef<bool(const FIntVector&)> CanPlace);
}
