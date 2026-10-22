// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Lumia : ModuleRules
{
	public Lumia(ReadOnlyTargetRules Target) : base(Target)
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

		PublicIncludePaths.AddRange(new string[] {
			"Lumia",
			"Lumia/Variant_Platforming",
			"Lumia/Variant_Platforming/Animation",
			"Lumia/Variant_Combat",
			"Lumia/Variant_Combat/AI",
			"Lumia/Variant_Combat/Animation",
			"Lumia/Variant_Combat/Gameplay",
			"Lumia/Variant_Combat/Interfaces",
			"Lumia/Variant_Combat/UI",
			"Lumia/Variant_SideScrolling",
			"Lumia/Variant_SideScrolling/AI",
			"Lumia/Variant_SideScrolling/Gameplay",
			"Lumia/Variant_SideScrolling/Interfaces",
			"Lumia/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
