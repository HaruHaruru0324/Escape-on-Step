#include "DungeonGenerationRetrySubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

void UDungeonGenerationRetrySubsystem::RequestRegeneration(UWorld* World)
{
 if (!IsInGameThread() || !IsValid(World) || !World->IsGameWorld() ||
     World->GetNetMode() == NM_Client || FailedWorld.Get() == World)
  return;

 const FString Map = UWorld::RemovePIEPrefix(World->GetOutermost()->GetName());
 if (RetryMap != Map)
 {
  RetryMap = Map;
  RetryCount = 0;
  bGenerationFailed = false;
 }
 FailedWorld = World;
 if (RetryCount >= MaxRetries)
 {
  bGenerationFailed = true;
  UE_LOG(LogTemp, Error, TEXT("GOAL_RETRY exhausted after %d reloads; gameplay paused (map=%s)"), RetryCount, *Map);
  UGameplayStatics::SetGamePaused(World, true);
  if (GEngine)
   GEngine->AddOnScreenDebugMessage(-1, 120.f, FColor::Red,
    TEXT("Dungeon generation failed: no safe goal location after 5 retries. Restart the game."));
  return;
 }

 ++RetryCount;
 UE_LOG(LogTemp, Warning, TEXT("GOAL_RETRY scheduled %d/%d map=%s"), RetryCount, MaxRetries, *Map);
 // Do not reload inside BeginPlay / the Blueprint's active generation stack.
 // A fresh level removes all old generated actors and components together.
 ReloadTimer = World->GetTimerManager().SetTimerForNextTick(
  FTimerDelegate::CreateWeakLambda(this, [this, WeakWorld=TWeakObjectPtr<UWorld>(World), Map]()
  {
   UWorld* Current = WeakWorld.Get();
   if (!Current || FailedWorld != WeakWorld || GetWorld() != Current || Current->bIsTearingDown)
    return;
   UE_LOG(LogTemp, Display, TEXT("GOAL_RETRY reloading %d/%d"), RetryCount, MaxRetries);
   UGameplayStatics::OpenLevel(Current, FName(*Map), true);
  }));
}

void UDungeonGenerationRetrySubsystem::MarkGoalLocationFound(UWorld* World)
{
 // A duplicate selection in a rejected world must not cancel its pending reload.
 if (!World || FailedWorld.Get() == World)
  return;
 if (RetryCount > 0)
  UE_LOG(LogTemp, Display, TEXT("GOAL_RETRY recovered after %d reloads"), RetryCount);
 RetryCount = 0;
 bGenerationFailed = false;
 RetryMap.Reset();
 FailedWorld.Reset();
}

void UDungeonGenerationRetrySubsystem::Deinitialize()
{
 if (UWorld* World = FailedWorld.Get())
  World->GetTimerManager().ClearTimer(ReloadTimer);
 FailedWorld.Reset();
 Super::Deinitialize();
}
