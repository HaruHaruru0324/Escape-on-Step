#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "BackroomSynchronizedFlicker.generated.h"
class ULightComponent;
class UStaticMeshComponent;
class UMaterialInstanceDynamic;

USTRUCT()
struct FBackroomFlickerPair
{
 GENERATED_BODY()
 UPROPERTY() TObjectPtr<ULightComponent> Light;
 UPROPERTY() TObjectPtr<UStaticMeshComponent> Fixture;
 UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Material;
 float BaseIntensity=0;
 float Speed=1;
 float Bias=.2f;
};

/** One controller per fixture owner. Both outputs receive the same brightness each frame. */
UCLASS(ClassGroup=(Lighting))
class ESCAPE_ON_STEP_DEMO_API UBackroomSynchronizedFlicker : public UActorComponent
{
 GENERATED_BODY()
public:
 UBackroomSynchronizedFlicker();
 UFUNCTION(BlueprintCallable,Category="Dungeon|Lighting")
 static void SetSynchronizedPair(ULightComponent* Light,UStaticMeshComponent* Fixture,bool bFlicker);
 virtual void TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction) override;
private:
 UPROPERTY(Transient) TArray<FBackroomFlickerPair> Pairs;
 void Apply(FBackroomFlickerPair& Pair,float Brightness);
};
