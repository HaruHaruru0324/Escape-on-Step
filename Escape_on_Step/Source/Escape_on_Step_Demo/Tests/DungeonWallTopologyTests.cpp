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
 // Two perimeter rooms joined through exactly one doorway: add one more doorway.
 F.Reset();W.Reset();Rectangle(8,4,F,W);for(int32 Y=1;Y<4;++Y)W.Add(FIntVector(3,Y,0));
 TestEqual(TEXT("Two terminal perimeter rooms"),OuterDeadEnds(F,W).Num(),2);
 FIntVector Opening;
 TestFalse(TEXT("Furniture can prohibit every opening"),FindOuterLoop(F,W,W,[](const FIntVector&){return false;},Opening));
 TestTrue(TEXT("A return route can be opened"),FindOuterLoop(F,W,W,[](const FIntVector&){return true;},Opening));
 TestTrue(TEXT("Opening has floor on both sides"),F.Contains(FIntPoint(Opening.X,Opening.Y))&&F.Contains(FIntPoint(Opening.X,Opening.Y)+FIntPoint(Opening.Z==0?1:0,Opening.Z==1?1:0)));
 W.Remove(Opening);
 TestEqual(TEXT("One extra opening removes both dead ends"),OuterDeadEnds(F,W).Num(),0);
 TestFalse(TEXT("Already cyclic rooms get no further doorway"),FindOuterLoop(F,W,W,[](const FIntVector&){return true;},Opening));
 auto PreserveLoop=FindBaffle(F,W,W,4,[](const FIntVector&){return true;});for(auto E:PreserveLoop.Edges)W.Add(E);
 TestEqual(TEXT("Baffles preserve the return route"),Bridges(F,W).Num(),0);
 F.Reset();W.Reset();Rectangle(24,1,F,W);
 TestFalse(TEXT("Never opens outside a terminal corridor"),FindOuterLoop(F,W,W,[](const FIntVector&){return true;},Opening));
 F.Reset();W.Reset();Rectangle(12,10,F,W);W.Add(FIntVector(5,4,0));W.Add(FIntVector(5,5,0));
 auto Attach=FindFloatingWallExtension(F,W,W,[](const FIntVector&){return true;});
 TestTrue(TEXT("Floating straight wall gains an attachment"),!Attach.Edges.IsEmpty());
 for(auto E:Attach.Edges)W.Add(E);
 TestEqual(TEXT("Attachment does not split walkable space"),Components(F,W),1);
 TestEqual(TEXT("Attachment does not add bottlenecks"),Bridges(F,W).Num(),0);
 F.Reset();W.Reset();Rectangle(30,30,F,W);W.Add(FIntVector(14,14,0));W.Add(FIntVector(14,15,0));
 auto Elbow=FindFloatingWallExtension(F,W,W,[](const FIntVector&){return true;});
 TestEqual(TEXT("Distant floating wall gains a two-cell arm"),Elbow.Edges.Num(),2);
 if(!Elbow.Edges.IsEmpty())TestEqual(TEXT("Arm turns the straight wall into an L"),Elbow.Edges[0].Z,1);
 TestTrue(TEXT("Elbow selection is deterministic"),Elbow.Edges==FindFloatingWallExtension(F,W,W,[](const FIntVector&){return true;}).Edges);
 TestEqual(TEXT("Protected props reject extensions"),FindFloatingWallExtension(F,W,W,[](const FIntVector&){return false;}).Edges.Num(),0);
 return true;
}
#endif
