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
			"FXR_Core",
			"FXR_Interaction",
			"FXR_UI",
			// UUserWidget, for the validation panel. FXR_UI already depends on UMG publicly, but a
			// transitive dep gives headers and still fails to link.
			"UMG",
			// URadialSlider, which is the panel's score ring. An engine module rather than a
			// plugin, and the reason the ring needs no material of its own: SRadialSlider already
			// draws an arc, and UMG has no other way to draw one.
			"AdvancedWidgets",
			// FXR_PanelStyle.h builds FSlateBrush and FSlateFontInfo inline in a public header, so
			// the constructors are linked here and not merely declared. UMG above gives the headers
			// and still fails to link - the same transitive-dependency trap as always.
			"Slate",
			"SlateCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
		});
	}
}
