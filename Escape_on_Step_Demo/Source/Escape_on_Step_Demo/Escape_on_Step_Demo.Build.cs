// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Escape_on_Step_Demo : ModuleRules
{
	public Escape_on_Step_Demo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[] { "UnrealEd", "BlueprintGraph", "KismetCompiler" });
		}

		PublicIncludePaths.AddRange(new string[] {
			"Escape_on_Step_Demo",
			"Escape_on_Step_Demo/Variant_Horror",
			"Escape_on_Step_Demo/Variant_Horror/UI",
			"Escape_on_Step_Demo/Variant_Shooter",
			"Escape_on_Step_Demo/Variant_Shooter/AI",
			"Escape_on_Step_Demo/Variant_Shooter/UI",
			"Escape_on_Step_Demo/Variant_Shooter/Weapons"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
