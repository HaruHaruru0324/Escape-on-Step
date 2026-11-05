#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "DungeonWallTopology.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDungeonWallTopologyTest,"EscapeOnStep.Dungeon.WallTopology",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDungeonWallTopologyTest::RunTest(const FString&)
{
 using namespace DungeonWallTopology;
 auto Rectangle=[](int32 W,int32 H,TSet<FIntPoint>& F,TSet<FIntVector>& Walls)
 {for(int32 X=0;X<W;++X)for(int32 Y=0;Y<H;++Y)F.Add(FIntPoint(X,Y));for(int32 Y=0;Y<H;++Y){Walls.Add(FIntVector(-1,Y,0));Walls.Add(FIntVector(W-1,Y,0));}for(int32 X=0;X<W;++X){Walls.Add(FIntVector(X,-1,1));Walls.Add(FIntVector(X,H-1,1));}};
 TSet<FIntPoint> F;TSet<FIntVector> W;Rectangle(16,8,F,W);
 auto Plan=FindBaffle(F,W,W,7,[](const FIntVector&){return true;});
 TestTrue(TEXT("Wide open room receives a baffle"),Plan.Edges.Num()>=3);
 TestTrue(TEXT("Sight score decreases"),Plan.AfterScore<Plan.BeforeScore);
 auto W2=W;for(auto E:Plan.Edges)W2.Add(E);
 TestEqual(TEXT("Every floor cell remains reachable"),Components(F,W2),1);
 auto Again=FindBaffle(F,W,W,7,[](const FIntVector&){return true;});
 TestTrue(TEXT("Selection is deterministic"),Again.Edges==Plan.Edges);
 auto NoProps=FindBaffle(F,W,W,7,[](const FIntVector&){return false;});
 TestEqual(TEXT("Protected prop locations reject new walls"),NoProps.Edges.Num(),0);
 F.Reset();W.Reset();Rectangle(24,1,F,W);
 auto Narrow=FindBaffle(F,W,W,7,[](const FIntVector&){return true;});
 TestEqual(TEXT("One-cell corridor is never sealed"),Narrow.Edges.Num(),0);
 F.Reset();W.Reset();Rectangle(4,4,F,W);
 TestEqual(TEXT("Short sightlines do not gain walls"),FindBaffle(F,W,W,7,[](const FIntVector&){return true;}).Edges.Num(),0);
 F.Reset();W.Reset();Rectangle(16,8,F,W);for(int32 Y=0;Y<8;++Y)W.Add(FIntVector(7,Y,0));
 TestEqual(TEXT("Fixture has two components"),Components(F,W),2);
 auto Separate=FindBaffle(F,W,W,7,[](const FIntVector&){return true;});for(auto E:Separate.Edges)W.Add(E);
 TestEqual(TEXT("Existing components never split further"),Components(F,W),2);
 return true;
}
#endif
