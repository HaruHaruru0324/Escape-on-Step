#include "DungeonWallTopology.h"
namespace DungeonWallTopology
{
 FIntVector Edge(const FIntPoint& A,const FIntPoint& B)
 { return A.X!=B.X ? FIntVector(FMath::Min(A.X,B.X),A.Y,0) : FIntVector(A.X,FMath::Min(A.Y,B.Y),1); }
 static const FIntPoint Steps[]={FIntPoint(1,0),FIntPoint(0,1),FIntPoint(-1,0),FIntPoint(0,-1)};
 static bool Open(const FIntPoint& A,const FIntPoint& B,const TSet<FIntPoint>& F,const TSet<FIntVector>& W)
 {return F.Contains(B)&&!W.Contains(Edge(A,B));}
 int32 Components(const TSet<FIntPoint>& F,const TSet<FIntVector>& W)
 {
  TSet<FIntPoint> Seen;int32 Count=0;TArray<FIntPoint> Queue;
  for(const FIntPoint& Start:F)
  {
   if(Seen.Contains(Start))continue;
   ++Count;Queue.Reset();Queue.Add(Start);Seen.Add(Start);
   for(int32 I=0;I<Queue.Num();++I)for(const auto& D:Steps)
   { const FIntPoint B=Queue[I]+D;if(!Seen.Contains(B)&&Open(Queue[I],B,F,W)){Seen.Add(B);Queue.Add(B);} }
  }
  return Count;
 }
 struct FRun {FIntPoint Start;int32 Axis,Length;};
 static TArray<FRun> Runs(const TSet<FIntPoint>& F,const TSet<FIntVector>& W,int32 Limit)
 {
  TArray<FRun> R;
  for(const auto& A:F)for(int32 Axis=0;Axis<2;++Axis)
  {
   const auto D=Steps[Axis];if(Open(A,A-D,F,W))continue;
   int32 L=1;FIntPoint P=A;
   while(Open(P,P+D,F,W)){P+=D;++L;}
   if(L>Limit)R.Add({A,Axis,L});
  }
  R.Sort([](const FRun&A,const FRun&B){if(A.Length!=B.Length)return A.Length>B.Length;if(A.Start.X!=B.Start.X)return A.Start.X<B.Start.X;if(A.Start.Y!=B.Start.Y)return A.Start.Y<B.Start.Y;return A.Axis<B.Axis;});
  return R;
 }
 int64 SightScore(const TSet<FIntPoint>& F,const TSet<FIntVector>& W,int32 Limit)
 {int64 S=0;for(const auto&R:Runs(F,W,Limit)){const int64 N=R.Length-Limit;S+=N*N;}return S;}
 static bool TouchesEnd(const FIntVector& E,int32 Sign,const TSet<FIntVector>& W)
 {
  const auto Along=E.Z==0?FIntVector(0,Sign,0):FIntVector(Sign,0,0);
  if(W.Contains(E+Along))return true;
  if(E.Z==0){const int32 Y=E.Y+(Sign<0?-1:0);return W.Contains(FIntVector(E.X,Y,1))||W.Contains(FIntVector(E.X+1,Y,1));}
  const int32 X=E.X+(Sign<0?-1:0);return W.Contains(FIntVector(X,E.Y,0))||W.Contains(FIntVector(X,E.Y+1,0));
 }
 FPlan FindBaffle(const TSet<FIntPoint>& F,const TSet<FIntVector>& W,const TSet<FIntVector>& Walls,int32 Limit,TFunctionRef<bool(const FIntVector&)> CanPlace)
 {
  FPlan Best;Best.BeforeScore=SightScore(F,W,Limit);Best.AfterScore=Best.BeforeScore;
  const int32 BaseComponents=Components(F,W);const auto OldBridges=Bridges(F,W);int32 Evaluations=0,Scans=0;
  for(const auto& R:Runs(F,W,Limit))
  {
   for(int32 Offset:{0,-1,1,-2,2})
   {
    const int32 I=(R.Length-1)/2+Offset;if(I<1||I>=R.Length-2)continue;
    const auto A=R.Start+Steps[R.Axis]*I;const auto Target=Edge(A,A+Steps[R.Axis]);
    const auto Along=R.Axis==0?FIntVector(0,1,0):FIntVector(1,0,0);
    // Enumerate straight, wall-attached runs of 3-6 cells containing the sightline.
    for(int32 Length=3;Length<=6;++Length)for(int32 Before=0;Before<Length;++Before)
    {
     if(++Scans>4096)return Best;
     const auto First=Target-Along*Before;const auto Last=First+Along*(Length-1);
     if(!TouchesEnd(First,-1,Walls)&&!TouchesEnd(Last,1,Walls))continue;
     TArray<FIntVector> Candidate;bool OK=true;
     for(int32 J=0;J<Length;++J)
     {
      auto E=First+Along*J;const FIntPoint P(E.X,E.Y);
      if(!F.Contains(P)||!F.Contains(P+Steps[E.Z])||W.Contains(E)||!CanPlace(E)){OK=false;break;}
      Candidate.Add(E);
     }
     if(!OK)continue;
     if(++Evaluations>384)return Best;
     TSet<FIntVector> Trial=W;for(auto E:Candidate)Trial.Add(E);
     const int64 Score=SightScore(F,Trial,Limit);
     if(Score>=Best.AfterScore||Components(F,Trial)!=BaseComponents)continue;
     bool NewDeadEnd=false;for(auto E:Bridges(F,Trial))if(!OldBridges.Contains(E)){NewDeadEnd=true;break;}
     if(NewDeadEnd)continue;
     Best.Edges=MoveTemp(Candidate);Best.AfterScore=Score;
    }
   }
  }
  return Best;
 }
}
