// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_TrainingEditor : ModuleRules
{
	public FXR_TrainingEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor-only module: authoring tools for FXR_Training. Keeps editor code out of the lean
		// runtime modules (CODING_STANDARDS 4), and sits above FXR_Training rather than inside
		// FXR_InteractionEditor, which must not learn that training exists either.
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
			"FXR_Core",        // LogFXR
			"FXR_Training"     // UFXR_TrainingPanel, the generated panel's parent
		});
	}
}
