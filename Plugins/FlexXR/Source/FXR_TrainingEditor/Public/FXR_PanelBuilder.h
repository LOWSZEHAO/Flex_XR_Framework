// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FXR_PanelBuilder.generated.h"

/**
 * UFXR_PanelBuilder — generates WBP_FXR_TrainingPanel, the framework's default training panel, and
 * the font it is set in.
 *
 * So the framework ships a readable panel rather than an empty parent class and a paragraph telling
 * you to build one. The layout is code because the design *is* a set of decisions worth keeping
 * under version control and worth explaining; the tokens live in `FXR_PanelStyle.h` and the shape
 * each screen takes is explained where it is built.
 *
 * The design it builds is `UI_Design/panel.html`, drawn at the panel's real size. If the two
 * disagree, the page is right.
 *
 * **Why this is C++ rather than a Python tool like the material generator.** It cannot be Python. A
 * widget blueprint's contents live in `UWidgetBlueprint::WidgetTree` and `UWidgetTree::RootWidget`,
 * both bare `UPROPERTY()`s with no edit or blueprint flag, so neither is exported to script, and
 * `UWidgetTree` has no `UFUNCTION` at all. `ConstructWidget` is a C++ template, which settles it.
 * Python can create the asset and then do nothing with it.
 *
 * Reached from the editor console as:
 *     unreal.FXR_PanelBuilder.build_default_training_panel(False)
 * or through Tools/make_fxr_training_panel.py, which is that line plus the reporting.
 */
UCLASS()
class UFXR_PanelBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Build the default panel at /FlexXR/UI/WBP_FXR_TrainingPanel.
	 *
	 * Imports the font first if it is missing, because a panel without it renders every glyph as a
	 * missing-glyph box — which is not a subtle failure, but is a confusing one.
	 *
	 * Refuses to overwrite unless asked. The point of generating a panel is that someone then tunes
	 * it, and a regenerate that silently discarded that tuning would be worse than no generator.
	 *
	 * @return true when an asset was written. False is also the answer when one already existed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	static bool BuildDefaultTrainingPanel(bool bOverwrite = false);

	/**
	 * Import Montserrat from the plugin's Resources/Fonts and compose it into
	 * /FlexXR/UI/Fonts/F_Montserrat, with Medium, SemiBold and ExtraBold as its three typefaces.
	 *
	 * Three faces, not the family: the design uses 500, 600 and 800 and no italics, and an unused
	 * face is still a cooked megabyte. The composite is a Runtime font — Offline caching is the old
	 * UFont path and bakes a texture at fixed sizes, which is the wrong trade for a panel whose
	 * type is read at a distance that varies.
	 *
	 * The face data is embedded in the asset (`Inline`), so the .ttf files are needed only to
	 * reimport. They stay in the repo for that and because the SIL Open Font License travels with
	 * the font.
	 *
	 * @return true when the composite font exists afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	static bool ImportPanelFonts(bool bOverwrite = false);
};
