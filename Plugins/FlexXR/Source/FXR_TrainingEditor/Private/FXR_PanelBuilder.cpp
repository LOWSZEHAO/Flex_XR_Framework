// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_PanelBuilder.h"

#include "AssetToolsModule.h"
#include "Blueprint/WidgetTree.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "FileHelpers.h"
#include "IAssetTools.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Styling/CoreStyle.h"
#include "Types/FXR_LogChannels.h"
#include "UI/FXR_TrainingPanel.h"
#include "WidgetBlueprint.h"
#include "WidgetBlueprintFactory.h"

/**
 * THE DESIGN, AND WHERE EVERY NUMBER COMES FROM
 *
 * The unit that governs VR text is **angular size**, not pixels. A panel is a quad some distance
 * away, so what reaches the eye is an angle, and the same pixel height that is comfortable at 2 m
 * is unreadable at 5. The convention for this is dmm: one dmm is a 1 mm feature seen from 1 m, so
 * it is an angle wearing a length's clothes. Reading studies on Quest-class hardware put preferred
 * body text near 41 dmm with a wide spread, roughly 27 to 55; short strings sit at the low end of
 * that, and short strings are all this panel shows.
 *
 * So the panel is authored against a physical size rather than a screen:
 *
 *     Draw Size 1200 x 720 px at a 60 cm Panel Width  ->  exactly 20 px per centimetre
 *
 * That one number is what turns the rest from magic constants into design. At a 1.5 m reading
 * distance, which is where an instruction panel beside a task tends to sit:
 *
 *     Instruction     96 px  = 4.8 cm  = 32 dmm     the line you actually read
 *     Step counter    56 px  = 2.8 cm  = 19 dmm     glanced at, never read
 *     Mistake         64 px  = 3.2 cm  = 21 dmm
 *     Score          180 px  = 9.0 cm  = 60 dmm     legible across a room
 *
 * Contrast is held at or above 7:1 for anything that is read, which is where those same studies
 * settled, and comfortably past Meta's published 4.5:1 floor. Light on dark, because that is what
 * subjects chose and because a bright panel in a headset is a lamp pointed at the face.
 *
 * **An instruction has about 40 characters.** 1200 px less two 80 px gutters leaves 1040 px of
 * line, and 96 px text averages near 48 px a character, so roughly 21 per line over the two lines
 * the block allows. Author step text to that and it never clips. This is the one constraint worth
 * knowing before writing a step graph.
 *
 * The single distinctive element is the top edge: the panel's own border *is* the progress bar,
 * filling left to right as the procedure advances. It carries the most important fact, how far
 * through you are, with no words and without a second widget competing for the same job.
 *
 * Nothing here is bound. Every widget is named to match a `meta = (BindWidgetOptional)` pointer on
 * UFXR_TrainingPanel, which writes them when something changes rather than polling every frame, and
 * which keeps the logic in C++ where the framework's rules require it.
 */

namespace
{
	const FString PanelPath = TEXT("/FlexXR/UI");
	const FString PanelAsset = TEXT("WBP_FXR_TrainingPanel");

	// Authored at 20 px per centimetre. Changing these means redoing the dmm arithmetic above.
	constexpr int32 CanvasWidth = 1200;
	constexpr int32 CanvasHeight = 720;
	constexpr float Gutter = 80.f;
	constexpr float BarHeight = 14.f;

	/** Hex the way a colour picker shows it. Slate stores linear, and the conversion is not a divide. */
	FLinearColor Srgb(uint8 R, uint8 G, uint8 B, float A = 1.f)
	{
		FLinearColor Out = FLinearColor::FromSRGBColor(FColor(R, G, B));
		Out.A = A;
		return Out;
	}

	UTextBlock* MakeLabel(UWidgetTree& Tree, FName Name, int32 Size, const FLinearColor& Colour,
		bool bBold = false, int32 Tracking = 0, bool bWrap = false)
	{
		UTextBlock* Block = Tree.ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);

		// Without this the widget is not a Blueprint variable, and BindWidgetOptional on the C++
		// side has nothing to bind to. The panel would build and then never update.
		Block->bIsVariable = true;

		FSlateFontInfo Font = FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size);
		Font.LetterSpacing = Tracking;
		Block->SetFont(Font);
		Block->SetColorAndOpacity(FSlateColor(Colour));
		Block->SetAutoWrapText(bWrap);
		return Block;
	}

	UBorder* MakeCard(UWidgetTree& Tree, FName Name, const FLinearColor& Colour, float Radius,
		const FMargin& Padding)
	{
		UBorder* Border = Tree.ConstructWidget<UBorder>(UBorder::StaticClass(), Name);
		Border->bIsVariable = true;
		Border->SetBrush(FSlateRoundedBoxBrush(Colour, Radius));
		Border->SetPadding(Padding);
		Border->SetHorizontalAlignment(HAlign_Fill);
		Border->SetVerticalAlignment(VAlign_Center);
		return Border;
	}

	/**
	 * Put a widget on the canvas.
	 *
	 * Canvas offsets change meaning per axis: where an anchor is stretched they are margins, and
	 * where it is a point they are position and size. Worth saying once here rather than wondering
	 * at each call site why a 300 means a height in one place and an inset in another.
	 */
	UCanvasPanelSlot* Place(UCanvasPanel& Canvas, UWidget* Widget, const FAnchors& Anchors,
		const FMargin& Offsets, bool bAutoSize = false)
	{
		UCanvasPanelSlot* Slot = Canvas.AddChildToCanvas(Widget);
		Slot->SetAnchors(Anchors);
		Slot->SetOffsets(Offsets);
		Slot->SetAlignment(FVector2D::ZeroVector);
		Slot->SetAutoSize(bAutoSize);
		return Slot;
	}
}

bool UFXR_PanelBuilder::BuildDefaultTrainingPanel(bool bOverwrite)
{
	const FString Full = PanelPath / PanelAsset;
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
		Blueprint = LoadObject<UWidgetBlueprint>(nullptr, *(Full + TEXT(".") + PanelAsset));
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
			AssetTools.CreateAsset(PanelAsset, PanelPath, UWidgetBlueprint::StaticClass(), Factory));
	}

	if (!Blueprint || !Blueprint->WidgetTree)
	{
		UE_LOG(LogFXR, Error, TEXT("Could not create %s."), *Full);
		return false;
	}

	UWidgetTree& Tree = *Blueprint->WidgetTree;

	// --- tokens ------------------------------------------------------------------------------
	const FLinearColor Surface = Srgb(0x14, 0x17, 0x1C, 0.92f);  // near-black; pure black smears
	const FLinearColor Edge = Srgb(0x2A, 0x2F, 0x38);            // the progress track
	const FLinearColor Text = Srgb(0xF2, 0xF4, 0xF7);            // 15:1 on surface
	const FLinearColor Muted = Srgb(0x9A, 0xA4, 0xB2);           // 7:1, the studies' floor
	const FLinearColor Accent = Srgb(0x4D, 0xA3, 0xFF);          // progress, and nothing else
	const FLinearColor Alarm = Srgb(0xFF, 0x6B, 0x5A);           // a mistake, and nothing else
	const FLinearColor AlarmBed = Srgb(0x2A, 0x17, 0x14, 0.95f); // what a mistake sits on
	const FLinearColor Good = Srgb(0x4E, 0xD6, 0xA4);            // a finished run

	// --- tree --------------------------------------------------------------------------------
	// The factory seeds a root of its own from project settings, which may be any widget type.
	if (Tree.RootWidget)
	{
		Tree.RemoveWidget(Tree.RootWidget);
	}

	UCanvasPanel* Canvas = Tree.ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	Tree.RootWidget = Canvas;

	// Child order is draw order, so this reads bottom layer upwards.

	UBorder* Backplate = MakeCard(Tree, TEXT("Backplate"), Surface, 12.f, FMargin(0.f));
	Place(*Canvas, Backplate, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f));

	// The signature element: the panel's top edge is the progress bar.
	UProgressBar* Progress = Tree.ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ProgressFill"));
	Progress->bIsVariable = true;
	Progress->SetPercent(0.f);
	Progress->SetFillColorAndOpacity(Accent);
	{
		// The fill brush stays white and takes its colour from FillColorAndOpacity, so the one
		// place the accent is written is the one place C++ would change it.
		FProgressBarStyle Style;
		Style.BackgroundImage = FSlateRoundedBoxBrush(Edge, 0.f);
		Style.FillImage = FSlateRoundedBoxBrush(FLinearColor::White, 0.f);
		Progress->SetWidgetStyle(Style);
	}
	Place(*Canvas, Progress, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(0.f, 0.f, 0.f, BarHeight));

	// Tracked out, because at this size a bare "Step 2 / 7" otherwise reads as a fragment of the
	// sentence under it rather than as a label.
	UTextBlock* StepLabel = MakeLabel(Tree, TEXT("ProgressLabel"), 56, Muted, true, 120);
	StepLabel->SetText(NSLOCTEXT("FlexXR", "PanelStepPlaceholder", "Step 1 / 1"));
	Place(*Canvas, StepLabel, FAnchors(0.f, 0.f, 0.f, 0.f), FMargin(Gutter, 54.f, 0.f, 0.f), true);

	UTextBlock* Instruction = MakeLabel(Tree, TEXT("InstructionLabel"), 96, Text, true, 0, true);
	Instruction->SetText(NSLOCTEXT("FlexXR", "PanelInstructionPlaceholder", "Instruction appears here"));
	Place(*Canvas, Instruction, FAnchors(0.f, 0.f, 1.f, 0.f), FMargin(Gutter, 150.f, Gutter, 300.f));

	// Anchored to the bottom, so a complaint arrives from under the instruction instead of pushing
	// it down the panel. C++ collapses it whenever there is nothing to say.
	UBorder* MistakeCard = MakeCard(Tree, TEXT("MistakeCard"), AlarmBed, 8.f, FMargin(36.f, 24.f, 36.f, 24.f));
	Place(*Canvas, MistakeCard, FAnchors(0.f, 1.f, 1.f, 1.f), FMargin(Gutter, -200.f, Gutter, 140.f));

	UTextBlock* MistakeLabel = MakeLabel(Tree, TEXT("MistakeLabel"), 64, Alarm, true, 0, true);
	MistakeLabel->SetText(NSLOCTEXT("FlexXR", "PanelMistakePlaceholder", "Not that one."));
	MistakeCard->AddChild(MistakeLabel);

	// The finished run, over the step view. Inset by the bar's height so the full progress bar
	// stays visible behind it, which is the moment it reads as complete.
	UBorder* Completion = MakeCard(Tree, TEXT("CompletionCard"), Surface, 12.f,
		FMargin(Gutter, Gutter, Gutter, Gutter));
	Place(*Canvas, Completion, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(0.f, BarHeight, 0.f, 0.f));

	UVerticalBox* Stack = Tree.ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("CompletionStack"));
	Completion->AddChild(Stack);

	UTextBlock* Score = MakeLabel(Tree, TEXT("ScoreLabel"), 180, Good, true);
	Score->SetJustification(ETextJustify::Center);
	Score->SetText(NSLOCTEXT("FlexXR", "PanelScorePlaceholder", "100"));
	Stack->AddChildToVerticalBox(Score);

	UTextBlock* Summary = MakeLabel(Tree, TEXT("SummaryLabel"), 56, Muted);
	Summary->SetJustification(ETextJustify::Center);
	Summary->SetText(NSLOCTEXT("FlexXR", "PanelSummaryPlaceholder", "0 of 0 steps     0 mistakes     0:00"));
	if (UVerticalBoxSlot* SummarySlot = Stack->AddChildToVerticalBox(Summary))
	{
		SummarySlot->SetPadding(FMargin(0.f, 16.f, 0.f, 0.f));
	}

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
		Defaults->DesignTimeSize = FVector2D(CanvasWidth, CanvasHeight);
		Defaults->DesignSizeMode = EDesignPreviewSizeMode::Custom;
	}
#endif

	// Saved without a prompt: this is a tool, and a dialog here is the difference between a script
	// that works unattended and one that quietly waits forever.
	UPackage* Package = Blueprint->GetOutermost();
	Package->MarkPackageDirty();
	FEditorFileUtils::PromptForCheckoutAndSave({ Package }, /*bCheckDirty*/ false, /*bPromptToSave*/ false);

	UE_LOG(LogFXR, Warning, TEXT("Built %s at %dx%d. Put it in an FXR_Panel with Panel Width 60."),
		*Full, CanvasWidth, CanvasHeight);
	return true;
}
