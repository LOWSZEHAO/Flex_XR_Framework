# Copyright (c) 2026 Low Sze Hao. All rights reserved.
#
# Generates WBP_FXR_TrainingPanel, the framework's default validation panel, so FlexXR ships a
# readable panel instead of an empty parent class and a paragraph telling you to build one.
#
# Run from the editor console:  py "G:/Flex_XR_Framework/Tools/make_fxr_training_panel.py"
#
# **The layout is not here.** It lives in UFXR_PanelBuilder, in the FXR_TrainingEditor module,
# together with the reasoning behind every size and colour. That is not a preference: a widget
# blueprint's contents live in UWidgetBlueprint::WidgetTree and UWidgetTree::RootWidget, both bare
# UPROPERTY()s with no edit or blueprint flag, so neither is exported to script, and UWidgetTree has
# no UFUNCTION at all. Python can create the asset and then do nothing whatsoever with it. Unlike
# regen_fxr_materials.py, which really is a Python tool, this file is a doorbell.
#
# It refuses to overwrite. The point of generating a panel is that someone then tunes it, and a
# regenerate that silently discarded that tuning would be worse than no generator. Pass True below
# if you do want the authored version thrown away.

import unreal

OVERWRITE = False

if unreal.FXR_PanelBuilder.build_default_training_panel(OVERWRITE):
    unreal.log_warning('FXR_PANEL: built. Drop it in an FXR_Panel with Panel Width 60.')
else:
    # Already present, or the builder logged why not. Said twice on purpose: a console that scrolls
    # is a console where "nothing happened" and "nothing needed to happen" look identical.
    unreal.log_warning('FXR_PANEL: nothing written, see the log above.')
