// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FXR_PanelBuilder.generated.h"

/**
 * UFXR_PanelBuilder - generates WBP_FXR_TrainingPanel (the default training panel) and its font.
 *
 * The layout is built in code so it's versioned and reproducible. Sizes and colours come from
 * FXR_PanelStyle.h; the reference mockup is UI_Design/panel.html.
 *
 * This has to be C++. A widget blueprint's tree (UWidgetBlueprint::WidgetTree,
 * UWidgetTree::RootWidget) isn't exposed to Python, UWidgetTree has no UFUNCTIONs, and
 * ConstructWidget is a template. Python can create the asset but can't fill it.
 *
 * Run from the editor console:
 *     unreal.FXR_PanelBuilder.build_default_training_panel(False)
 * or through Tools/make_fxr_training_panel.py.
 */
UCLASS()
class UFXR_PanelBuilder : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Build the panel at /FlexXR/UI/WBP_FXR_TrainingPanel. Imports the font first if needed (without
	 * it every glyph renders as a box). Won't overwrite an existing panel unless bOverwrite is set,
	 * so hand tweaks aren't lost.
	 *
	 * @return true if an asset was written (false also when it already existed).
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	static bool BuildDefaultTrainingPanel(bool bOverwrite = false);

	/**
	 * Import Montserrat from Resources/Fonts into /FlexXR/UI/Fonts/F_Montserrat with three faces:
	 * Medium, SemiBold and ExtraBold (the only weights the design uses).
	 *
	 * Runtime font cache rather than Offline, since the panel is read at varying distances. Face
	 * data is embedded, so the .ttf files are only needed to reimport (they stay in the repo with
	 * the OFL licence).
	 *
	 * @return true if the font exists afterwards.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	static bool ImportPanelFonts(bool bOverwrite = false);
};
