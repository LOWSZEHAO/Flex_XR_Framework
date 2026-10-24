// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_TrainingEditor : ModuleRules
{
	public FXR_TrainingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor-only tools for FXR_Training. Sits above FXR_Training instead of inside
		// FXR_InteractionEditor, which shouldn't depend on training either.
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"AssetTools",      // CreateAsset, for the generated panel
			"UMG",             // the widgets being assembled
			"UMGEditor",       // UWidgetBlueprint and its factory
			"Slate",
			"SlateCore",       // FSlateFontInfo, FSlateRoundedBoxBrush
			"AdvancedWidgets", // URadialSlider, the score ring
			"AssetRegistry",   // so a generated asset appears without a rescan
			"Projects",        // IPluginManager, to find the font sources in Resources/Fonts
			"PropertyEditor",  // the GM event dropdown
			"InputCore",       // SComboBox instantiates key handling, and links against EKeys
			"FXR_Core",        // LogFXR
			"FXR_Training"     // UFXR_TrainingPanel and FXR_PanelStyle
		});
	}
}
