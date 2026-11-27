#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CourtyardISMEditorLibrary.generated.h"
UCLASS()
class ESCAPE_ON_STEP_DEMO_API UCourtyardISMEditorLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
  public:
	UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Editor")
	static bool ExtractCurrentModule(UWorld *World, const FString &CatalogPath);
	UFUNCTION(BlueprintCallable, Category = "Courtyard ISM|Editor")
	static bool AdaptPrototypeBlueprint(UBlueprint *Blueprint);
};
