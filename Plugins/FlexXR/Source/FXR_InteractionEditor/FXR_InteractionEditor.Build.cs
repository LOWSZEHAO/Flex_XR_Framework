// Copyright (c) 2026 Low Sze Hao. All rights reserved.

using UnrealBuildTool;

public class FXR_InteractionEditor : ModuleRules
{
	public FXR_InteractionEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Editor-only module: viewport gizmos / component visualizers for FXR_Interaction.
		// Keeps editor code out of the lean runtime modules (CODING_STANDARDS 4).
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"FXR_Core",       // interactor query-shape gizmo (grab sphere + poke tip)
			"FXR_Interaction"
		});
	}
}
