#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DungeonGenerationRetrySubsystem.generated.h"

/** Retry state survives level reloads, but not a new game/PIE session. */
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UDungeonGenerationRetrySubsystem : public UGameInstanceSubsystem
{
 GENERATED_BODY()
public:
 void RequestRegeneration(UWorld* World);
 void MarkGoalLocationFound(UWorld* World);
 virtual void Deinitialize() override;

 UPROPERTY(BlueprintReadOnly, Category="Dungeon|Generation")
 int32 RetryCount = 0;

 UPROPERTY(BlueprintReadOnly, Category="Dungeon|Generation")
 bool bGenerationFailed = false;

 static constexpr int32 MaxRetries = 5;
private:
 FString RetryMap;
 TWeakObjectPtr<UWorld> FailedWorld;
 FTimerHandle ReloadTimer;
};
