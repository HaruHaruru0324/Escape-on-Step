#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameFramework/Actor.h"
#include "CourtyardLayout.h"
#include "CourtyardGenerator.generated.h"

class ULevelStreamingDynamic;
class UStaticMeshComponent;
class ACharacter;

/** 配置するレベルと地面の参照をデータ化。既存レベル/マテリアル自体は変更しない。 */
UCLASS(BlueprintType)
class ESCAPE_ON_STEP_DEMO_API UCourtyardTheme : public UDataAsset
{
	GENERATED_BODY()
  public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") TArray<TSoftObjectPtr<UWorld>> Facades;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") TArray<TSoftObjectPtr<UWorld>> Portals;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") TSoftObjectPtr<UWorld> Passage;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") TSoftObjectPtr<UWorld> RoomFacade;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces") TObjectPtr<UStaticMesh> FloorMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces") TObjectPtr<UMaterialInterface> StreetMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces") TObjectPtr<UMaterialInterface> RoomMaterial;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surfaces") TObjectPtr<UMaterialInterface> CeilingMaterial;
};

USTRUCT(BlueprintType)
struct FCourtyardModulePlacement
{
	GENERATED_BODY()
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") TSoftObjectPtr<UWorld> Level;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") FTransform Transform;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") bool bPassage = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") bool bHideStartArch = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Modules") bool bHideEndArch = false;
};

/** BPはこのアクターを継承する。データ生成と描画の入口を分け、後からBP生成へ差し替えられる。 */
UCLASS(Blueprintable)
class ESCAPE_ON_STEP_DEMO_API ACourtyardGenerator : public AActor
{
	GENERATED_BODY()
  public:
	ACourtyardGenerator();
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Layout") int32 Seed = -1;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Layout") FCourtyardSettings Settings;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Layout",
			  meta = (ClampMin = "200", ClampMax = "800"))
	float CellSize = 400;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Modules") TObjectPtr<UCourtyardTheme> Theme;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Gameplay") bool bGenerateOnBeginPlay = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Courtyard|Gameplay") TSubclassOf<AActor> GoalClass;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") FCourtyardPlan Plan;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") TArray<FCourtyardModulePlacement> ModulePlacements;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") bool bGenerationComplete = false;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") bool bGenerationFailed = false;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") FString GenerationError;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") int32 ActualSeed = 0;
	UPROPERTY(BlueprintReadOnly, Category = "Courtyard|Result") int32 PhysicallyReachableCells = 0;

	UFUNCTION(BlueprintCallable, Category = "Courtyard") bool GenerateCourtyard();
	// BPで作ったPlanをここへ渡せる。C++のBuildCourtyardPlanを使うことは必須ではない。
	UFUNCTION(BlueprintCallable, Category = "Courtyard") bool RenderCourtyardPlan(const FCourtyardPlan& NewPlan);
	UFUNCTION(BlueprintCallable, Category = "Courtyard") void ClearCourtyard();
	UFUNCTION(BlueprintPure, Category = "Courtyard")
	FVector GetCellWorldPosition(FIntPoint Cell, float Height = 0) const;
	UFUNCTION(BlueprintImplementableEvent, Category = "Courtyard") void OnCourtyardReady();

  private:
	UPROPERTY(Transient) TArray<TObjectPtr<ULevelStreamingDynamic>> LoadedModules;
	UPROPERTY(Transient) TArray<TObjectPtr<UStaticMeshComponent>> GeneratedMeshes;
	UPROPERTY(Transient) TArray<TObjectPtr<AActor>> GeneratedActors;
	TSet<int32> PreparedModules;
	FTimerHandle LoadingTimer;
	double LoadingStarted = 0;
	int32 GenerationSerial = 0;
	bool bRendering = false;
	TWeakObjectPtr<ACharacter> FrozenPlayer;
	uint8 PreviousMovementMode = 0;
	void PlanModules();
	void PollModules();
	bool ValidatePhysicalLayout();
	void SetPlayerFrozen(bool Frozen);
	bool Fail(const FString& Reason);
	void AddSurface(FIntPoint Cell, float Height, UMaterialInterface* Material, bool Ceiling = false);
};
