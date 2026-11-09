#if WITH_DEV_AUTOMATION_TESTS
#include "DungeonGoalRegions.h"
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDungeonGoalRegionTest,"EscapeOnStep.Dungeon.GoalRegions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDungeonGoalRegionTest::RunTest(const FString&)
{
 using namespace DungeonLightRegions;
 auto Build=[](int32 W,int32 H)
 {
  TArray<FCell> C;
  for(int32 X=0;X<W;++X)for(int32 Y=0;Y<H;++Y){FCell V;V.Key={X,Y};V.Position=FVector(X*300,Y*300,0);C.Add(V);}
  for(int32 X=0;X<W;++X)for(int32 Y=0;Y<H;++Y)
  {auto& V=C[X*H+Y];if(X+1<W)V.Neighbors[0]=(X+1)*H+Y;if(Y+1<H)V.Neighbors[1]=X*H+Y+1;if(X>0)V.Neighbors[2]=(X-1)*H+Y;if(Y>0)V.Neighbors[3]=X*H+Y-1;}
  return C;
 };
 TArray<int32> Labels;TArray<FRegion> Regions;
 auto C=Build(12,6);Partition(C,Labels,Regions);
 auto P=DungeonGoalRegions::CentralCandidates(C,Labels,Regions,300);
 TestTrue(TEXT("Merged open rooms have a usable centre"),!P.IsEmpty());
 if(!P.IsEmpty())
 {
  TestTrue(TEXT("Uses centre of finished 12x6 space, not original 6x6 room"),P[0].Position.Equals(FVector(1650,750,0),0.01));
  TestEqual(TEXT("Area is actual merged floor area"),P[0].Area,72);
  TestTrue(TEXT("Centre has substantial wall clearance"),P[0].Clearance>=899);
 }
 // A later full partition changes the centres even though floor metadata is unchanged.
 for(int32 Y=0;Y<6;++Y){C[5*6+Y].Neighbors[0]=INDEX_NONE;C[6*6+Y].Neighbors[2]=INDEX_NONE;}
 Partition(C,Labels,Regions);P=DungeonGoalRegions::CentralCandidates(C,Labels,Regions,300);
 TestEqual(TEXT("Final wall divides spaces"),Regions.Num(),2);
 TestTrue(TEXT("Split spaces have central positions"),!P.IsEmpty());
 for(const auto& Candidate:P)
 {
  TestTrue(TEXT("No goal on the old centre partition"),FMath::Abs(Candidate.Position.X-1650)>=375);
  TestTrue(TEXT("Only central candidates"),FVector::Dist2D(Candidate.Position,Candidate.Centroid)<=225.01);
 }
 C=Build(2,20);Partition(C,Labels,Regions);
 TestEqual(TEXT("Long narrow corridor is not mistaken for a large room"),DungeonGoalRegions::CentralCandidates(C,Labels,Regions,300).Num(),0);
 C=Build(3,3);Partition(C,Labels,Regions);P=DungeonGoalRegions::CentralCandidates(C,Labels,Regions,300);
 TestTrue(TEXT("Small broad room still has a centre"),!P.IsEmpty());
 if(!P.IsEmpty())TestTrue(TEXT("Small room centre is exact"),P[0].Position.Equals(FVector(300,300,0),0.01));
 TestTrue(TEXT("Empty topology returns no unsafe fallback"),DungeonGoalRegions::CentralCandidates({}, {}, {},300).IsEmpty());
 return true;
}
#endif
