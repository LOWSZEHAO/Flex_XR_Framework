// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_Locomotion : ModuleRules
{
	public FXR_Locomotion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// One-way dependency: FXR_Locomotion is a sibling of FXR_UI, above FXR_Interaction.
		// It may reference FXR_Interaction (climbing reuses FXR_Grab) and FXR_Core — never
		// FXR_UI or FXR_Training.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FXR_Core",
			"FXR_Interaction"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"EnhancedInput",
			"NavigationSystem"
		});
	}
}
