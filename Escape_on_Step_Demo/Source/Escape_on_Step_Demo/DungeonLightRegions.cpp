#include "DungeonLightRegions.h"
#include "DungeonLightPlacementLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

namespace DungeonLightRegions
{
	static const FIntPoint Directions[] = {{1,0},{0,1},{-1,0},{0,-1}};

	void Partition(const TArray<FCell>& Cells, TArray<int32>& Labels, TArray<FRegion>& Regions)
	{
		Labels.Init(INDEX_NONE, Cells.Num());
		Regions.Reset();
		TMap<FIntPoint, int32> Lookup;
		for (int32 I=0; I<Cells.Num(); ++I) Lookup.Add(Cells[I].Key,I);
		TArray<bool> Core;
		Core.Init(false,Cells.Num());
		for (int32 I=0; I<Cells.Num(); ++I)
		{
			bool Open=true;
			for (int32 X=-1; X<=1 && Open; ++X) for (int32 Y=-1; Y<=1 && Open; ++Y)
			{
				const int32* J=Lookup.Find(Cells[I].Key+FIntPoint(X,Y));
				if (!J) { Open=false; break; }
				// Test every internal edge in this 3x3, not just the nine cell centres.
				if ((X<1 && Cells[*J].Neighbors[0]==INDEX_NONE) ||
					(Y<1 && Cells[*J].Neighbors[1]==INDEX_NONE)) Open=false;
			}
			Core[I]=Open;
		}
		// Connected broad interiors are one space even if original room IDs differ.
		for (int32 I=0; I<Cells.Num(); ++I)
		{
			if (!Core[I] || Labels[I]!=INDEX_NONE) continue;
			const int32 Id=Regions.AddDefaulted();
			TArray<int32> Queue{I}; Labels[I]=Id;
			for (int32 Head=0; Head<Queue.Num(); ++Head)
				for (int32 J:Cells[Queue[Head]].Neighbors)
					if (J!=INDEX_NONE && Core[J] && Labels[J]==INDEX_NONE) { Labels[J]=Id; Queue.Add(J); }
		}
		// Restore a one-cell border (including corners), without crossing blocked edges.
		// Do not merge distinct cores when their restored borders touch at a doorway.
		for (int32 I=0; I<Cells.Num(); ++I)
		{
			if (!Core[I]) continue;
			TArray<int32> Queue{I};
			for (int32 Head=0; Head<Queue.Num(); ++Head)
			{
				for (int32 J:Cells[Queue[Head]].Neighbors)
				{
					if (J==INDEX_NONE || Queue.Contains(J)) continue;
					const FIntPoint Offset=Cells[J].Key-Cells[I].Key;
					if (FMath::Abs(Offset.X)>1 || FMath::Abs(Offset.Y)>1) continue;
					if (Labels[J]!=INDEX_NONE && Labels[J]!=Labels[I]) continue;
					Labels[J]=Labels[I]; Queue.Add(J);
				}
			}
		}
		// Narrow corridors and small rooms with no broad core also get one light each.
		for (int32 I=0; I<Cells.Num(); ++I)
		{
			if (Labels[I]!=INDEX_NONE) continue;
			const int32 Id=Regions.AddDefaulted();
			TArray<int32> Queue{I}; Labels[I]=Id;
			for (int32 Head=0; Head<Queue.Num(); ++Head)
				for (int32 J:Cells[Queue[Head]].Neighbors)
					if (J!=INDEX_NONE && Labels[J]==INDEX_NONE) { Labels[J]=Id; Queue.Add(J); }
		}
		for (int32 I=0; I<Cells.Num(); ++I) Regions[Labels[I]].Cells.Add(I);
		for (FRegion& Region:Regions)
		{
			FVector Centre=FVector::ZeroVector;
			bool HasCore=false;
			for (int32 I:Region.Cells) { Centre+=Cells[I].Position; HasCore|=Core[I]; }
			Centre/=Region.Cells.Num();
			double Best=TNumericLimits<double>::Max();
			for (int32 I:Region.Cells)
			{
				if (HasCore && !Core[I]) continue;
				const double D=FVector::DistSquared2D(Cells[I].Position,Centre);
				if (D<Best) { Best=D; Region.Representative=I; }
			}
		}
	}
}

TArray<FVector> UDungeonLightPlacementLibrary::PlanRegionLightPositions(
	const UObject* WorldContextObject, const TArray<FVector>& CandidateWorldPositions,
	const TArray<FVector>& ExistingLightWorldPositions, double GridSize,
	TArray<int32>& CandidateRegionIds, TArray<double>& AttenuationRadii,
	bool& bLimitReached, int32 MaxNewLights)
{
	using namespace DungeonLightRegions;
	TArray<FVector> Result;
	CandidateRegionIds.Reset(); AttenuationRadii.Reset(); bLimitReached=false;
	if (!IsInGameThread() || !GEngine || !FMath::IsFinite(GridSize) || GridSize<1.0) return Result;
	UWorld* World=GEngine->GetWorldFromContextObject(WorldContextObject,EGetWorldErrorMode::ReturnNull);
	if (!World || CandidateWorldPositions.IsEmpty()) return Result;
	if (CandidateWorldPositions.Num()>16384) { bLimitReached=true; return Result; }
	CandidateRegionIds.Init(INDEX_NONE,CandidateWorldPositions.Num());
	TArray<FVector> Sorted=CandidateWorldPositions;
	Sorted.RemoveAll([](const FVector& P){return P.ContainsNaN();});
	Sorted.Sort([](const FVector& A,const FVector& B){return A.X!=B.X?A.X<B.X:(A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z);});
	if (Sorted.IsEmpty()) return Result;
	const FVector Origin=Sorted[0];
	auto KeyFor=[&](const FVector& P,FIntPoint& Key)
	{
		if (P.ContainsNaN() || FMath::Abs(P.Z-Origin.Z)>1.0) return false;
		const double X=(P.X-Origin.X)/GridSize,Y=(P.Y-Origin.Y)/GridSize;
		if (!FMath::IsFinite(X) || !FMath::IsFinite(Y) || FMath::Abs(X)>1000000 || FMath::Abs(Y)>1000000) return false;
		Key=FIntPoint(FMath::RoundToInt(X),FMath::RoundToInt(Y));
		return FMath::Abs(X-Key.X)<0.01 && FMath::Abs(Y-Key.Y)<0.01;
	};
	FCollisionObjectQueryParams Objects;
	Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(DungeonLightRegions),false);
	const float Clearance=static_cast<float>(FMath::Min(40.0,GridSize*0.15));
	TArray<FCell> Cells;
	TMap<FIntPoint,int32> Lookup;
	for (const FVector& P:Sorted)
	{
		FIntPoint Key;
		if (!KeyFor(P,Key) || Lookup.Contains(Key)) continue;
		// Reject walls/pillars at body height and clearance around the ceiling fixture.
		if (World->OverlapAnyTestByObjectType(P-FVector(0,0,120),FQuat::Identity,Objects,
			FCollisionShape::MakeBox(FVector(Clearance,Clearance,10)),Params) ||
			World->OverlapAnyTestByObjectType(P,FQuat::Identity,Objects,
			FCollisionShape::MakeBox(FVector(Clearance,Clearance,5)),Params)) continue;
		FCell Cell; Cell.Key=Key; Cell.Position=P;
		Lookup.Add(Key,Cells.Add(Cell));
	}
	int32 BlockedEdges=0;
	for (int32 I=0; I<Cells.Num(); ++I) for (int32 D=0; D<2; ++D)
	{
		const int32* J=Lookup.Find(Cells[I].Key+Directions[D]);
		if (!J) continue;
		const FVector A=Cells[I].Position-FVector(0,0,120),B=Cells[*J].Position-FVector(0,0,120);
		if (World->LineTraceTestByObjectType(A,B,Objects,Params)) { ++BlockedEdges; continue; }
		Cells[I].Neighbors[D]=*J; Cells[*J].Neighbors[D+2]=I;
	}
	TArray<int32> Labels;
	TArray<FRegion> Regions;
	Partition(Cells,Labels,Regions);
	for (int32 I=0; I<CandidateWorldPositions.Num(); ++I)
	{
		FIntPoint Key;
		if (!KeyFor(CandidateWorldPositions[I],Key)) continue;
		if (const int32* J=Lookup.Find(Key)) CandidateRegionIds[I]=Labels[*J];
	}
	TSet<int32> Occupied;
	for (const FVector& P:ExistingLightWorldPositions)
	{
		// Room fixtures may record Z=290 while light candidates are Z=270.
		FIntPoint Key; FVector Flat=P; Flat.Z=Origin.Z;
		if (!KeyFor(Flat,Key)) continue;
		if (const int32* J=Lookup.Find(Key)) Occupied.Add(Labels[*J]);
	}
	const int32 Limit=FMath::Clamp(MaxNewLights,0,256);
	for (int32 Id=0; Id<Regions.Num(); ++Id)
	{
		if (Occupied.Contains(Id)) continue;
		if (Result.Num()>=Limit) { bLimitReached=true; break; }
		const FRegion& Region=Regions[Id];
		const FVector P=Cells[Region.Representative].Position;
		double RadiusSquared=0;
		for (int32 I:Region.Cells) RadiusSquared=FMath::Max(RadiusSquared,FVector::DistSquared2D(P,Cells[I].Position));
		Result.Add(P);
		AttenuationRadii.Add(FMath::Clamp(FMath::Sqrt(RadiusSquared)+GridSize,1000.0,20000.0));
	}
	UE_LOG(LogTemp,Display,TEXT("LIGHT_REGIONS: candidates=%d clear=%d blockedEdges=%d regions=%d newLights=%d limited=%d"),
		CandidateWorldPositions.Num(),Cells.Num(),BlockedEdges,Regions.Num(),Result.Num(),bLimitReached);
	return Result;
}
