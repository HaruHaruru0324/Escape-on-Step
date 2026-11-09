#include "DungeonLightRegions.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDungeonLightRegionTest,"EscapeOnStep.Dungeon.LightRegions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDungeonLightRegionTest::RunTest(const FString& Parameters)
{
	using namespace DungeonLightRegions;
	auto Build=[](const TArray<FIntPoint>& Keys)
	{
		TArray<FCell> C; TMap<FIntPoint,int32> Map;
		for (FIntPoint K:Keys) { FCell V; V.Key=K; V.Position=FVector(K.X*300,K.Y*300,270); Map.Add(K,C.Add(V)); }
		const FIntPoint D[]={{1,0},{0,1},{-1,0},{0,-1}};
		for (FCell& V:C) for (int32 I=0;I<4;++I) if (int32* J=Map.Find(V.Key+D[I])) V.Neighbors[I]=*J;
		return C;
	};
	TArray<FIntPoint> Keys;
	for(int32 X=0;X<12;++X) for(int32 Y=0;Y<7;++Y) Keys.Add({X,Y});
	auto C=Build(Keys); TArray<int32> Labels; TArray<FRegion> Regions;
	Partition(C,Labels,Regions);
	TestEqual(TEXT("Broad joined rectangular rooms form one region"),Regions.Num(),1);
	TestEqual(TEXT("All floor cells included"),Regions[0].Cells.Num(),84);
	// A real partition wall, even though floor cells are still adjacent.
	for(int32 I=0;I<C.Num();++I) if(C[I].Key.X==5) { int32 J=C[I].Neighbors[0]; C[I].Neighbors[0]=INDEX_NONE; C[J].Neighbors[2]=INDEX_NONE; }
	Partition(C,Labels,Regions); TestEqual(TEXT("Wall splits adjacent floors"),Regions.Num(),2);
	for (int32 Width:{1,2,3})
	{
		Keys.Reset();
		for(int32 X=0;X<13;++X) for(int32 Y=0;Y<7;++Y)
			if(X<5 || X>7 || (Y>=2 && Y<2+Width)) Keys.Add({X,Y});
		C=Build(Keys); Partition(C,Labels,Regions);
		TestEqual(*FString::Printf(TEXT("Opening width %d"),Width),Regions.Num(),Width<3?3:1);
		for (const FRegion& R:Regions) TestTrue(TEXT("Representative belongs to region"),R.Cells.Contains(R.Representative));
	}
	Keys.Reset();for(int32 X=0;X<30;++X) Keys.Add({X,0});
	C=Build(Keys);Partition(C,Labels,Regions);TestEqual(TEXT("Long narrow corridor one region"),Regions.Num(),1);
	Keys={{0,0},{10,0}};C=Build(Keys);Partition(C,Labels,Regions);TestEqual(TEXT("Disconnected tiny rooms"),Regions.Num(),2);
	Partition({},Labels,Regions);TestTrue(TEXT("Empty floor"),Regions.IsEmpty() && Labels.IsEmpty());
	return true;
}
#endif
