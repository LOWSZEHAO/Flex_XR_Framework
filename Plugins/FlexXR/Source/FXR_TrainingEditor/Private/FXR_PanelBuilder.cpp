// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PanelBuilder.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ProgressBar.h"
#include "Components/RadialSlider.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/Spacer.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Components/WidgetSwitcherSlot.h"
#include "Engine/Font.h"
#include "Engine/FontFace.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Types/FXR_LogChannels.h"
#include "UI/FXR_PanelStyle.h"
#include "UI/FXR_TrainingPanel.h"
#include "UObject/Package.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

/**
 * Panel layout notes.
 *
 * Four screens (Start, Explanation, Procedure, Complete) as children of one Widget Switcher in one
 * asset, so they share the frame, top bar, fonts and foot.
 *
 * Sizes and colours come from FXR_PanelStyle.h (900 x 870 px at 90 cm wide = 1 px per mm).
 *
 * A widget created with a name (New<T>(Tree, TEXT("StatusLabel"))) becomes a Blueprint variable,
 * which is what BindWidgetOptional on UFXR_TrainingPanel binds to. Everything else stays unnamed so
 * the variable list only shows what's actually driven.
 *
 * Differences from the mockup:
 * - No frosted glass blur (not possible on a world-space widget); translucent fill + inner ring.
 * - No check mark glyph in Montserrat, so finished rows show their number on a green disc.
 * - No fade at the bottom of the list (would need a texture or material).
 * - No line clamping in UMG: closed rows are one line with an ellipsis, the open row wraps.
 */

namespace
{
	using namespace FXR_PanelStyle;

	const FString UIPath = TEXT("/FlexXR/UI");
	const FString FontDir = TEXT("/FlexXR/UI/Fonts");
	const FString PanelAssetName = TEXT("WBP_FXR_TrainingPanel");
	const FString FontAssetName = TEXT("F_Montserrat");

	/** Montserrat's three weights, using the typeface names FXR_PanelStyle expects. */
	struct FFaceSpec
	{
		const TCHAR* Typeface;
		const TCHAR* File;
	};

	const FFaceSpec Faces[] = {
		{ TEXT("Medium"),    TEXT("Montserrat-Medium.ttf") },
		{ TEXT("SemiBold"),  TEXT("Montserrat-SemiBold.ttf") },
		{ TEXT("ExtraBold"), TEXT("Montserrat-ExtraBold.ttf") },
	};

	/** Glyphs checked to exist in all three faces. */
	const FString PlayGlyph = TEXT("▶");

	// -- assets ----------------------------------------------------------------------------------

	FString ObjectPathFor(const FString& Dir, const FString& Name)
	{
		return Dir / Name + TEXT(".") + Name;
	}

	/** Save without a dialog, so the tool can run unattended. */
	void SaveAsset(UObject* Asset)
	{
		if (!Asset)
		{
			return;
		}

		UPackage* Package = Asset->GetOutermost();
		Package->MarkPackageDirty();
		FEditorFileUtils::PromptForCheckoutAndSave({ Package }, /*bCheckDirty*/ false, /*bPromptToSave*/ false);
	}

	// -- widget helpers --------------------------------------------------------------------------

	/** Create a widget. Giving it a name makes it a Blueprint variable the runtime panel can bind to. */
	template <typename T>
	T* New(UWidgetTree& Tree, const TCHAR* Name = nullptr)
	{
		T* Widget = Tree.ConstructWidget<T>(T::StaticClass(), Name ? FName(Name) : NAME_None);
		Widget->bIsVariable = Name != nullptr;
		return Widget;
	}

	UTextBlock* Label(UWidgetTree& Tree, const TCHAR* Name, const FSlateFontInfo& Font,
		const FLinearColor& Colour, const FText& Text = FText::GetEmpty(),
		float LineHeight = 1.f, bool bWrap = false,
		ETextJustify::Type Justify = ETextJustify::Left)
	{
		UTextBlock* Block = New<UTextBlock>(Tree, Name);
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Colour));
		Block->SetText(Text);
		Block->SetAutoWrapText(bWrap);
		Block->SetJustification(Justify);

		if (!FMath::IsNearlyEqual(LineHeight, 1.f))
		{
			Block->SetLineHeightPercentage(LineHeight);
		}

		return Block;
	}

	UBorder* Card(UWidgetTree& Tree, const TCHAR* Name, const FSlateBrush& Brush, const FMargin& Padding)
	{
		UBorder* Border = New<UBorder>(Tree, Name);
		Border->SetBrush(Brush);
		Border->SetPadding(Padding);
		Border->SetHorizontalAlignment(HAlign_Fill);
		Border->SetVerticalAlignment(VAlign_Fill);
		return Border;
	}

	/** A 3 px rule (see FXR_PanelStyle). */
	UImage* Rule(UWidgetTree& Tree, const FLinearColor& Colour, float Thickness = RuleW)
	{
		// Sized through the brush; SetDesiredSizeOverride does nothing before the widget is
		// constructed (see FXR_PanelStyle::Band).
		UImage* Image = New<UImage>(Tree);
		Image->SetBrush(Band(Colour, Thickness));
		return Image;
	}

	USizeBox* Fixed(UWidgetTree& Tree, const TCHAR* Name, float Width, float Height)
	{
		USizeBox* Size = New<USizeBox>(Tree, Name);

		if (Width > 0.f)
		{
			Size->SetWidthOverride(Width);
		}

		if (Height > 0.f)
		{
			Size->SetHeightOverride(Height);
		}

		return Size;
	}

	//~ Slot helpers. Fill on a vertical slot is the mockup's `flex: 1`.

	UVerticalBoxSlot* Stack(UVerticalBox* Box, UWidget* Widget, const FMargin& Padding = FMargin(0.f),
		bool bFill = false, EHorizontalAlignment HAlign = HAlign_Fill)
	{
		UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget);
		Slot->SetPadding(Padding);
		Slot->SetHorizontalAlignment(HAlign);

		if (bFill)
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		return Slot;
	}

	UHorizontalBoxSlot* Line(UHorizontalBox* Box, UWidget* Widget, const FMargin& Padding = FMargin(0.f),
		bool bFill = false, EVerticalAlignment VAlign = VAlign_Center)
	{
		UHorizontalBoxSlot* Slot = Box->AddChildToHorizontalBox(Widget);
		Slot->SetPadding(Padding);
		Slot->SetVerticalAlignment(VAlign);

		if (bFill)
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		return Slot;
	}

	/** Pushes everything above it up (the mockup's `.spacer`). */
	void PushUp(UWidgetTree& Tree, UVerticalBox* Box)
	{
		Stack(Box, New<USpacer>(Tree), FMargin(0.f), true);
	}

	/**
	 * Pill button with a label. 122 px tall, well above the ~80 px minimum for something you aim at
	 * with a hand ray (there's no snapping or dwell here).
	 */
	UButton* PillButton(UWidgetTree& Tree, const TCHAR* Name, const TCHAR* LabelName,
		const FLinearColor& Face, const FLinearColor& Ink, const FText& Text,
		bool bRinged, UTextBlock** OutLabel = nullptr)
	{
		UButton* Button = New<UButton>(Tree, Name);
		Button->SetStyle(ButtonFace(Face, Divider(), bRinged ? RuleW : 0.f));

		UHorizontalBox* Content = New<UHorizontalBox>(Tree);
		UTextBlock* LabelBlock = Label(Tree, LabelName, TypeButton(), Ink, Text);
		Line(Content, LabelBlock);
		Button->AddChild(Content);

		if (OutLabel)
		{
			*OutLabel = LabelBlock;
		}

		return Button;
	}

	/** One "Steps / 5" row of a definition list. */
	UTextBlock* DefRow(UWidgetTree& Tree, UVerticalBox* Into, const FText& Term, const TCHAR* ValueName)
	{
		UHorizontalBox* Row = New<UHorizontalBox>(Tree);

		Line(Row, Label(Tree, nullptr, TypeDefTerm(), Text2(), Term));
		Line(Row, New<USpacer>(Tree), FMargin(0.f), true);

		UTextBlock* Value = Label(Tree, ValueName, TypeDefValue(), Text());
		Line(Row, Value);

		Stack(Into, Row, FMargin(0.f, 17.f));
		return Value;
	}

	/** Separator, only added between rows. */
	void DefRule(UWidgetTree& Tree, UVerticalBox* Into)
	{
		Stack(Into, Rule(Tree, Divider()));
	}

	/**
	 * Check every BindWidgetOptional on UFXR_TrainingPanel against the tree that was just built, by
	 * name and class, and log the result.
	 *
	 * UMG fails these silently: a wrong name or type just leaves the pointer null and the panel never
	 * updates. Since the builder picks the names, it checks them.
	 *
	 * Unused names are only counted, not treated as errors (InstructionLabel, ProgressFill etc. are
	 * for other layouts).
	 */
	void ReportBindings(UWidgetTree& Tree)
	{
		TArray<UWidget*> All;
		Tree.GetAllWidgets(All);

		TMap<FName, UClass*> Named;
		for (const UWidget* Widget : All)
		{
			if (Widget && Widget->bIsVariable)
			{
				Named.Add(Widget->GetFName(), Widget->GetClass());
			}
		}

		int32 Resolved = 0;
		int32 Unused = 0;
		TStringBuilder<1024> Wrong;

		for (TFieldIterator<FObjectPropertyBase> It(UFXR_TrainingPanel::StaticClass()); It; ++It)
		{
			const FObjectPropertyBase* Property = *It;
			if (!Property->HasMetaData(TEXT("BindWidgetOptional")))
			{
				continue;
			}

			UClass* const* Built = Named.Find(Property->GetFName());
			if (!Built)
			{
				++Unused;
				continue;
			}

			if ((*Built)->IsChildOf(Property->PropertyClass))
			{
				++Resolved;
			}
			else
			{
				Wrong.Appendf(TEXT("\n    %s was built as %s but the panel binds a %s"),
					*Property->GetName(), *(*Built)->GetName(), *Property->PropertyClass->GetName());
			}
		}

		if (Wrong.Len() > 0)
		{
			UE_LOG(LogFXR, Error, TEXT("The generated panel has bindings that cannot resolve:%s"),
				Wrong.ToString());
			return;
		}

		UE_LOG(LogFXR, Log,
			TEXT("Bindings check out: %d of the panel's widgets resolve by name and class, %d "
				 "declared names are unused by this layout, %d widgets in the tree."),
			Resolved, Unused, All.Num());
	}
}

// -----------------------------------------------------------------------------------------------
// Font
// -----------------------------------------------------------------------------------------------

bool UFXR_PanelBuilder::ImportPanelFonts(bool bOverwrite)
{
	const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("FlexXR"));
	if (!Plugin.IsValid())
	{
		UE_LOG(LogFXR, Error, TEXT("Could not find the FlexXR plugin, so the font sources are unreachable."));
		return false;
	}

	const FString SourceDir = Plugin->GetBaseDir() / TEXT("Resources/Fonts");
	const FString FontObjectPath = ObjectPathFor(FontDir, FontAssetName);

	if (!bOverwrite && FPackageName::DoesPackageExist(FontDir / FontAssetName))
	{
		UE_LOG(LogFXR, Log, TEXT("%s already exists; left alone."), *FontObjectPath);
		return true;
	}

	// -- the three faces -------------------------------------------------------------------------
	TArray<UFontFace*> Imported;

	for (const FFaceSpec& Spec : Faces)
	{
		const FString SourceFile = SourceDir / Spec.File;

		TArray<uint8> Bytes;
		if (!FFileHelper::LoadFileToArray(Bytes, *SourceFile))
		{
			UE_LOG(LogFXR, Error, TEXT("Could not read %s. The font sources ship in the plugin's "
				"Resources/Fonts; if they are missing, the panel cannot be set in Montserrat."), *SourceFile);
			return false;
		}

		const FString FaceName = FString::Printf(TEXT("F_Montserrat_%s"), Spec.Typeface);
		const FString PackageName = FontDir / FaceName;

		UPackage* Package = CreatePackage(*PackageName);
		if (!Package)
		{
			UE_LOG(LogFXR, Error, TEXT("Could not create package %s."), *PackageName);
			return false;
		}

		Package->FullyLoad();

		UFontFace* Face = NewObject<UFontFace>(Package, *FaceName, RF_Public | RF_Standalone);
		Face->SourceFilename = SourceFile;

		// Embedded rather than streamed, so there's no file IO hitch when a glyph is first drawn.
		Face->LoadingPolicy = EFontLoadingPolicy::Inline;
		Face->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
		Face->CacheSubFaces();

		// Without this the content browser doesn't see the new asset until restart.
		FAssetRegistryModule::AssetCreated(Face);
		Face->MarkPackageDirty();
		SaveAsset(Face);

		Imported.Add(Face);
	}

	// -- the composite ---------------------------------------------------------------------------
	const FString PackageName = FontDir / FontAssetName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	UFont* Font = NewObject<UFont>(Package, *FontAssetName, RF_Public | RF_Standalone);

	// Runtime, not Offline (Offline bakes glyphs at fixed sizes).
	Font->FontCacheType = EFontCacheType::Runtime;

	// CompositeFont direct access is deprecated in 5.7 (and warnings are errors here).
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	Composite = FCompositeFont();
	Composite.DefaultTypeface.Fonts.Reset();

	for (int32 Index = 0; Index < Imported.Num(); ++Index)
	{
		FTypefaceEntry& Entry = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
		Entry.Name = FName(Faces[Index].Typeface);
		Entry.Font = FFontData(Imported[Index]);
	}

	FAssetRegistryModule::AssetCreated(Font);
	Font->MarkPackageDirty();
	SaveAsset(Font);

	UE_LOG(LogFXR, Log, TEXT("Imported %d Montserrat faces and composed %s."),
		Imported.Num(), *FontObjectPath);

	return true;
}

// -----------------------------------------------------------------------------------------------
// Panel
// -----------------------------------------------------------------------------------------------

bool UFXR_PanelBuilder::BuildDefaultTrainingPanel(bool bOverwrite)
{
	// Font first, otherwise every glyph renders as a missing-glyph box.
	if (!ImportPanelFonts(/*bOverwrite*/ false))
	{
		UE_LOG(LogFXR, Error, TEXT("The font could not be prepared, so the panel was not built."));
		return false;
	}

	const FString Full = UIPath / PanelAssetName;
	UWidgetBlueprint* Blueprint = nullptr;

	if (FPackageName::DoesPackageExist(Full))
	{
		if (!bOverwrite)
		{
			UE_LOG(LogFXR, Warning,
				TEXT("%s already exists and was left alone. Pass bOverwrite to replace it, which "
					 "discards whatever was authored on top of the generated layout."), *Full);
			return false;
		}

		// Rebuild in place instead of delete + recreate: deleting needs a confirmation dialog, and
		// rebuilding keeps existing references to the panel valid.
		Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *ObjectPathFor(UIPath, PanelAssetName));
		if (!Blueprint)
		{
			UE_LOG(LogFXR, Error, TEXT("%s exists but would not load, so it was left alone."), *Full);
			return false;
		}
	}
	else
	{
		UWidgetBlueprintFactory* Factory = NewObject<UWidgetBlueprintFactory>();
		Factory->ParentClass = UFXR_TrainingPanel::StaticClass();

		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
		Blueprint = Cast<UWidgetBlueprint>(
			AssetTools.CreateAsset(PanelAssetName, UIPath, UWidgetBlueprint::StaticClass(), Factory));
	}

	if (!Blueprint || !Blueprint->WidgetTree)
	{
		UE_LOG(LogFXR, Error, TEXT("Could not create %s."), *Full);
		return false;
	}

	UWidgetTree& Tree = *Blueprint->WidgetTree;

	/**
	 * Empty the existing tree.
	 *
	 * Detaching the root isn't enough: the old widgets keep their names inside the WidgetTree, and
	 * creating a new widget with the same name but a different class crashes ("Cannot replace
	 * existing object of a different class"). Renaming them into the transient package frees the
	 * names and lets GC collect them. The factory's default root goes too, since its type comes from
	 * project settings.
	 */
	{
		TArray<UWidget*> Stale;
		Tree.GetAllWidgets(Stale);

		for (UWidget* Widget : Stale)
		{
			Tree.RemoveWidget(Widget);
			Widget->Rename(nullptr, GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional | REN_DoNotDirty);
		}

		Tree.RootWidget = nullptr;

		// Clear the GUID map too, so the widget compiler rebuilds it deterministically from the new
		// tree (WidgetBlueprintCompiler.cpp, ValidateAndFixUpVariableGuids) instead of hitting an
		// ensure for every widget.
		Blueprint->WidgetVariableNameToGuidMap.Reset();
	}

	// -- the surface -----------------------------------------------------------------------------
	UCanvasPanel* Canvas = New<UCanvasPanel>(Tree);
	Tree.RootWidget = Canvas;

	// Inner ring, no drop shadow. A floating panel has nothing behind it to catch a shadow.
	UBorder* Backplate = Card(Tree, nullptr,
		RingedBox(Surface(), PanelRadius, PanelRing(), PanelRingW), FMargin(0.f));

	if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Backplate))
	{
		Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		Slot->SetOffsets(FMargin(0.f));
		Slot->SetAlignment(FVector2D::ZeroVector);
	}

	UVerticalBox* Root = New<UVerticalBox>(Tree);
	// Clip everything to the panel. Slate clips to the rectangle, not the rounded corners, but the
	// 52 px gutter keeps content clear of the 42 px corners anyway.
	Backplate->SetClipping(EWidgetClipping::ClipToBounds);
	Backplate->AddChild(Root);

	// -- top bar, shared by Start and Procedure ---------------------------------------------------
	//
	// Sits above the switcher so both screens use the same one. The runtime panel collapses it on
	// the other screens; the rule is inside it so it goes too.
	UVerticalBox* TopBar = New<UVerticalBox>(Tree, TEXT("TopBar"));
	Stack(Root, TopBar);
	{
		USizeBox* BarBox = Fixed(Tree, nullptr, 0.f, BarH);
		UHorizontalBox* Bar = New<UHorizontalBox>(Tree);
		BarBox->AddChild(Bar);

		Line(Bar, Label(Tree, TEXT("CrumbRootLabel"), TypeCrumb(), Text2()), FMargin(Gutter, 0.f, 0.f, 0.f));

		UTextBlock* Separator = Label(Tree, TEXT("CrumbSeparator"), TypeCrumb(), Text4(),
			NSLOCTEXT("FlexXR", "PanelCrumbSep", "/"));
		Line(Bar, Separator, FMargin(11.f, 0.f, 11.f, 0.f));

		// The procedure name takes the spare width and ellipses, so the status never gets pushed off.
		UTextBlock* Leaf = Label(Tree, TEXT("CrumbLeafLabel"), TypeCrumb(), Text());
		Leaf->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
		Line(Bar, Leaf, FMargin(0.f), true);

		UImage* Pip = New<UImage>(Tree, TEXT("StatusPip"));
		Pip->SetBrush(Dot(Text4(), 14.f));
		Line(Bar, Pip, FMargin(12.f, 0.f, 11.f, 0.f));

		Line(Bar, Label(Tree, TEXT("StatusLabel"), TypeStatus(), Text2(),
			NSLOCTEXT("FlexXR", "PanelNotStarted", "Not started")), FMargin(0.f, 0.f, Gutter, 0.f));

		Stack(TopBar, BarBox);
		Stack(TopBar, Rule(Tree, Divider()));
	}

	// One segment per step, filled at runtime, right under the bar. No SizeBox around it: the
	// runtime adds children to this box directly, and a wrapper would keep its height when the
	// meter is collapsed. Height comes from the segments.
	UHorizontalBox* Meter = New<UHorizontalBox>(Tree, TEXT("StepMeter"));
	Stack(Root, Meter);

	UWidgetSwitcher* Switcher = New<UWidgetSwitcher>(Tree, TEXT("ScreenSwitcher"));

	// -- Start -----------------------------------------------------------------------------------
	//
	// Shows the mode but doesn't let the trainee change it.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		// Every body clips. Long titles or briefings would otherwise draw over the foot and
		// past the edge of the panel.
		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);
		Stack(Screen, Body, FMargin(Gutter, 36.f, Gutter, 0.f), true);

		Stack(Body, Label(Tree, nullptr, TypeEyebrow(), Text3(),
			NSLOCTEXT("FlexXR", "PanelStartEyebrow", "TRAINING PROCEDURE")));

		Stack(Body, Label(Tree, TEXT("StartTitleLabel"), TypeTitle(), Text(),
			FText::GetEmpty(), LineTitle, true), FMargin(0.f, 14.f, 0.f, 0.f));

		UVerticalBox* Defs = New<UVerticalBox>(Tree);
		Stack(Body, Defs, FMargin(0.f, 34.f, 0.f, 0.f));
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelSteps", "Steps"), TEXT("StartStepsValue"));
		DefRule(Tree, Defs);
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelMode", "Mode"), TEXT("StartModeValue"));

		PushUp(Tree, Body);

		UVerticalBox* Foot = New<UVerticalBox>(Tree);
		Stack(Screen, Foot, FMargin(Gutter, 0.f, Gutter, Gutter));

		USizeBox* ButtonBox = Fixed(Tree, nullptr, 0.f, ButtonH);
		ButtonBox->AddChild(PillButton(Tree, TEXT("BeginButton"), nullptr, Blue(), OnBlue(),
			NSLOCTEXT("FlexXR", "PanelBegin", "Begin"), false));
		Stack(Foot, ButtonBox);
	}

	// -- Explanation -----------------------------------------------------------------------------
	//
	// Eyebrow is a dot + label; the dot pulses with the highlight. Audio gets a small transport
	// rather than a fill behind the text.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);
		Stack(Screen, Body, FMargin(Gutter, 56.f, Gutter, 0.f), true);

		UHorizontalBox* Kind = New<UHorizontalBox>(Tree);
		Stack(Body, Kind, FMargin(0.f), false, HAlign_Left);

		UImage* Pip = New<UImage>(Tree, TEXT("ExplainPip"));
		Pip->SetBrush(Dot(Amber(), 16.f));
		Line(Kind, Pip, FMargin(0.f, 0.f, 14.f, 0.f));

		Line(Kind, Label(Tree, nullptr, TypeKind(), Explain(),
			NSLOCTEXT("FlexXR", "PanelExplanation", "EXPLANATION")));

		Stack(Body, Label(Tree, TEXT("ExplanationLabel"), TypeBody(), Text(),
			FText::GetEmpty(), LineBody, true), FMargin(0.f, 34.f, 0.f, 0.f));

		PushUp(Tree, Body);

		UVerticalBox* Foot = New<UVerticalBox>(Tree);
		Stack(Screen, Foot, FMargin(Gutter, 0.f, Gutter, Gutter));

		UWidgetSwitcher* FootSwitch = New<UWidgetSwitcher>(Tree, TEXT("ExplainFootSwitcher"));
		Stack(Foot, FootSwitch);

		// [0] Idle: full-width play button.
		{
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, ButtonH);
			UButton* Play = New<UButton>(Tree, TEXT("ExplainPlayButton"));
			Play->SetStyle(ButtonFace(Fill(), Divider(), RuleW));

			UHorizontalBox* Content = New<UHorizontalBox>(Tree);
			Line(Content, Label(Tree, nullptr, Type(30, FontSemiBold()), Text(),
				FText::FromString(PlayGlyph)), FMargin(0.f, 0.f, 18.f, 0.f));
			Line(Content, Label(Tree, nullptr, TypeButton(), Text(),
				NSLOCTEXT("FlexXR", "PanelPlayAudio", "Play audio")));

			Play->AddChild(Content);
			Box->AddChild(Play);
			FootSwitch->AddChild(Box);
		}

		// [1] Playing: pause, a thin progress bar, elapsed time.
		{
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, ButtonH);
			UHorizontalBox* Transport = New<UHorizontalBox>(Tree);
			Box->AddChild(Transport);

			USizeBox* PauseBox = Fixed(Tree, nullptr, ButtonH, ButtonH);
			UButton* Pause = New<UButton>(Tree, TEXT("ExplainPauseButton"));
			Pause->SetStyle(ButtonFace(Fill(), Divider(), RuleW));

			// Two bars instead of a glyph, since Montserrat has no pause character.
			UHorizontalBox* Bars = New<UHorizontalBox>(Tree);
			for (int32 Index = 0; Index < 2; ++Index)
			{
				UImage* Bar = New<UImage>(Tree);
				FSlateBrush BarBrush = FXR_PanelStyle::Box(Text(), 2.f);
				BarBrush.ImageSize = FVector2D(6.f, 32.f);
				Bar->SetBrush(BarBrush);
				Line(Bars, Bar, Index == 0 ? FMargin(0.f, 0.f, 6.f, 0.f) : FMargin(0.f));
			}

			Pause->AddChild(Bars);
			PauseBox->AddChild(Pause);
			Line(Transport, PauseBox, FMargin(0.f, 0.f, 22.f, 0.f));

			USizeBox* TrackBox = Fixed(Tree, nullptr, 0.f, 10.f);
			UProgressBar* Audio = New<UProgressBar>(Tree, TEXT("ExplainAudioBar"));
			Audio->SetWidgetStyle(BarStyle(Track(), Text2()));
			Audio->SetPercent(0.f);
			TrackBox->AddChild(Audio);
			Line(Transport, TrackBox, FMargin(0.f), true);

			Line(Transport, Label(Tree, TEXT("ExplainTimeLabel"), TypeTime(), Text3(),
				NSLOCTEXT("FlexXR", "PanelZeroTime", "0:00")), FMargin(22.f, 0.f, 0.f, 0.f));

			FootSwitch->AddChild(Box);
		}

		// [2] Read: continue button.
		{
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, ButtonH);
			Box->AddChild(PillButton(Tree, TEXT("ExplainConfirmButton"), nullptr, Blue(), OnBlue(),
				NSLOCTEXT("FlexXR", "PanelGotIt", "Got it"), false));
			FootSwitch->AddChild(Box);
		}
	}

	// -- Procedure -------------------------------------------------------------------------------
	//
	// Meter = one bar per step, list = the open step's sub-steps. The foot shows mode and time, and
	// is replaced by the mistake message when there is one.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		// Two bodies over one foot. [0] Training: heading and rows. [1] Exam: progress and mistake
		// count only, since the steps would give the answers away.
		UWidgetSwitcher* BodySwitch = New<UWidgetSwitcher>(Tree, TEXT("ProcedureBodySwitcher"));
		Stack(Screen, BodySwitch, FMargin(0.f), true);

		const auto AddBody = [BodySwitch](UWidget* Child)
		{
			if (UWidgetSwitcherSlot* BodySlot = Cast<UWidgetSwitcherSlot>(BodySwitch->AddChild(Child)))
			{
				BodySlot->SetPadding(FMargin(Gutter, 36.f, Gutter, 0.f));
				BodySlot->SetHorizontalAlignment(HAlign_Fill);
				BodySlot->SetVerticalAlignment(VAlign_Fill);
			}
		};

		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);
		AddBody(Body);

		Stack(Body, Label(Tree, nullptr, TypeEyebrow(), Text3(),
			NSLOCTEXT("FlexXR", "PanelCurrentStep", "CURRENT STEP")));

		Stack(Body, Label(Tree, TEXT("StepTitleLabel"), TypeHeading(), Text(),
			FText::GetEmpty(), LineHeading, true), FMargin(0.f, 10.f, 0.f, 0.f));

		UScrollBox* Scroll = New<UScrollBox>(Tree, TEXT("StepScroll"));
		Stack(Body, Scroll, FMargin(0.f, 26.f, 0.f, 0.f), true);

		UBorder* ListCard = Card(Tree, nullptr,
			RingedBox(Fill2(), ListRadius, Divider(), PanelRingW), FMargin(0.f));
		ListCard->SetClipping(EWidgetClipping::ClipToBounds);

		// Keep clear of the scroll bar.
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(ListCard)))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}

		// These rows are read, not clicked, so they can be smaller than the 80 px target size.
		UVerticalBox* List = New<UVerticalBox>(Tree, TEXT("StepList"));
		ListCard->AddChild(List);

		// [1] Exam body, same layout as Start (eyebrow, big number, definition list).
		{
			UVerticalBox* ExamBody = New<UVerticalBox>(Tree);
			ExamBody->SetClipping(EWidgetClipping::ClipToBounds);
			AddBody(ExamBody);

			Stack(ExamBody, Label(Tree, nullptr, TypeEyebrow(), Text3(),
				NSLOCTEXT("FlexXR", "PanelExamInProgress", "EXAM IN PROGRESS")));

			Stack(ExamBody, Label(Tree, TEXT("ExamProgressLabel"), TypeTitle(), Text(),
				FText::GetEmpty(), LineTitle), FMargin(0.f, 14.f, 0.f, 0.f));

			Stack(ExamBody, Label(Tree, nullptr, TypeReason(), Text2(),
				NSLOCTEXT("FlexXR", "PanelExamStepsComplete", "steps complete")), FMargin(0.f, 6.f, 0.f, 0.f));

			UVerticalBox* Defs = New<UVerticalBox>(Tree);
			Stack(ExamBody, Defs, FMargin(0.f, 34.f, 0.f, 0.f));
			DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelMistakes", "Mistakes"), TEXT("ExamMistakesValue"));

			PushUp(Tree, ExamBody);
		}

		UWidgetSwitcher* FootSwitch = New<UWidgetSwitcher>(Tree, TEXT("FootSwitcher"));
		Stack(Screen, FootSwitch);

		// [0] Mode and time.
		{
			UVerticalBox* Foot = New<UVerticalBox>(Tree);
			Stack(Foot, Rule(Tree, Divider()));

			USizeBox* Box = Fixed(Tree, nullptr, 0.f, FootH);
			UHorizontalBox* Row = New<UHorizontalBox>(Tree);
			Box->AddChild(Row);

			Line(Row, Label(Tree, TEXT("ModeLabel"), TypeStatus(), Text3()),
				FMargin(Gutter, 0.f, 0.f, 0.f));
			Line(Row, New<USpacer>(Tree), FMargin(0.f), true);
			Line(Row, Label(Tree, TEXT("ElapsedLabel"), TypeStatus(), Text3(),
				NSLOCTEXT("FlexXR", "PanelZeroTimeFoot", "0:00")), FMargin(0.f, 0.f, Gutter, 0.f));

			Stack(Foot, Box);
			FootSwitch->AddChild(Foot);
		}

		// [1] Mistake message, in the same space.
		{
			UVerticalBox* Foot = New<UVerticalBox>(Tree, TEXT("MistakeCard"));
			Stack(Foot, Rule(Tree, AlarmRule()));

			UBorder* Bed = Card(Tree, nullptr, Band(AlarmBed()), FMargin(0.f));
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, FootH);
			UHorizontalBox* Row = New<UHorizontalBox>(Tree);
			Box->AddChild(Row);

			UImage* Pip = New<UImage>(Tree);
			Pip->SetBrush(Dot(Red(), 18.f));
			Line(Row, Pip, FMargin(Gutter, 0.f, 16.f, 0.f));

			Line(Row, Label(Tree, TEXT("MistakeLabel"), TypeMistake(), Danger(),
				FText::GetEmpty(), LineMistake, true), FMargin(0.f, 0.f, Gutter, 0.f), true);

			Bed->AddChild(Box);
			Stack(Foot, Bed);
			FootSwitch->AddChild(Foot);
		}
	}

	// -- Complete --------------------------------------------------------------------------------
	//
	// Same screen for pass and fail. The ring is always the score; colour and the eyebrow show the
	// outcome, and a failed run gets an extra line with the reason.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);

		// No bottom padding (same as the other screens). The mockup pads all four sides and relies
		// on min-height: 0 to let the body shrink; a vertical box can't shrink below its content, so
		// the padding pushed a failed report to 918 px on an 870 px panel. The spacer and foot padding
		// already provide the gap.
		Stack(Screen, Body, FMargin(Gutter, Gutter, Gutter, 0.f), true);

		USizeBox* RingBox = Fixed(Tree, nullptr, RingSize, RingSize);
		UOverlay* RingStack = New<UOverlay>(Tree);
		RingBox->AddChild(RingStack);

		URadialSlider* Ring = New<URadialSlider>(Tree, TEXT("ScoreRing"));
		Ring->SliderHandleStartAngle = 0.f;
		Ring->SliderHandleEndAngle = 360.f;
		Ring->ShowSliderHandle = false;
		Ring->ShowSliderHand = false;
		Ring->Locked = true;
		Ring->SliderBarColor = Track();
		Ring->SliderProgressColor = Green();
		Ring->CenterBackgroundColor = FLinearColor::Transparent;
		Ring->SetValue(0.f);
		{
			FSliderStyle Style = Ring->WidgetStyle;
			Style.SetBarThickness(RingThickness);
			Ring->WidgetStyle = Style;
		}

		// Not interactive, otherwise the laser can drag the slider.
		Ring->SetVisibility(ESlateVisibility::HitTestInvisible);

		if (UOverlaySlot* Slot = RingStack->AddChildToOverlay(Ring))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		UTextBlock* Score = Label(Tree, TEXT("ScoreLabel"), TypeScore(), Green(),
			FText::GetEmpty(), 1.f, false, ETextJustify::Center);

		if (UOverlaySlot* Slot = RingStack->AddChildToOverlay(Score))
		{
			Slot->SetHorizontalAlignment(HAlign_Center);
			Slot->SetVerticalAlignment(VAlign_Center);
		}

		// Tick shown instead of the score after a Training run. Drawn as two rounded bars because
		// Montserrat has no check mark. Points relative to the ring centre: (-30,0) -> (-8,22) ->
		// (34,-22); each bar is its segment plus its thickness long so the round caps overlap at the
		// joint.
		UOverlay* Tick = New<UOverlay>(Tree, TEXT("ScoreTick"));
		{
			struct FStroke
			{
				float Length;
				FVector2D Centre;
				float Angle;
			};

			const FStroke Strokes[] = {
				{ 45.f, FVector2D(-19.f, 11.f), 45.f },
				{ 75.f, FVector2D(13.f, 0.f), -46.3f },
			};

			for (const FStroke& Stroke : Strokes)
			{
				FSlateBrush Brush = Pill(Green());
				Brush.ImageSize = FVector2D(Stroke.Length, 14.f);

				UImage* Bar = New<UImage>(Tree);
				Bar->SetBrush(Brush);
				Bar->SetRenderTransform(FWidgetTransform(Stroke.Centre, FVector2D(1.f, 1.f), FVector2D::ZeroVector, Stroke.Angle));

				if (UOverlaySlot* BarSlot = Tick->AddChildToOverlay(Bar))
				{
					BarSlot->SetHorizontalAlignment(HAlign_Center);
					BarSlot->SetVerticalAlignment(VAlign_Center);
				}
			}
		}

		// Hidden until the runtime panel shows it.
		Tick->SetVisibility(ESlateVisibility::Collapsed);

		if (UOverlaySlot* Slot = RingStack->AddChildToOverlay(Tick))
		{
			Slot->SetHorizontalAlignment(HAlign_Fill);
			Slot->SetVerticalAlignment(VAlign_Fill);
		}

		Stack(Body, RingBox, FMargin(0.f), false, HAlign_Center);

		Stack(Body, Label(Tree, TEXT("CompleteEyebrowLabel"), TypeEyebrow(), Green(),
			FText::GetEmpty(), 1.f, false, ETextJustify::Center),
			FMargin(0.f, 24.f, 0.f, 0.f), false, HAlign_Center);

		Stack(Body, Label(Tree, TEXT("CompleteTitleLabel"), TypeHeading(), Text(),
			FText::GetEmpty(), LineHeading, true, ETextJustify::Center),
			FMargin(0.f, 12.f, 0.f, 0.f), false, HAlign_Center);

		Stack(Body, Label(Tree, TEXT("ReasonLabel"), TypeReason(), Text2(),
			FText::GetEmpty(), LineReason, true, ETextJustify::Center),
			FMargin(0.f, 14.f, 0.f, 0.f), false, HAlign_Center);

		UVerticalBox* Defs = New<UVerticalBox>(Tree);
		Stack(Body, Defs, FMargin(0.f, 22.f, 0.f, 0.f));
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelSteps", "Steps"), TEXT("ResultStepsValue"));
		DefRule(Tree, Defs);
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelMistakes", "Mistakes"), TEXT("ResultMistakesValue"));
		DefRule(Tree, Defs);
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelTime", "Time"), TEXT("ResultTimeValue"));

		PushUp(Tree, Body);

		UVerticalBox* Foot = New<UVerticalBox>(Tree);
		Stack(Screen, Foot, FMargin(Gutter, 0.f, Gutter, Gutter));

		USizeBox* ButtonBox = Fixed(Tree, nullptr, 0.f, ButtonH);
		ButtonBox->AddChild(PillButton(Tree, TEXT("DoneButton"), TEXT("DoneLabel"), Blue(), OnBlue(),
			NSLOCTEXT("FlexXR", "PanelDoneButton", "Done"), false));
		Stack(Foot, ButtonBox);
	}

	Stack(Root, Switcher, FMargin(0.f), true);

	// Compile before saving, otherwise the named widgets aren't Blueprint variables yet and the
	// C++ bindings find nothing.
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

#if WITH_EDITORONLY_DATA
	// Make the designer preview the panel's real size instead of a default square.
	if (UUserWidget* Defaults = Blueprint->GeneratedClass
		? Cast<UUserWidget>(Blueprint->GeneratedClass->GetDefaultObject())
		: nullptr)
	{
		Defaults->DesignTimeSize = FVector2D(PanelW, PanelH);
		Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
	}
#endif

	ReportBindings(Tree);
	SaveAsset(Blueprint);

	UE_LOG(LogFXR, Log,
		TEXT("Built %s at %.0fx%.0f. Put it in an FXR_Panel with Draw Size %.0fx%.0f and Panel "
			 "Width 90, which is the pairing that makes one pixel one millimetre."),
		*Full, PanelW, PanelH, PanelW, PanelH);

	return true;
}
