#include "DungeonGoalRegions.h"
namespace DungeonGoalRegions
{
TArray<FCentre> CentralCandidates(const TArray<DungeonLightRegions::FCell>& Cells,
 const TArray<int32>& Labels,const TArray<DungeonLightRegions::FRegion>& Regions,double S)
{
 TArray<FCentre> Result;if(S<=0||!FMath::IsFinite(S)||Labels.Num()!=Cells.Num())return Result;
 const FVector Directions[]={FVector(1,0,0),FVector(0,1,0),FVector(-1,0,0),FVector(0,-1,0)};
 for(int32 Id=0;Id<Regions.Num();++Id)
 {
  const auto& Region=Regions[Id];if(Region.Cells.Num()<9)continue;
  FVector Centre=FVector::ZeroVector;TArray<TPair<FVector,FVector>> Boundary;TArray<FVector> Points;
  for(int32 I:Region.Cells)
  {
   const auto& C=Cells[I];Centre+=C.Position;Points.Add(C.Position);
   for(int32 D=0;D<4;++D)
   {
    const int32 N=C.Neighbors[D];
    if(N!=INDEX_NONE&&Labels[N]==Id)
    {
     if(D<2)Points.Add((C.Position+Cells[N].Position)*0.5);
     continue;
    }
    const auto Mid=C.Position+Directions[D]*(S*0.5);
    const auto Along=Directions[(D+1)%4]*(S*0.5);
    Boundary.Emplace(Mid-Along,Mid+Along);
   }
   // Include centres between four cells for even-width/even-height rooms.
   const int32 X=C.Neighbors[0],Y=C.Neighbors[1];
   if(X!=INDEX_NONE&&Y!=INDEX_NONE&&Labels[X]==Id&&Labels[Y]==Id)
   {const int32 XY=Cells[X].Neighbors[1];if(XY!=INDEX_NONE&&Labels[XY]==Id&&Cells[Y].Neighbors[0]==XY)Points.Add((C.Position+Cells[XY].Position)*0.5);}
  }
  Centre/=Region.Cells.Num();Points.Add(Centre);
  TArray<FCentre> Valid;double MaxClearance=0;
  for(auto P:Points)
  {
   // An arithmetic centroid can lie in a concavity or a hole. Require support.
   bool Inside=false;for(int32 I:Region.Cells)
    if(FMath::Abs(P.X-Cells[I].Position.X)<=S*0.5&&FMath::Abs(P.Y-Cells[I].Position.Y)<=S*0.5){Inside=true;break;}
   if(!Inside)continue;
   double ClearanceSquared=TNumericLimits<double>::Max();
   for(const auto& Segment:Boundary)
   {
    const FVector AB=Segment.Value-Segment.Key;
    const double T=FMath::Clamp(FVector::DotProduct(P-Segment.Key,AB)/AB.SizeSquared(),0.0,1.0);
    ClearanceSquared=FMath::Min(ClearanceSquared,FVector::DistSquared2D(P,Segment.Key+AB*T));
   }
   const double Clearance=FMath::Sqrt(ClearanceSquared);MaxClearance=FMath::Max(MaxClearance,Clearance);
   Valid.Add({P,Centre,Id,Region.Cells.Num(),Clearance});
  }
  // Never fall back to a wall-side point. If the middle is obstructed, use another space.
  Valid.RemoveAll([&](const FCentre& C){return C.Clearance<FMath::Max(S*1.25,MaxClearance*0.75)||FVector::DistSquared2D(C.Position,Centre)>FMath::Square(S*0.75);});
  Valid.Sort([&](const FCentre& A,const FCentre& B)
  {
   const double DA=FVector::DistSquared2D(A.Position,Centre),DB=FVector::DistSquared2D(B.Position,Centre);
   if(!FMath::IsNearlyEqual(DA,DB,0.001))return DA<DB;
   if(A.Clearance!=B.Clearance)return A.Clearance>B.Clearance;
   return A.Position.X!=B.Position.X?A.Position.X<B.Position.X:A.Position.Y<B.Position.Y;
  });
  TSet<FVector> Seen;for(const auto& C:Valid)if(!Seen.Contains(C.Position)){Seen.Add(C.Position);Result.Add(C);}
 }
 return Result;
}
}
