// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_UI : ModuleRules
{
	public FXR_UI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// One-way dependency: FXR_UI may reference FXR_Interaction (and below), never FXR_Training.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// Directly references UFXR_MotionSettings and FFXR_Motion. A transitive public dep through
			// FXR_Interaction gives header access but still fails to link.
			"FXR_Core",
			"FXR_Interaction",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Slate",
			"SlateCore"
		});
	}
}
