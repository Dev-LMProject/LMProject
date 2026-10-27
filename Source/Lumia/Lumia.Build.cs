// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Lumia : ModuleRules
{
    public Lumia(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "GameplayTags"
        });
    }
}
