#if WITH_EDITOR
#include "DungeonAdjacentLayout.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
static int32 SnapshotIndex = 0;
static FAutoConsoleCommand LayoutReviewSnapshot(TEXT("DungeonReview.Snapshot"),TEXT("Record deterministic layout fingerprints for refactoring review."),FConsoleCommandDelegate::CreateLambda([]()
{
 FString Report;
 const auto Snapshot = [&](int32 Target,int32 Seed)
 {
  const auto L=DungeonAdjacent::Build({0,0},{39,39},Target,Seed);
  FString Data=FString::Printf(TEXT("%d,%d,%d,%d,%d;"),L.Corridors,L.DeadEnds,L.Branches,L.CorridorRooms,L.Rooms.Num());
  for(const auto& R:L.Rooms)Data+=FString::Printf(TEXT("R%d,%d,%d,%d,%d;"),R.X,R.Y,R.W,R.H,R.CutCorner);
  for(const auto& D:L.Doors){Data+=FString::Printf(TEXT("D%d,%d:"),D.A,D.B);for(auto E:D.Edges)Data+=FString::Printf(TEXT("%d,%d,%d;"),E.X,E.Y,E.Z);}
  TArray<FIntPoint> Cells;L.Cells.GetKeys(Cells);Cells.Sort([](auto A,auto B){return A.X!=B.X?A.X<B.X:A.Y<B.Y;});
  for(auto P:Cells)Data+=FString::Printf(TEXT("C%d,%d,%d;"),P.X,P.Y,L.Cells.FindChecked(P));
  auto Walls=L.Walls.Array();Walls.Sort([](auto A,auto B){return A.X!=B.X?A.X<B.X:A.Y!=B.Y?A.Y<B.Y:A.Z<B.Z;});
  for(auto E:Walls)Data+=FString::Printf(TEXT("W%d,%d,%d;"),E.X,E.Y,E.Z);
  Report+=FString::Printf(TEXT("%d,%d=%s\n"),Target,Seed,*FMD5::HashAnsiString(*Data));
 };
 for(int32 Seed=0;Seed<100;++Seed)Snapshot(0,Seed);
 for(int32 Target:{1,14,28})for(int32 Seed:{0,37,28976})Snapshot(Target,Seed);
 const FString Dir=FPaths::ProjectSavedDir()/TEXT("CodexBackups/20261112_CppReview");IFileManager::Get().MakeDirectory(*Dir,true);
 while(IFileManager::Get().FileExists(*(Dir/FString::Printf(TEXT("layout-snapshot-%d.txt"),SnapshotIndex))))++SnapshotIndex;
 const FString Path=Dir/FString::Printf(TEXT("layout-snapshot-%d.txt"),SnapshotIndex++);
 FFileHelper::SaveStringToFile(Report,*Path);UE_LOG(LogTemp,Display,TEXT("CPP_REVIEW snapshot=%s layouts=109"),*Path);
}));
#endif

