#include "DungeonWallTopology.h"

namespace DungeonWallTopology
{
namespace
{
 const FIntPoint Directions[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
 bool LessCell(const FIntPoint& A,const FIntPoint& B){return A.X!=B.X?A.X<B.X:A.Y<B.Y;}
 bool LessEdge(const FIntVector& A,const FIntVector& B){return A.X!=B.X?A.X<B.X:A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z;}
 TMap<FIntPoint,int32> Labels(const TSet<FIntPoint>& F,const TSet<FIntVector>& W)
 {
  TMap<FIntPoint,int32> L;auto Cells=F.Array();Cells.Sort(LessCell);int32 Label=0;
  for(auto A:Cells)
  {
   if(L.Contains(A))continue;
   TArray<FIntPoint> Q{A};L.Add(A,Label);
   for(int32 I=0;I<Q.Num();++I)for(auto D:Directions)
   {auto B=Q[I]+D;if(F.Contains(B)&&!L.Contains(B)&&!W.Contains(Edge(Q[I],B))){L.Add(B,Label);Q.Add(B);}}
   ++Label;
  }
  return L;
 }
 // Integer corner lattice: cell (x,y) occupies corners (x,y)..(x+1,y+1).
 void Ends(FIntVector E,FIntPoint& A,FIntPoint& B)
 {A=E.Z==0?FIntPoint(E.X+1,E.Y):FIntPoint(E.X,E.Y+1);B=A+(E.Z==0?FIntPoint(0,1):FIntPoint(1,0));}
 FIntVector CornerEdge(FIntPoint A,FIntPoint B)
 {return A.X==B.X?FIntVector(A.X-1,FMath::Min(A.Y,B.Y),0):FIntVector(FMath::Min(A.X,B.X),A.Y-1,1);}
}

TSet<FIntVector> Bridges(const TSet<FIntPoint>& F,const TSet<FIntVector>& W)
{
 TSet<FIntVector> Result;TMap<FIntPoint,int32> Visit,Low;int32 Time=0;
 struct FFrame{FIntPoint Cell,Parent;int32 Next=0;bool HasParent=false;};
 auto Cells=F.Array();Cells.Sort(LessCell);
 for(auto Root:Cells)
 {
  if(Visit.Contains(Root))continue;
  TArray<FFrame> Stack;Stack.Add({Root,Root,0,false});Visit.Add(Root,++Time);Low.Add(Root,Time);
  while(!Stack.IsEmpty())
  {
   auto& Frame=Stack.Last();const auto A=Frame.Cell;
   if(Frame.Next<4)
   {
    auto B=A+Directions[Frame.Next++];
    if(!F.Contains(B)||W.Contains(Edge(A,B))||(Frame.HasParent&&B==Frame.Parent))continue;
    if(Visit.Contains(B)){Low[A]=FMath::Min(Low[A],Visit[B]);continue;}
    Visit.Add(B,++Time);Low.Add(B,Time);Stack.Add({B,A,0,true});
   }
   else
   {
    const auto Done=Frame;Stack.Pop(EAllowShrinking::No);
    if(Done.HasParent){Low[Done.Parent]=FMath::Min(Low[Done.Parent],Low[A]);if(Low[A]>Visit[Done.Parent])Result.Add(Edge(A,Done.Parent));}
   }
  }
 }
 return Result;
}

TArray<TSet<FIntPoint>> OuterDeadEnds(const TSet<FIntPoint>& F,const TSet<FIntVector>& W)
{
 const auto B=Bridges(F,W);auto Cut=W;Cut.Append(B);const auto L=Labels(F,Cut);
 TArray<TSet<FIntPoint>> Regions;TArray<int32> Degree;TArray<bool> Outer;
 for(const auto& Pair:L)
 {
  while(Regions.Num()<=Pair.Value){Regions.AddDefaulted();Degree.Add(0);Outer.Add(false);}
  Regions[Pair.Value].Add(Pair.Key);
  for(auto D:Directions)if(!F.Contains(Pair.Key+D))Outer[Pair.Value]=true;
 }
 for(auto E:B){++Degree[L[FIntPoint(E.X,E.Y)]];++Degree[L[FIntPoint(E.X,E.Y)+Directions[E.Z]]];}
 TArray<TSet<FIntPoint>> Result;
 for(int32 I=0;I<Regions.Num();++I)if(Outer[I]&&Degree[I]==1)Result.Add(MoveTemp(Regions[I]));
 return Result;
}

bool FindOuterLoop(const TSet<FIntPoint>& F,const TSet<FIntVector>& W,const TSet<FIntVector>& Walls,
 TFunctionRef<bool(const FIntVector&)> CanOpen,FIntVector& Out)
{
 const auto L=Labels(F,W);const auto ExistingBridges=Bridges(F,W);auto Edges=Walls.Array();Edges.Sort(LessEdge);
 for(const auto& Region:OuterDeadEnds(F,W))
 {
  bool Found=false;int64 Best=MAX_int64;
  for(auto E:Edges)
  {
   const FIntPoint A(E.X,E.Y),B=A+Directions[E.Z];
   if(!F.Contains(A)||!F.Contains(B)||Region.Contains(A)==Region.Contains(B)||L[A]!=L[B]||!CanOpen(E))continue;
   // Keep a wall segment between the original entrance and the new exit. Removing
   // its immediate neighbour would only widen the same doorway.
   FIntPoint EA,EB;Ends(E,EA,EB);bool AdjacentEntrance=false;
   for(auto Bridge:ExistingBridges)
   {
    const FIntPoint BA(Bridge.X,Bridge.Y),BB=BA+Directions[Bridge.Z];
    if(Region.Contains(BA)==Region.Contains(BB))continue;
    FIntPoint CA,CB;Ends(Bridge,CA,CB);
    if(EA==CA||EA==CB||EB==CA||EB==CB){AdjacentEntrance=true;break;}
   }
   if(AdjacentEntrance)continue;
   auto Trial=W;Trial.Remove(E);const auto Score=SightScore(F,Trial,7);
   if(!Found||Score<Best){Found=true;Best=Score;Out=E;}
  }
  if(Found)return true; // Exactly one new opening for this terminal region.
 }
 return false;
}

FPlan FindFloatingWallExtension(const TSet<FIntPoint>& F,const TSet<FIntVector>& W,const TSet<FIntVector>& Walls,
 TFunctionRef<bool(const FIntVector&)> CanPlace)
{
 FPlan Result;const int32 Base=Components(F,W);const auto OldBridges=Bridges(F,W);
 TMap<FIntPoint,TArray<FIntVector>> At;
 for(auto E:Walls){FIntPoint A,B;Ends(E,A,B);At.FindOrAdd(A).Add(E);At.FindOrAdd(B).Add(E);}
 TSet<FIntVector> Seen;auto Sorted=Walls.Array();Sorted.Sort(LessEdge);
 for(auto Seed:Sorted)
 {
  if(Seen.Contains(Seed))continue;
  TArray<FIntVector> Group{Seed};Seen.Add(Seed);bool Straight=true,Interior=true;
  for(int32 I=0;I<Group.Num();++I)
  {
   const auto E=Group[I];Straight&=E.Z==Seed.Z;Interior&=F.Contains(FIntPoint(E.X,E.Y))&&F.Contains(FIntPoint(E.X,E.Y)+Directions[E.Z]);
   FIntPoint A,B;Ends(E,A,B);for(auto P:{A,B})for(auto N:At[P])if(!Seen.Contains(N)){Seen.Add(N);Group.Add(N);}
  }
  if(!Straight||!Interior)continue;
  Group.Sort(LessEdge);FIntPoint First,Unused,Last;Ends(Group[0],First,Unused);Ends(Group.Last(),Unused,Last);
  TSet<FIntPoint> Own;for(auto E:Group){FIntPoint A,B;Ends(E,A,B);Own.Add(A);Own.Add(B);}
  auto Accept=[&](const TArray<FIntVector>& Path)
  {
   auto Trial=W;for(auto E:Path)Trial.Add(E);
   if(Components(F,Trial)!=Base)return false;
   for(auto E:Bridges(F,Trial))if(!OldBridges.Contains(E))return false;
   return true;
  };
  FPlan Elbow;
  // Prefer a short attachment; fall back to a two-cell perpendicular arm (an L).
  for(int32 Length=1;Length<=4;++Length)for(auto Start:{First,Last})for(int32 Direction=0;Direction<4;++Direction)
  {
   TArray<FIntVector> Path;auto P=Start;bool OK=true;
   for(int32 J=0;J<Length;++J)
   {
    const auto Next=P+Directions[Direction];const auto E=CornerEdge(P,Next);const FIntPoint A(E.X,E.Y);
    if(Walls.Contains(E)||W.Contains(E)||Own.Contains(Next)||!F.Contains(A)||!F.Contains(A+Directions[E.Z])||!CanPlace(E)){OK=false;break;}
    Path.Add(E);P=Next;
    if(At.Contains(P)&&J!=Length-1){OK=false;break;}
   }
   if(!OK)continue;
   const bool Attached=At.Contains(P);
   const bool IsElbow=Length==2&&Path[0].Z!=Seed.Z;
   if(!Attached&&!IsElbow)continue;
   if(!Accept(Path))continue;
   if(Attached){Result.Edges=MoveTemp(Path);return Result;}
   if(Elbow.Edges.IsEmpty())Elbow.Edges=Path;
  }
  if(!Elbow.Edges.IsEmpty())return Elbow;
 }
 return Result;
}
}
