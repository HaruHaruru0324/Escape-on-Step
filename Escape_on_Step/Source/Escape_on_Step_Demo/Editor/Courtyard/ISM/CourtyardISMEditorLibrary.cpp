#include "Editor/Courtyard/ISM/CourtyardISMEditorLibrary.h"
#include "Generation/Courtyard/ISM/CourtyardISMGenerator.h"
#if WITH_EDITOR
#include "Components/DecalComponent.h"
#include "Components/LightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Blueprint.h"
#include "Engine/Brush.h"
#include "Engine/DecalActor.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/WorldSettings.h"
#include "K2Node_CallFunction.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
namespace
{
void ExportProperties(UObject *Object, TMap<FName, FString> &Values, bool BlueprintOnly)
{
	for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
	{
		FProperty *Property = *It;
		if (!Property->HasAnyPropertyFlags(CPF_Edit) ||
			Property->HasAnyPropertyFlags(CPF_Transient | CPF_DisableEditOnInstance | CPF_InstancedReference |
										  CPF_ContainsInstancedReference))
			continue;
		if (BlueprintOnly && !Cast<UBlueprintGeneratedClass>(Property->GetOwnerClass()))
			continue;
		if (Property->IsA<FObjectPropertyBase>() &&
			CastField<FObjectPropertyBase>(Property)->PropertyClass->IsChildOf(
				UActorComponent::StaticClass()))
			continue;
		FString Text;
		Property->ExportText_InContainer(0, Text, Object, Object, Object, PPF_None);
		Values.Add(Property->GetFName(), Text);
	}
}
bool SaveCatalog(UCourtyardISMModuleCatalog *Catalog)
{
	FSavePackageArgs Args;
	Args.TopLevelFlags = RF_Public | RF_Standalone;
	Args.SaveFlags = SAVE_NoError;
	return UPackage::SavePackage(
		Catalog->GetPackage(), Catalog,
		*FPackageName::LongPackageNameToFilename(Catalog->GetPackage()->GetName(),
												 FPackageName::GetAssetPackageExtension()),
		Args);
}
} // namespace
#endif
bool UCourtyardISMEditorLibrary::ExtractCurrentModule(UWorld *World, const FString &CatalogPath)
{
#if WITH_EDITOR
	if (!World || !CatalogPath.StartsWith(TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/")))
		return false;
	auto *Catalog = LoadObject<UCourtyardISMModuleCatalog>(nullptr, *CatalogPath);
	if (!Catalog)
	{
		UPackage *Package = CreatePackage(*FPackageName::ObjectPathToPackageName(CatalogPath));
		Catalog = NewObject<UCourtyardISMModuleCatalog>(
			Package, *FPackageName::ObjectPathToObjectName(CatalogPath), RF_Public | RF_Standalone);
	}
	FCourtyardISMModule Module;
	Module.SourceLevel = FSoftObjectPath(World);
	for (AActor *Actor : World->PersistentLevel->Actors)
	{
		if (!IsValid(Actor) || Actor->HasAnyFlags(RF_Transient) || Actor->IsEditorOnly() ||
			Actor->GetName().StartsWith(TEXT("ChaosDebugDrawActor_")) ||
			Actor->GetClass()->GetFName() == TEXT("ChaosDebugDrawActor") || Actor->IsA<AWorldSettings>() ||
			Actor->IsA<ALevelScriptActor>() || Actor == World->GetDefaultBrush())
			continue;
		if (auto *StaticActor = Cast<AStaticMeshActor>(Actor))
		{
			auto *Component = StaticActor->GetStaticMeshComponent();
			if (!Component || !Component->GetStaticMesh())
				continue;
			FCourtyardISMMesh Entry;
			Entry.Mesh = Component->GetStaticMesh();
			Entry.LocalTransform = Component->GetComponentTransform();
			for (int32 Slot = 0; Slot < Component->GetNumMaterials(); ++Slot)
				Entry.Materials.Add(Component->GetMaterial(Slot));
			Entry.CollisionProfile = Component->GetCollisionProfileName();
			Entry.CollisionEnabled = Component->GetCollisionEnabled();
			Entry.ObjectType = Component->GetCollisionObjectType();
			Entry.Responses = Component->GetCollisionResponseToChannels();
			Entry.bDisallowNanite = Component->bDisallowNanite;
			Entry.bAffectDistanceFieldLighting = Component->bAffectDistanceFieldLighting;
			Entry.bVisibleInRayTracing = Component->bVisibleInRayTracing;
			Entry.bCastHiddenShadow = Component->bCastHiddenShadow;
			Entry.TranslucencySortPriority = Component->TranslucencySortPriority;
			Entry.LightingChannels = Component->LightingChannels;
			Entry.bCastShadow = Component->CastShadow;
			Entry.bReceivesDecals = Component->bReceivesDecals;
			Entry.bVisible = Component->IsVisible();
			Entry.bHiddenInGame = Component->bHiddenInGame || Actor->IsHidden();
			Entry.bCustomDepth = Component->bRenderCustomDepth;
			Entry.Stencil = Component->CustomDepthStencilValue;
			Entry.bNavigation = Component->CanEverAffectNavigation();
			if (!Actor->GetActorEnableCollision())
				Entry.CollisionEnabled = ECollisionEnabled::NoCollision;
			const bool Arch = Entry.Mesh->GetName() == TEXT("SM_archT1");
			Entry.bStartArch = Arch && FMath::Abs(Actor->GetActorLocation().X) < 2;
			Entry.bEndArch = Arch && FMath::Abs(Actor->GetActorLocation().X - 400) < 2;
			Module.Meshes.Add(Entry);
		}
		else if (auto *DecalActor = Cast<ADecalActor>(Actor))
		{
			auto *Component = DecalActor->GetDecal();
			FCourtyardISMDecal Entry;
			Entry.Material = Component->GetDecalMaterial();
			Entry.LocalTransform = Component->GetComponentTransform();
			Entry.Size = Component->DecalSize;
			Entry.SortOrder = Component->SortOrder;
			Entry.FadeScreenSize = Component->FadeScreenSize;
			Entry.bVisible = Component->IsVisible();
			Entry.bHiddenInGame = Component->bHiddenInGame || Actor->IsHidden();
			Module.Decals.Add(Entry);
		}
		else if (Actor->GetClass()->ClassGeneratedBy)
		{
			FCourtyardISMActor Entry;
			Entry.ActorClass = Actor->GetClass();
			Entry.LocalTransform = Actor->GetActorTransform();
			Entry.bHidden = Actor->IsHidden();
			Entry.bCollision = Actor->GetActorEnableCollision();
			ExportProperties(Actor, Entry.Properties, true);
			TInlineComponentArray<ULightComponent *> Lights;
			Actor->GetComponents(Lights);
			for (ULightComponent *Light : Lights)
			{
				FCourtyardISMComponentProperties Values;
				Values.ComponentName = Light->GetFName();
				ExportProperties(Light, Values.Properties, false);
				Entry.Components.Add(Values);
			}
			Module.Actors.Add(Entry);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("ISM extraction unsupported actor %s"), *Actor->GetPathName());
			return false;
		}
	}
	Catalog->Modules.RemoveAll([&](const FCourtyardISMModule &Entry)
							   { return Entry.SourceLevel == Module.SourceLevel; });
	Catalog->Modules.Add(Module);
	Catalog->MarkPackageDirty();
	return SaveCatalog(Catalog);
#else
	return false;
#endif
}
bool UCourtyardISMEditorLibrary::AdaptPrototypeBlueprint(UBlueprint *Blueprint)
{
#if WITH_EDITOR
	if (!Blueprint ||
		!Blueprint->GetPathName().StartsWith(TEXT("/Game/AutoCreateMap/Courtyard/ISMPrototype/")))
		return false;
	Blueprint->ParentClass = ACourtyardISMGenerator::StaticClass();
	TArray<UEdGraph *> Graphs;
	Blueprint->GetAllGraphs(Graphs);
	int32 Replaced = 0;
	for (UEdGraph *Graph : Graphs)
		for (UEdGraphNode *Node : Graph->Nodes)
			if (auto *Call = Cast<UK2Node_CallFunction>(Node))
			{
				if ((Call->FunctionReference.GetMemberName() == TEXT("RenderCourtyardPlan") ||
					 Call->FunctionReference.GetMemberName() == TEXT("RenderCourtyardISMPlan")))
				{
					Call->SetFromFunction(ACourtyardISMGenerator::StaticClass()->FindFunctionByName(
						TEXT("RenderCourtyardISMPlan")));
					++Replaced;
				}
			}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Replaced > 0 && Blueprint->Status != BS_Error;
#else
	return false;
#endif
}
