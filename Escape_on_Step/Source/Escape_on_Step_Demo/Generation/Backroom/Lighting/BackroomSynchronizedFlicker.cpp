#include "Generation/Backroom/Lighting/BackroomSynchronizedFlicker.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

UBackroomSynchronizedFlicker::UBackroomSynchronizedFlicker()
{
 PrimaryComponentTick.bCanEverTick=true;
 PrimaryComponentTick.bStartWithTickEnabled=false;
}
void UBackroomSynchronizedFlicker::Apply(FBackroomFlickerPair& Pair,float Brightness)
{
 Pair.Material->SetScalarParameterValue(TEXT("SyncBrightness"),Brightness);
 Pair.Light->SetIntensity(Pair.BaseIntensity*Brightness);
}
void UBackroomSynchronizedFlicker::SetSynchronizedPair(ULightComponent* Light,UStaticMeshComponent* Fixture,bool bFlicker)
{
 if(!IsValid(Fixture))return;
 AActor* Owner=Fixture->GetOwner();
 if(!IsValid(Owner))return;
 auto* Controller=Owner->FindComponentByClass<UBackroomSynchronizedFlicker>();
 float Base=IsValid(Light)?Light->Intensity:0;
 if(Controller)
 {
  // Reconfiguration must restore the original intensity, not a dimmed frame's value.
  for(int32 I=Controller->Pairs.Num()-1;I>=0;--I)
  {
   auto& Old=Controller->Pairs[I];
   if(Old.Light==Light||Old.Fixture==Fixture)
   {
    if(IsValid(Old.Light)&&IsValid(Old.Material))Controller->Apply(Old,1);
    if(Old.Light==Light)Base=Old.BaseIntensity;
    Controller->Pairs.RemoveAtSwap(I);
   }
  }
 }
 // The GPU light-function path is deliberately disabled: intensity is the sole driver.
 if(IsValid(Light)){Light->SetMobility(EComponentMobility::Movable);Light->SetLightFunctionMaterial(nullptr);Light->SetIntensity(Base);}
 auto* Material=Fixture->CreateAndSetMaterialInstanceDynamic(0);
 if(!Material)return;
 Material->SetScalarParameterValue(TEXT("SyncEnabled"),1);
 Material->SetScalarParameterValue(TEXT("SyncBrightness"),1);
 if(bFlicker&&IsValid(Light))
 {
  if(!Controller)
  {
   Controller=NewObject<UBackroomSynchronizedFlicker>(Owner);
   Owner->AddInstanceComponent(Controller);Controller->RegisterComponent();
  }
  FBackroomFlickerPair Pair;Pair.Light=Light;Pair.Fixture=Fixture;Pair.Material=Material;Pair.BaseIntensity=Base;
  Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Time")),Pair.Speed);
  Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("Sine")),Pair.Bias);
  Controller->Pairs.Add(Pair);
 }
 if(Controller){Controller->SetComponentTickEnabled(!Controller->Pairs.IsEmpty());Controller->TickComponent(0,LEVELTICK_All,nullptr);}
}
void UBackroomSynchronizedFlicker::TickComponent(float DeltaTime,ELevelTick TickType,FActorComponentTickFunction* ThisTickFunction)
{
 Super::TickComponent(DeltaTime,TickType,ThisTickFunction);
 const double Time=GetWorld()?GetWorld()->GetTimeSeconds():0;
 for(int32 I=Pairs.Num()-1;I>=0;--I)
 {
  auto& Pair=Pairs[I];
  if(!IsValid(Pair.Light)||!IsValid(Pair.Fixture)||!IsValid(Pair.Material))
  {
   if(IsValid(Pair.Light))Pair.Light->SetIntensity(Pair.BaseIntensity);
   if(IsValid(Pair.Material))Pair.Material->SetScalarParameterValue(TEXT("SyncBrightness"),1);
   Pairs.RemoveAtSwap(I);continue;
  }
  const double Wave=FMath::Sin(Time*Pair.Speed*2*PI/3.0);
  const float Brightness=FMath::Pow(FMath::Clamp(float(Wave-FMath::FloorToDouble(Wave))+Pair.Bias,0.f,1.f),3.f);
  Apply(Pair,Brightness);
 }
 if(Pairs.IsEmpty())SetComponentTickEnabled(false);
}
