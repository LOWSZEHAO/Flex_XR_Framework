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
			// Uses UFXR_MotionSettings / FFXR_Motion directly; the transitive dependency through
			// FXR_Interaction gives headers but doesn't link.
			"FXR_Core",
			"FXR_Interaction",
			"UMG"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// EKeys::LeftMouseButton, used to send a hand's press to Slate.
			"InputCore",
			"Slate",
			"SlateCore"
		});
	}
}
