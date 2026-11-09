#include "DungeonLightPlacementLibrary.h"

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDungeonLightPlacementTest,
	"EscapeOnStep.Dungeon.LightPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDungeonLightPlacementTest::RunTest(const FString& Parameters)
{
	bool Budget = false;
	const TArray<FVector> Candidates = { FVector(200,0,30), FVector(0,0,30), FVector(100,0,30), FVector(300,0,30) };
	const TArray<FVector> Original = Candidates;
	auto Plan = [&](const TArray<FVector>& C, const TArray<FVector>& E, double D, int32 N, int64 B = 2000000)
	{
		return UDungeonLightPlacementLibrary::PlanCorridorLightPositions(C,E,D,N,Budget,B);
	};
	const TArray<FVector> Expected = { FVector(0,0,30), FVector(300,0,30), FVector(100,0,30), FVector(200,0,30) };
	TestTrue(TEXT("Deterministic empty-existing seed, ties and inclusive spacing"), Plan(Candidates,{},100,10) == Expected);
	TestTrue(TEXT("Input unchanged"), Candidates == Original);
	TArray<FVector> Reordered = {Candidates[3],Candidates[2],Candidates[1],Candidates[0]};
	TestTrue(TEXT("Independent of input order"), Plan(Reordered,{},100,10) == Expected);
	TestTrue(TEXT("XY only and existing overlap excluded"), Plan(Candidates,{FVector(0,0,9999)},300,10) == TArray<FVector>{FVector(300,0,30)});
	TestEqual(TEXT("Zero limit"), Plan(Candidates,{},0,0).Num(),0);
	TestEqual(TEXT("Empty candidates"), Plan({}, {},0,10).Num(),0);
	TestEqual(TEXT("Negative spacing becomes zero"), Plan(Candidates,{},-1,10).Num(),4);
	TestEqual(TEXT("Invalid spacing"), Plan(Candidates,{},std::numeric_limits<double>::infinity(),10).Num(),0);
	TestTrue(TEXT("Duplicates keep lowest Z; invalid position ignored"),
		Plan({FVector(1,2,9),FVector(1,2,3),FVector(std::numeric_limits<double>::quiet_NaN(),0,0)}, {},0,10)
		== TArray<FVector>{FVector(1,2,3)});
	TestEqual(TEXT("Initialization budget rejects incomplete cache"), Plan(Candidates,{FVector::ZeroVector},1,10,3).Num(),0);
	TestTrue(TEXT("Initialization reports budget exhaustion"), Budget);
	TestEqual(TEXT("Budget returns valid prefix"), Plan(Candidates,{},1,10,0).Num(),1);
	TestTrue(TEXT("Update reports budget exhaustion"), Budget);

	// Compare cached implementation with independent full-distance recomputation.
	TArray<FVector> Grid;
	for (int32 X=0; X<17; ++X)
		for (int32 Y=0; Y<13; ++Y) { Grid.Add(FVector(X*113,Y*97,20)); }
	TArray<FVector> Existing = {FVector(41,37,1000), FVector(800,500,-1000)};
	TArray<FVector> Reference;
	TArray<FVector> AllLights = Existing;
	TArray<bool> Used;
	Used.Init(false,Grid.Num());
	for (int32 Step=0; Step<70; ++Step)
	{
		int32 Best=INDEX_NONE;
		double BestD=-1;
		for (int32 I=0; I<Grid.Num(); ++I)
		{
			if (Used[I]) { continue; }
			double D=std::numeric_limits<double>::infinity();
			for (const FVector& L : AllLights) { D=FMath::Min(D,FVector::DistSquaredXY(Grid[I],L)); }
			if (D>0 && D>BestD) { Best=I; BestD=D; }
		}
		if (Best==INDEX_NONE || BestD<180.0*180.0) { break; }
		Used[Best]=true;
		Reference.Add(Grid[Best]);
		AllLights.Add(Grid[Best]);
	}
	TestTrue(TEXT("Cache matches full recomputation"), Plan(Grid,Existing,180,70)==Reference);
	TestFalse(TEXT("Normal workload fits budget"),Budget);
	TArray<FVector> Many;
	for (int32 I=0; I<300; ++I) { Many.Add(FVector(I,0,0)); }
	TestEqual(TEXT("Hard placement cap"),Plan(Many,{},0,1000).Num(),256);
	return true;
}
#endif
