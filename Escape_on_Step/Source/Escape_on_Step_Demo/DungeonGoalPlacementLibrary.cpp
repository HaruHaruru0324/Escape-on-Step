#include "DungeonGoalPlacementLibrary.h"
#include "UObject/UnrealType.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "CollisionQueryParams.h"
#include "DungeonGoalRegions.h"
#include "DungeonGenerationRetrySubsystem.h"
#include "Engine/GameInstance.h"
#include "HAL/IConsoleManager.h"

#if !UE_BUILD_SHIPPING
// Development-only fault injection for verifying reloads and the retry limit.
static TAutoConsoleVariable<int32> GoalForcedFailures(TEXT("Dungeon.Goal.ForceFailures"), 0,
 TEXT("Reject the next N goal selections to test regeneration. Development only."), ECVF_Default);
#endif

namespace
{
 FProperty* Field(UStruct* Type,const FString& Name)
 {
  for(TFieldIterator<FProperty> It(Type);It;++It)
   if(It->GetName()==Name||It->GetName().StartsWith(Name+TEXT("_")))return *It;
  return nullptr;
 }
 bool Integer(UStruct* Type,const void* Data,const FString& Name,int32& Out)
 {
  auto* P=CastField<FNumericProperty>(Field(Type,Name));if(!P||!P->IsInteger())return false;
  Out=int32(P->GetSignedIntPropertyValue(P->ContainerPtrToValuePtr<void>(Data)));return true;
 }
 struct FCandidate{FVector P,Centroid;int32 Index,Area;double DistanceSquared,Clearance;};
}

static bool FindLargeRoomGoalLocation(AActor* G,UStaticMesh* Table,
 FVector& Out,int32& RoomIndex,int32& RoomArea)
{
 Out=FVector::ZeroVector;RoomIndex=INDEX_NONE;RoomArea=0;
 if(!IsInGameThread()||!IsValid(G)||!Table||!G->GetWorld())return false;
 auto* World=G->GetWorld();auto* Player=UGameplayStatics::GetPlayerCharacter(G,0);
 if(!Player)return false;
 int32 S=0;if(!Integer(G->GetClass(),G,TEXT("Grid_size"),S)||S<100||S>10000)return false;
 auto* GP=CastField<FArrayProperty>(Field(G->GetClass(),TEXT("Grids")));
 auto* GS=GP?CastField<FStructProperty>(GP->Inner):nullptr;
 if(!GS){UE_LOG(LogTemp,Warning,TEXT("GOAL_SPACE: missing Grids schema"));return false;}
 FScriptArrayHelper Grids(GP,GP->ContainerPtrToValuePtr<void>(G));
 if(Grids.Num()>16384)return false;
 auto* Pos=CastField<FStructProperty>(Field(GS->Struct,TEXT("Grid_Pos")));
 FProperty* Type=Field(GS->Struct,TEXT("Grid_Type"));
 if(!Pos||Pos->Struct!=TBaseStructure<FIntPoint>::Get()||!Type)return false;
 auto* Enum=CastField<FEnumProperty>(Type);
 auto* Numeric=Enum?Enum->GetUnderlyingProperty():CastField<FNumericProperty>(Type);
 if(!Numeric)return false;
 TSet<FIntPoint> Floors;
 for(int32 I=0;I<Grids.Num();++I)
 {
  const void* Cell=Grids.GetRawPtr(I);
  const int64 Kind=Numeric->GetSignedIntPropertyValue(Type->ContainerPtrToValuePtr<void>(Cell));
  if(Kind>=1&&Kind<=3)Floors.Add(*Pos->ContainerPtrToValuePtr<FIntPoint>(Cell));
 }
 if(Floors.IsEmpty()||Floors.Num()>4096)return false;
 FCollisionQueryParams Query(SCENE_QUERY_STAT(DungeonGoalRoom),false,G);
 // Moving characters are not permanent geometry; do not let them disconnect rooms.
 for(TActorIterator<APawn> It(World);It;++It)Query.AddIgnoredActor(*It);
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 const float Radius=Player->GetCapsuleComponent()->GetScaledCapsuleRadius()+3.f;
 const float Half=Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()+3.f;
 const auto Capsule=FCollisionShape::MakeCapsule(Radius,Half);
 const auto At=[&](FIntPoint P){return FVector(double(P.X)*S,double(P.Y)*S,Half+5);};
 const auto ClearPath=[&](FVector A,FVector B)
 {return !World->SweepTestByObjectType(A,B,FQuat::Identity,Objects,Capsule,Query);};
 TArray<FIntPoint> Sorted=Floors.Array();const auto PlayerP=Player->GetActorLocation();
 Sorted.Sort([&](const FIntPoint& A,const FIntPoint& B){const double DA=FVector::DistSquared2D(At(A),PlayerP),DB=FVector::DistSquared2D(At(B),PlayerP);return DA!=DB?DA<DB:A.X!=B.X?A.X<B.X:A.Y<B.Y;});
 TSet<FIntPoint> Reachable;TArray<FIntPoint> Queue;
 for(auto C:Sorted)
 {
  if(FVector::DistSquared2D(At(C),PlayerP)>FMath::Square(2.0*S))break;
  if(ClearPath(FVector(PlayerP.X,PlayerP.Y,Half+5),At(C))){Reachable.Add(C);Queue.Add(C);break;}
 }
 const FIntPoint Steps[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
 for(int32 I=0;I<Queue.Num();++I)for(auto D:Steps)
 {
  auto B=Queue[I]+D;if(!Floors.Contains(B)||Reachable.Contains(B))continue;
  if(ClearPath(At(Queue[I]),At(B))){Reachable.Add(B);Queue.Add(B);}
 }
 const auto Bounds=Table->GetBounds();
 const FVector Ext(FMath::Abs(Bounds.Origin.X)+Bounds.BoxExtent.X+Radius,
  FMath::Abs(Bounds.Origin.Y)+Bounds.BoxExtent.Y+Radius,FMath::Max(100.0,double(Bounds.Origin.Z+Bounds.BoxExtent.Z)*0.5));
 // Reconstruct open spaces from the FINAL physical walls. Original Rooms is
 // intentionally unused: its rectangles can overlap or be cut by later walls.
 TArray<DungeonLightRegions::FCell> Cells;TMap<FIntPoint,int32> Lookup;
 auto GeometryKeys=Floors.Array();GeometryKeys.Sort([](FIntPoint A,FIntPoint B){return A.X!=B.X?A.X<B.X:A.Y<B.Y;});
 for(auto K:GeometryKeys)
 {
  FVector P(double(K.X)*S,double(K.Y)*S,0);
  if(World->OverlapAnyTestByObjectType(P+FVector(0,0,150),FQuat::Identity,Objects,FCollisionShape::MakeBox(FVector(20,20,20)),Query))continue;
  DungeonLightRegions::FCell C;C.Key=K;C.Position=P;Lookup.Add(K,Cells.Add(C));
 }
 for(int32 I=0;I<Cells.Num();++I)for(int32 D=0;D<2;++D)
 {
  const int32* J=Lookup.Find(Cells[I].Key+Steps[D]);if(!J)continue;
  if(!World->LineTraceTestByObjectType(Cells[I].Position+FVector(0,0,150),Cells[*J].Position+FVector(0,0,150),Objects,Query))
  {Cells[I].Neighbors[D]=*J;Cells[*J].Neighbors[D+2]=I;}
 }
 TArray<int32> Labels;TArray<DungeonLightRegions::FRegion> Regions;
 DungeonLightRegions::Partition(Cells,Labels,Regions);
 const auto Centres=DungeonGoalRegions::CentralCandidates(Cells,Labels,Regions,S);
 TArray<FCandidate> Candidates;TSet<int32> SelectedRegions;
 for(const auto& Centre:Centres)
 {
  if(SelectedRegions.Contains(Centre.Region))continue;
  const FVector P=Centre.Position;
  if(World->OverlapAnyTestByObjectType(P+FVector(0,0,Ext.Z+5),FQuat::Identity,Objects,FCollisionShape::MakeBox(Ext),Query))continue;
  bool Supported=true;
  for(int32 DX:{-1,1})for(int32 DY:{-1,1})
  {
   const auto* I=Lookup.Find(FIntPoint(FMath::RoundToInt((P.X+DX*Ext.X)/S),FMath::RoundToInt((P.Y+DY*Ext.Y)/S)));
   Supported&=I&&Labels[*I]==Centre.Region;
  }
  if(!Supported)continue;
  bool Accessible=false;
  for(auto D:Steps)
  {
   const FVector Approach=P+FVector(D.X*(Ext.X+Radius+5),D.Y*(Ext.Y+Radius+5),Half+5);
   const FIntPoint Cell(FMath::RoundToInt(Approach.X/S),FMath::RoundToInt(Approach.Y/S));
   if(Reachable.Contains(Cell)&&ClearPath(At(Cell),Approach)){Accessible=true;break;}
  }
  if(!Accessible)continue;
  Candidates.Add({P,Centre.Centroid,Centre.Region,Centre.Area,FVector::DistSquared2D(P,PlayerP),Centre.Clearance});SelectedRegions.Add(Centre.Region);
 }
 if(Candidates.IsEmpty()){UE_LOG(LogTemp,Error,TEXT("GOAL_SPACE: no clear reachable broad-space centre; refusing wall-side fallback (reachable=%d regions=%d)"),Reachable.Num(),Regions.Num());return false;}
 int32 MaxArea=0;for(const auto& C:Candidates)MaxArea=FMath::Max(MaxArea,C.Area);
 // Prefer >=25 cells when available and the upper area band among usable rooms.
 const double MinArea=FMath::Max(MaxArea*0.75,double(FMath::Min(25,MaxArea)));
 double Far=0;for(const auto& C:Candidates)if(C.Area>=MinArea)Far=FMath::Max(Far,C.DistanceSquared);
 TArray<int32> Pool;
 for(int32 I=0;I<Candidates.Num();++I)if(Candidates[I].Area>=MinArea&&Candidates[I].DistanceSquared>=Far*0.64)Pool.Add(I);
 const auto& Chosen=Candidates[Pool[FMath::RandRange(0,Pool.Num()-1)]];
 Out=Chosen.P;RoomIndex=Chosen.Index;RoomArea=Chosen.Area;
 UE_LOG(LogTemp,Display,TEXT("GOAL_SPACE region=%d area=%d largestUsable=%d position=(%s) centroid=(%s) centreOffset=%.1f wallClearance=%.1f distance=%.0f reachable=%d candidates=%d"),RoomIndex,RoomArea,MaxArea,*Out.ToString(),*Chosen.Centroid.ToString(),FVector::Dist2D(Out,Chosen.Centroid),Chosen.Clearance,FMath::Sqrt(Chosen.DistanceSquared),Reachable.Num(),Candidates.Num());
 return true;
}

bool UDungeonGoalPlacementLibrary::SelectLargeRoomGoalLocation(AActor* Generator,UStaticMesh* Table,
 FVector& Out,int32& RoomIndex,int32& RoomArea)
{
 Out=FVector::ZeroVector;RoomIndex=INDEX_NONE;RoomArea=0;
 if(!IsInGameThread()||!IsValid(Generator)||!Generator->GetWorld())return false;
 bool bForceFailure=false;
#if !UE_BUILD_SHIPPING
 const int32 Forced=GoalForcedFailures.GetValueOnGameThread();
 if(Forced>0)
 {
  GoalForcedFailures->Set(Forced-1,ECVF_SetByConsole);
  bForceFailure=true;
  UE_LOG(LogTemp,Display,TEXT("GOAL_RETRY injected failure; remaining=%d"),Forced-1);
 }
#endif
 const bool bFound=!bForceFailure&&FindLargeRoomGoalLocation(Generator,Table,Out,RoomIndex,RoomArea);
 if(auto* GI=Generator->GetWorld()->GetGameInstance())
  if(auto* Retry=GI->GetSubsystem<UDungeonGenerationRetrySubsystem>())
  {
   if(bFound)Retry->MarkGoalLocationFound(Generator->GetWorld());
   else Retry->RequestRegeneration(Generator->GetWorld());
  }
 return bFound;
}
