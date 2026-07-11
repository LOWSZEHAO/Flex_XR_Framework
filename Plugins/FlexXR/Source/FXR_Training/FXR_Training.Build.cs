// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_Training : ModuleRules
{
	public FXR_Training(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Top of the chain: FXR_Training watches interaction events via FXR_UI and below.
		// Nothing in the framework references FXR_Training — games simply never load it.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FXR_UI"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
