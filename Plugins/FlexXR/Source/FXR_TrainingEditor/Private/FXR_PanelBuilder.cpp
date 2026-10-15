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
 * THE PANEL, AND WHY IT IS SHAPED LIKE THIS
 *
 * Four screens — Start, Explanation, Procedure, Complete — as the four children of one Widget
 * Switcher in one asset. They share a surface, a bar, a type scale and a foot, and four assets
 * would be four copies of all of it that drift apart the first time one is edited.
 *
 * Every size and colour comes from `FXR_PanelStyle.h`, which explains the one identity the whole
 * design rests on: 900 x 870 px at a 90 cm panel width makes one pixel one millimetre.
 *
 * Naming a widget here is what binds it. `New<T>(Tree, TEXT("StatusLabel"))` marks it as a
 * Blueprint variable, which is what `meta = (BindWidgetOptional)` on UFXR_TrainingPanel resolves
 * against; everything built without a name stays out of the Blueprint's variable list, so what a
 * designer sees is the dozen things that are actually driven rather than sixty boxes.
 *
 *
 * FOUR PLACES THE DESIGN COULD NOT BE TRANSLATED, AND WHAT HAPPENED INSTEAD
 *
 * - **Frosted glass.** `backdrop-filter: blur(30px)` has no equivalent for a world-space widget:
 *   there is no scene colour behind the quad to blur. The translucent fill and the bright inner
 *   ring carry the look without it.
 * - **The check mark.** Montserrat contains no U+2713 — verified against the font's own cmap, not
 *   assumed — so a completed step is its number on a green disc rather than a tick. A
 *   missing-glyph box would be worse than no tick.
 * - **The list's bottom fade.** A gradient needs a texture or a material. The list card's ring
 *   already gives the list an edge, so the fade is simply absent.
 * - **Line clamping.** UMG has no `line-clamp`, so a closed row is one line that ellipses and the
 *   open row wraps freely. A very long instruction grows its row rather than being cut at four
 *   lines.
 */

namespace
{
	using namespace FXR_PanelStyle;

	const FString UIPath = TEXT("/FlexXR/UI");
	const FString FontDir = TEXT("/FlexXR/UI/Fonts");
	const FString PanelAssetName = TEXT("WBP_FXR_TrainingPanel");
	const FString FontAssetName = TEXT("F_Montserrat");

	/** Montserrat's three weights, as the typeface names FXR_PanelStyle asks for. */
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

	/** Glyphs confirmed present in all three faces. Anything not on this list is not used. */
	const FString PlayGlyph = TEXT("▶");

	// ── assets ──────────────────────────────────────────────────────────────────────────────────

	FString ObjectPathFor(const FString& Dir, const FString& Name)
	{
		return Dir / Name + TEXT(".") + Name;
	}

	/**
	 * Saved without a prompt: this is a tool, and a dialog here is the difference between a script
	 * that runs unattended and one that quietly waits forever.
	 */
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

	// ── widget helpers ──────────────────────────────────────────────────────────────────────────

	/**
	 * Make a widget. Giving it a name is what makes it a Blueprint variable, and therefore what
	 * makes the runtime panel able to find and drive it.
	 */
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

	/** A flat rule. Three pixels, for the reason given in FXR_PanelStyle. */
	UImage* Rule(UWidgetTree& Tree, const FLinearColor& Colour, float Thickness = RuleW)
	{
		// Sized by its brush. SetDesiredSizeOverride is a no-op on an image that has not been
		// constructed into Slate yet, which every image built here is - see FXR_PanelStyle::Band.
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

	//~ Slot helpers. A vertical slot's Fill is what the design writes as `flex: 1`.

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

	/** Push everything above it upwards, which is the design's `.spacer`. */
	void PushUp(UWidgetTree& Tree, UVerticalBox* Box)
	{
		Stack(Box, New<USpacer>(Tree), FMargin(0.f), true);
	}

	/**
	 * A pill button with a label.
	 *
	 * 122 px tall, which is well past the 80 px floor anything laser-targetable needs: there is no
	 * magnetism, snapping or dwell on this stack, so a target has to be large enough to hit with a
	 * ray held in a hand at arm's length.
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

	/** The separator between definition rows, added only between them. */
	void DefRule(UWidgetTree& Tree, UVerticalBox* Into)
	{
		Stack(Into, Rule(Tree, Divider()));
	}

	/**
	 * Check every BindWidgetOptional on UFXR_TrainingPanel against the tree that was just built,
	 * and say so out loud.
	 *
	 * UMG resolves an optional binding silently. A name that does not match, or one that matches a
	 * widget of the wrong type, leaves the pointer null and reports nothing at all - and the symptom
	 * is a panel that comes up looking right and then never updates. This builder is the thing that
	 * chooses those names, so it is the right place to prove they line up, by the same two tests
	 * UMG itself applies: the name, and then the class.
	 *
	 * Names the panel declares but this layout does not use are reported as a count rather than a
	 * problem. They are deliberate: `InstructionLabel`, `ProgressFill` and the rest exist for
	 * simpler layouts somebody else builds.
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

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The font
// ───────────────────────────────────────────────────────────────────────────────────────────────

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

	// ── the three faces ─────────────────────────────────────────────────────────────────────────
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

		// Embedded in the asset rather than streamed. Streaming trades memory for file IO at the
		// moment a glyph is first drawn, and a hitch while a panel's text appears is the one place
		// that trade is clearly wrong.
		Face->LoadingPolicy = EFontLoadingPolicy::Inline;
		Face->FontFaceData = FFontFaceData::MakeFontFaceData(MoveTemp(Bytes));
		Face->CacheSubFaces();

		// Without this the asset exists on disk and the content browser does not know, which looks
		// exactly like an import that failed.
		FAssetRegistryModule::AssetCreated(Face);
		Face->MarkPackageDirty();
		SaveAsset(Face);

		Imported.Add(Face);
	}

	// ── the composite ───────────────────────────────────────────────────────────────────────────
	const FString PackageName = FontDir / FontAssetName;
	UPackage* Package = CreatePackage(*PackageName);
	Package->FullyLoad();

	UFont* Font = NewObject<UFont>(Package, *FontAssetName, RF_Public | RF_Standalone);

	// Runtime, not Offline. Offline is the old UFont path and bakes glyphs into a texture at fixed
	// sizes; a panel read from a distance that varies wants the glyph cache instead.
	Font->FontCacheType = EFontCacheType::Runtime;

	// Through the accessor: direct access to CompositeFont was deprecated in 5.7 and the warning is
	// an error here.
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

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The panel
// ───────────────────────────────────────────────────────────────────────────────────────────────

bool UFXR_PanelBuilder::BuildDefaultTrainingPanel(bool bOverwrite)
{
	// The font first. A panel built before it exists is set in nothing, and every glyph comes out
	// as a box with a Unicode block name in it — which is exactly the bug this replaces.
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

		// Rebuilt in place rather than deleted and recreated. Deleting an asset wants a
		// confirmation that an unattended run cannot give, which is how the material tool learned
		// to half-succeed; and rebuilding keeps every reference to this panel pointing at it.
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
	 * Clear out whatever is already in the tree, names and all.
	 *
	 * Detaching the root is not enough. Every widget stays outered to the WidgetTree under its own
	 * name, so constructing a new `MistakeCard` when the old one is a different class is a hard
	 * crash in UObject creation - "Cannot replace existing object of a different class" - rather
	 * than a tidy failure. Renaming them into the transient package releases the names and lets GC
	 * take them.
	 *
	 * This is also why the factory's own seeded root has to go: it comes from project settings and
	 * may be any widget type at all.
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

		// The GUID map belongs to the widgets that just went away. Emptying it is what makes the
		// widget compiler populate it deterministically from the new tree
		// (WidgetBlueprintCompiler.cpp, ValidateAndFixUpVariableGuids) instead of taking its
		// self-healing path and ensuring once per widget that one "was added but did not get a
		// GUID". The determinism matters: those GUIDs are what an external reference to one of
		// these widgets survives a rename by.
		Blueprint->WidgetVariableNameToGuidMap.Reset();
	}

	// ── the surface ─────────────────────────────────────────────────────────────────────────────
	UCanvasPanel* Canvas = New<UCanvasPanel>(Tree);
	Tree.RootWidget = Canvas;

	// An inner ring and no drop shadow. Nothing behind a floating panel catches a shadow, and in
	// stereo one reads as a sticker laid on the world rather than a thing in it.
	UBorder* Backplate = Card(Tree, nullptr,
		RingedBox(Surface(), PanelRadius, PanelRing(), PanelRingW), FMargin(0.f));

	if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Backplate))
	{
		Slot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		Slot->SetOffsets(FMargin(0.f));
		Slot->SetAlignment(FVector2D::ZeroVector);
	}

	UVerticalBox* Root = New<UVerticalBox>(Tree);
	// The last line of defence: nothing draws outside the panel. Slate clips to rectangles, not to
	// the rounded corners, which is enough because the 52 px gutter keeps all content well clear of
	// a 42 px corner.
	Backplate->SetClipping(EWidgetClipping::ClipToBounds);
	Backplate->AddChild(Root);

	// ── the bar, shared by Start and Procedure ──────────────────────────────────────────────────
	//
	// Above the switcher so one copy serves both, and collapsed as a unit by the runtime panel on
	// the two screens that are about one thing at a time. The rule belongs inside it: a rule left
	// behind by a hidden bar is a line across the top of nothing.
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

		// The leaf takes the slack, so a long procedure name ellipses rather than shoving the
		// status off the end of the bar.
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

	// One segment per step, filled by the runtime panel once a graph is compiled. It rides directly
	// under the bar rather than costing a block of its own.
	//
	// No SizeBox around it: the bound widget has to be the one the runtime panel adds children to,
	// and a box wrapping it would keep its own 6 px after the box inside was collapsed. The height
	// comes from the segments, which the panel gives an explicit desired height.
	UHorizontalBox* Meter = New<UHorizontalBox>(Tree, TEXT("StepMeter"));
	Stack(Root, Meter);

	UWidgetSwitcher* Switcher = New<UWidgetSwitcher>(Tree, TEXT("ScreenSwitcher"));

	// ── Start ───────────────────────────────────────────────────────────────────────────────────
	//
	// The mode is stated, never offered. A trainee who can pick Guided on the way into an exam is
	// not sitting an exam, so this reads the session and reports it.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		// Every screen's body clips. Its content can grow - a title or a briefing wraps to as
		// many lines as the author wrote - and without a clip the overflow paints straight over
		// the foot below it, button and all. In a headset that is text hanging in the air past
		// the edge of the board. Cut is the better failure: it is visible and it is local.
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

	// ── Explanation ─────────────────────────────────────────────────────────────────────────────
	//
	// The eyebrow is a dot and a word in the type colour rather than a highlighter: the dot is the
	// element bound to the highlight pulse, so the thing naming the panel is the thing tying it to
	// its object. Audio is a transport, not a fill creeping under text being read.
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

		// [0] Idle — the whole width offers to play it.
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

		// [1] Playing — pause, a thin track, elapsed time in tabular figures.
		{
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, ButtonH);
			UHorizontalBox* Transport = New<UHorizontalBox>(Tree);
			Box->AddChild(Transport);

			USizeBox* PauseBox = Fixed(Tree, nullptr, ButtonH, ButtonH);
			UButton* Pause = New<UButton>(Tree, TEXT("ExplainPauseButton"));
			Pause->SetStyle(ButtonFace(Fill(), Divider(), RuleW));

			// Two bars rather than a glyph: Montserrat has no pause character, and two rectangles
			// are a pause icon in every typeface there has ever been.
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

		// [2] Read — the one affirmative button, in the one accent.
		{
			USizeBox* Box = Fixed(Tree, nullptr, 0.f, ButtonH);
			Box->AddChild(PillButton(Tree, TEXT("ExplainConfirmButton"), nullptr, Blue(), OnBlue(),
				NSLOCTEXT("FlexXR", "PanelGotIt", "Got it"), false));
			FootSwitch->AddChild(Box);
		}
	}

	// ── Procedure ───────────────────────────────────────────────────────────────────────────────
	//
	// The meter is one bar per compiled step, the list is those same steps, and the open row is the
	// only one that opens to its full text. The foot carries the run's facts and gives that space
	// up to a mistake, rather than pushing the instruction off the panel to make room for one.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);
		Stack(Screen, Body, FMargin(Gutter, 36.f, Gutter, 0.f), true);

		Stack(Body, Label(Tree, nullptr, TypeEyebrow(), Text3(),
			NSLOCTEXT("FlexXR", "PanelCurrentStep", "CURRENT STEP")));

		Stack(Body, Label(Tree, TEXT("StepTitleLabel"), TypeHeading(), Text(),
			FText::GetEmpty(), LineHeading, true), FMargin(0.f, 10.f, 0.f, 0.f));

		UScrollBox* Scroll = New<UScrollBox>(Tree, TEXT("StepScroll"));
		Stack(Body, Scroll, FMargin(0.f, 26.f, 0.f, 0.f), true);

		UBorder* ListCard = Card(Tree, nullptr,
			RingedBox(Fill2(), ListRadius, Divider(), PanelRingW), FMargin(0.f));
		ListCard->SetClipping(EWidgetClipping::ClipToBounds);

		// Clear of the scroll rail, so the list's right edge is not under it.
		if (UScrollBoxSlot* CardSlot = Cast<UScrollBoxSlot>(Scroll->AddChild(ListCard)))
		{
			CardSlot->SetPadding(FMargin(0.f, 0.f, 18.f, 0.f));
		}

		// Read, not pressed — so these rows are allowed below the 80 px floor that anything
		// laser-targetable needs. Nothing in this list is a target.
		UVerticalBox* List = New<UVerticalBox>(Tree, TEXT("StepList"));
		ListCard->AddChild(List);

		UWidgetSwitcher* FootSwitch = New<UWidgetSwitcher>(Tree, TEXT("FootSwitcher"));
		Stack(Screen, FootSwitch);

		// [0] The run's own facts.
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

		// [1] The complaint, in the same space.
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
				FText::GetEmpty(), 1.2f, true), FMargin(0.f, 0.f, Gutter, 0.f), true);

			Bed->AddChild(Box);
			Stack(Foot, Bed);
			FootSwitch->AddChild(Foot);
		}
	}

	// ── Complete ────────────────────────────────────────────────────────────────────────────────
	//
	// One panel for both verdicts. The ring always means the score, so colour and the eyebrow carry
	// the outcome and nothing has to be re-learned — and a failed run gets the one line a passed run
	// does not need, saying why it ended.
	{
		UVerticalBox* Screen = New<UVerticalBox>(Tree);
		Switcher->AddChild(Screen);

		UVerticalBox* Body = New<UVerticalBox>(Tree);
		Body->SetClipping(EWidgetClipping::ClipToBounds);

		// No bottom padding, like the other three screens. The design pads this body on all four
		// sides and then sits a padded foot under it, which only works in the browser because the
		// body there is allowed to shrink below its content (`min-height: 0`). A vertical box has no
		// equivalent: its content overflows instead, and here it overflowed by exactly that 52 px -
		// a failed run measures 918 against an 870 panel. The spacer and the foot's own padding
		// already give the gap the bottom padding was for.
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

		// A score is not a control. Without this the rig's laser can drag the number it is reading.
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
		DefRow(Tree, Defs, NSLOCTEXT("FlexXR", "PanelHints", "Hints taken"), TEXT("ResultHintsValue"));
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

	// Compiled before saving, or the generated widgets have no Blueprint variables yet and the C++
	// side binds to nothing. Skipping this is what looks like "the panel never updates".
	FKismetEditorUtilities::CompileBlueprint(Blueprint);

#if WITH_EDITORONLY_DATA
	// The designer opens at the physical panel's shape rather than a default square, so what an
	// author lays out is what a headset shows.
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
