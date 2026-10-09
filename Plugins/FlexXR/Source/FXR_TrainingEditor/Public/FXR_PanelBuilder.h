// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FXR_PanelBuilder.generated.h"

/**
 * UFXR_PanelBuilder — generates WBP_FXR_TrainingPanel, the framework's default validation panel.
 *
 * So the framework ships a readable panel rather than an empty parent class and a paragraph telling
 * you to build one. The layout is code because the design *is* a set of decisions worth keeping
 * under version control and worth explaining; see the long comment in the implementation for where
 * every number comes from.
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
	 * Refuses to overwrite unless asked. The point of generating a panel is that someone then tunes
	 * it, and a regenerate that silently discarded that tuning would be worse than no generator.
	 *
	 * @return true when an asset was written. False is also the answer when one already existed.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	static bool BuildDefaultTrainingPanel(bool bOverwrite = false);
};
