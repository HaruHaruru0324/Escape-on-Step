#include "DungeonWallLayoutLibrary.h"
#include "DungeonWallTopology.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CollisionQueryParams.h"

namespace
{
 UDungeonWallRegistry* Registry(AActor* A)
 {
  if(auto* R=A->FindComponentByClass<UDungeonWallRegistry>())return R;
  auto* R=NewObject<UDungeonWallRegistry>(A,NAME_None,RF_Transient);A->AddInstanceComponent(R);R->RegisterComponent();return R;
 }
 FIntVector Canonical(FIntPoint P,int32 D)
 {
  if(D==0)return FIntVector(P.X,P.Y-1,1);
  if(D==1)return FIntVector(P.X,P.Y,0);
  if(D==2)return FIntVector(P.X,P.Y,1);
  return FIntVector(P.X-1,P.Y,0);
 }
 FVector Center(FIntVector E,int32 S,float Z=150.f)
 {return FVector((E.X+(E.Z==0?0.5:0))*S,(E.Y+(E.Z==1?0.5:0))*S,Z);}
}

AStaticMeshActor* UDungeonWallLayoutLibrary::SpawnAlignedDungeonWall(AActor* Generator,FIntPoint P,int32 D,int32 S,UStaticMesh* Mesh,float Thickness)
{
 if(!IsInGameThread()||!IsValid(Generator)||!Mesh||S<100||S>10000||D<0||D>3||!FMath::IsFinite(Thickness))return nullptr;
 UWorld* World=Generator->GetWorld();if(!World)return nullptr;
 const auto Key=Canonical(P,D);auto* R=Registry(Generator);
 if(auto* Existing=R->Walls.Find(Key);Existing&&Existing->IsValid())return Existing->Get();
 const auto Bounds=Mesh->GetBounds();const auto Ext=Bounds.BoxExtent;
 if(Ext.X<0.1||Ext.Y<0.1||Ext.Z<0.1)return nullptr;
 Thickness=FMath::Clamp(Thickness,5.f,S*0.2f);
 const FVector Scale(Thickness/(2*Ext.X),(S+Thickness)/(2*Ext.Y),1.0);
 const FQuat Rotation=FRotator(0,Key.Z==0?0:90,0).Quaternion();
 const FVector DesiredCenter=Center(Key,S,Ext.Z);
 const FVector Pivot=DesiredCenter-Rotation.RotateVector(Bounds.Origin*Scale);
 FActorSpawnParameters Params;Params.Owner=Generator;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 AStaticMeshActor* Wall=World->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),FTransform(Rotation,Pivot,Scale),Params);
 if(!Wall)return nullptr;
 auto* C=Wall->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Mesh);C->SetCollisionProfileName(TEXT("BlockAll"));C->SetMobility(EComponentMobility::Static);
 Wall->Tags.AddUnique(TEXT("DungeonAlignedWall"));R->Walls.Add(Key,Wall);R->bRefined=false;return Wall;
}

void UDungeonWallLayoutLibrary::RefineDungeonWallLayout(AActor* Generator,const TArray<FVector>& Positions,int32 S,int32& Removed,int32& Added,int32 MaxBaffles,int32 Limit)
{
 Removed=Added=0;
 if(!IsInGameThread()||!IsValid(Generator)||S<100||S>10000||Positions.Num()>4096)return;
 auto* R=Generator->FindComponentByClass<UDungeonWallRegistry>();if(!R||R->bRefined)return;
 UWorld* World=Generator->GetWorld();if(!World)return;
 TSet<FIntPoint> Floors;
 for(const auto& P:Positions)
 {
  if(P.ContainsNaN())continue;
  const double X=P.X/S,Y=P.Y/S;if(FMath::Abs(X)>100000||FMath::Abs(Y)>100000)continue;
  FIntPoint Cell(FMath::RoundToInt(X),FMath::RoundToInt(Y));
  if(FMath::Abs(X-Cell.X)<0.01&&FMath::Abs(Y-Cell.Y)<0.01)Floors.Add(Cell);
 }
 // Light candidates may omit corridor/small-room cells. Include every actual flat
 // floor tile on this generator's world-origin lattice before testing connectivity.
 for(TActorIterator<AStaticMeshActor> It(World);It;++It)
 {
  auto* C=It->GetStaticMeshComponent();if(!C->GetStaticMesh())continue;
  const auto B=C->Bounds;
  if(B.BoxExtent.Z>5||FMath::Abs(B.Origin.Z)>5||FMath::Abs(B.BoxExtent.X-S*0.5)>5||FMath::Abs(B.BoxExtent.Y-S*0.5)>5)continue;
  const double X=B.Origin.X/S,Y=B.Origin.Y/S;
  if(!FMath::IsFinite(X)||!FMath::IsFinite(Y)||FMath::Abs(X)>100000||FMath::Abs(Y)>100000)continue;
  FIntPoint Cell(FMath::RoundToInt(X),FMath::RoundToInt(Y));
  if(FMath::Abs(X-Cell.X)<0.01&&FMath::Abs(Y-Cell.Y)<0.01)Floors.Add(Cell);
 }
 if(Floors.IsEmpty()||Floors.Num()>4096)return;
 TSet<FIntVector> Walls;
 UStaticMesh* Mesh=nullptr;
 for(auto It=R->Walls.CreateIterator();It;++It)
 {if(!It.Value().IsValid()){It.RemoveCurrent();continue;}Walls.Add(It.Key());if(!Mesh)Mesh=It.Value()->GetStaticMeshComponent()->GetStaticMesh();}
 if(!Mesh)return;
 // Remove only one-cell interior fragments. Exterior boundaries are never removed.
 TArray<FIntVector> Remove;
 for(const auto& E:Walls)
 {
  const FIntPoint A(E.X,E.Y),B=A+(E.Z==0?FIntPoint(1,0):FIntPoint(0,1));
  const auto Along=E.Z==0?FIntVector(0,1,0):FIntVector(1,0,0);
  if(Floors.Contains(A)&&Floors.Contains(B)&&!Walls.Contains(E-Along)&&!Walls.Contains(E+Along))Remove.Add(E);
 }
 for(auto E:Remove)if(auto* W=R->Walls.Find(E);W&&W->IsValid()&&W->Get()->Destroy()){R->Walls.Remove(E);Walls.Remove(E);++Removed;}
 FCollisionQueryParams Query(SCENE_QUERY_STAT(DungeonWallWalkability),false,Generator);
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 // A conservative capsule checks furniture as well as wall collision, at walking height.
 const auto Shape=FCollisionShape::MakeCapsule(50.f,80.f);
 TSet<FIntVector> Blocked;
 const FIntPoint Steps[]={FIntPoint(1,0),FIntPoint(0,1)};
 for(const auto& A:Floors)for(int32 Axis=0;Axis<2;++Axis)
 {
  const auto B=A+Steps[Axis];if(!Floors.Contains(B))continue;
  const auto E=DungeonWallTopology::Edge(A,B);
  if(Walls.Contains(E)||World->SweepTestByObjectType(FVector(A.X*S,A.Y*S,110),FVector(B.X*S,B.Y*S,110),FQuat::Identity,Objects,Shape,Query))Blocked.Add(E);
 }
 const int32 BaseComponents=DungeonWallTopology::Components(Floors,Blocked);
 Limit=FMath::Clamp(Limit,4,20);MaxBaffles=FMath::Clamp(MaxBaffles,0,16);
 const int64 BeforeScore=DungeonWallTopology::SightScore(Floors,Blocked,Limit);
 // Never put a new baffle through existing non-wall props, goals or pawns.
 TArray<FBox> Obstacles;
 for(TActorIterator<AActor> It(World);It;++It)
 {
  AActor* A=*It;if(A==Generator||A->ActorHasTag(TEXT("DungeonAlignedWall")))continue;
  FVector O,E;A->GetActorBounds(true,O,E);
  if(E.Z<1||O.Z+E.Z<30||O.Z-E.Z>230)continue;
  Obstacles.Add(FBox(O-E,O+E).ExpandBy(FVector(50,50,0)));
 }
 TMap<FIntVector,bool> Allowed;
 auto CanPlace=[&](const FIntVector& E)
 {
  if(const bool* Cached=Allowed.Find(E))return *Cached;
  const FVector O=Center(E,S,130),Ext=E.Z==0?FVector(10,S*0.5+10,90):FVector(S*0.5+10,10,90);
  bool OK=true;for(const auto& Box:Obstacles)if(Box.Intersect(FBox(O-Ext,O+Ext))){OK=false;break;}
  Allowed.Add(E,OK);return OK;
 };
 for(int32 I=0;I<MaxBaffles;++I)
 {
  auto Plan=DungeonWallTopology::FindBaffle(Floors,Blocked,Walls,Limit,CanPlace);if(Plan.Edges.IsEmpty())break;
  TArray<FIntVector> Created;bool OK=true;
  for(auto E:Plan.Edges)
  {
   if(!SpawnAlignedDungeonWall(Generator,FIntPoint(E.X,E.Y),E.Z==0?1:2,S,Mesh)){OK=false;break;}
   Created.Add(E);
  }
  if(!OK){for(auto E:Created){if(auto* W=R->Walls.Find(E);W&&W->IsValid())W->Get()->Destroy();R->Walls.Remove(E);}break;}
  for(auto E:Created){Walls.Add(E);Blocked.Add(E);}++Added;
 }
 R->bRefined=true;
 UE_LOG(LogTemp,Display,TEXT("WALL_POLISH floors=%d removedStubs=%d baffles=%d sightScore=%lld->%lld components=%d->%d"),Floors.Num(),Removed,Added,BeforeScore,DungeonWallTopology::SightScore(Floors,Blocked,Limit),BaseComponents,DungeonWallTopology::Components(Floors,Blocked));
}
