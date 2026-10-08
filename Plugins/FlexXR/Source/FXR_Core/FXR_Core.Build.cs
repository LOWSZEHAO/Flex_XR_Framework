// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_Core : ModuleRules
{
	public FXR_Core(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// FXR_Core is the base of the FlexXR dependency chain:
		//   FXR_Training -> FXR_UI -> FXR_Interaction -> FXR_Core
		// As the platform layer it must never reference a higher FlexXR module.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"HeadMountedDisplay",
			"XRBase",
			// UFXR_MotionSettings derives from UDeveloperSettings, whose UCLASS lives here.
			"DeveloperSettings"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
