// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_Interaction : ModuleRules
{
	public FXR_Interaction(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// One-way dependency: FXR_Interaction may reference FXR_Core, never FXR_UI or FXR_Training.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FXR_Core",
			// UFXR_InteractionSettings derives from UDeveloperSettings, whose UCLASS lives here —
			// header access alone links against nothing.
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
