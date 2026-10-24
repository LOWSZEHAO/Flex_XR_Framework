// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_Training : ModuleRules
{
	public FXR_Training(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Top of the chain. Nothing else in the framework depends on FXR_Training.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"FXR_Core",
			"FXR_Interaction",
			"FXR_UI",
			// UUserWidget for the panel. FXR_UI already pulls UMG in, but a transitive dependency
			// gives the headers and still fails to link.
			"UMG",
			// URadialSlider for the score ring (draws the arc, so no custom material needed).
			"AdvancedWidgets",
			// FXR_PanelStyle.h builds FSlateBrush / FSlateFontInfo inline in a public header, so these
			// need to be linked here directly.
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
