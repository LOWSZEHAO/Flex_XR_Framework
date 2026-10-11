// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "UI/FXR_TrainingPanel.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/FXR_TrainingSession.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/RadialSlider.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Types/FXR_LogChannels.h"
#include "UI/FXR_PanelStyle.h"

namespace
{
	/** Montserrat has no check mark (verified against its cmap), so a done row is a green disc. */
	const FString Chevron = TEXT("›");

	/** The clock in the foot. Twice a second, so a rolling second never looks stuck. */
	constexpr float ClockPeriod = 0.5f;

	FText ModeToText(EFXR_DeliveryMode Mode)
	{
		return UEnum::GetDisplayValueAsText(Mode);
	}

	/** Collapsed, not Hidden: a hidden widget keeps its space and leaves a hole where it was. */
	void SetShown(UWidget* Widget, bool bShown, ESlateVisibility WhenShown = ESlateVisibility::SelfHitTestInvisible)
	{
		if (Widget)
		{
			Widget->SetVisibility(bShown ? WhenShown : ESlateVisibility::Collapsed);
		}
	}
}

UFXR_TrainingPanel::UFXR_TrainingPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultMistakeMessage = NSLOCTEXT("FlexXR", "PanelDefaultMistake", "Not that one.");
}

void UFXR_TrainingPanel::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtons();

	if (!Session.IsValid())
	{
		BindToSession(FindSessionInWorld());
	}
	else
	{
		// Already bound, so BindToSession will not run and the widgets have never been written.
		Refresh();
	}
}

void UFXR_TrainingPanel::NativeDestruct()
{
	// Unbinds and kills both timers. A widget torn down mid-complaint must not come back on a timer
	// that outlived it, and a clock with no panel to write to is a tick for nothing.
	StopClock();
	BindToSession(nullptr);

	Super::NativeDestruct();
}

void UFXR_TrainingPanel::BindButtons()
{
	// Cleared first so a reconstruct cannot stack a second binding on the same button.
	if (BeginButton)
	{
		BeginButton->OnClicked.RemoveAll(this);
		BeginButton->OnClicked.AddDynamic(this, &UFXR_TrainingPanel::HandleBeginClicked);
	}

	if (DoneButton)
	{
		DoneButton->OnClicked.RemoveAll(this);
		DoneButton->OnClicked.AddDynamic(this, &UFXR_TrainingPanel::HandleDoneClicked);
	}

	if (ExplainPlayButton)
	{
		ExplainPlayButton->OnClicked.RemoveAll(this);
		ExplainPlayButton->OnClicked.AddDynamic(this, &UFXR_TrainingPanel::HandleExplainPlayClicked);
	}

	if (ExplainPauseButton)
	{
		ExplainPauseButton->OnClicked.RemoveAll(this);
		ExplainPauseButton->OnClicked.AddDynamic(this, &UFXR_TrainingPanel::HandleExplainPauseClicked);
	}

	if (ExplainConfirmButton)
	{
		ExplainConfirmButton->OnClicked.RemoveAll(this);
		ExplainConfirmButton->OnClicked.AddDynamic(this, &UFXR_TrainingPanel::HandleExplainConfirmClicked);
	}
}

void UFXR_TrainingPanel::BindToSession(UFXR_TrainingSession* InSession)
{
	if (UFXR_TrainingSession* Previous = Session.Get())
	{
		Previous->OnStepActivated.RemoveAll(this);
		Previous->OnMistake.RemoveAll(this);
		Previous->OnSessionFinished.RemoveAll(this);
	}

	Session = InSession;

	ClearMistake();
	StopClock();
	Instruction = FText::GetEmpty();
	StepId = NAME_None;
	Report = FFXR_SessionReport();
	bFinished = false;

	// The rows belong to the session that was being followed. Dropped rather than reused: a new
	// graph has different steps, and a stale row is worse than a missing one.
	Rows.Reset();
	MeterSegments.Reset();

	if (!InSession)
	{
		Refresh();
		return;
	}

	InSession->OnStepActivated.AddDynamic(this, &UFXR_TrainingPanel::HandleStepActivated);
	InSession->OnMistake.AddDynamic(this, &UFXR_TrainingPanel::HandleMistake);
	InSession->OnSessionFinished.AddDynamic(this, &UFXR_TrainingPanel::HandleFinished);

	// A panel built after the run started would otherwise sit blank until the next step opened,
	// which on a long step is most of the time a trainee spends looking at it.
	if (InSession->IsRunning())
	{
		Instruction = InSession->GetActiveInstruction();

		const TArray<FName> Active = InSession->GetActiveStepIds();
		if (!Active.IsEmpty())
		{
			StepId = Active[0];
		}

		StartClock();
		OnStepChanged(StepId, Instruction, InSession->GetStepsCompleted() + 1, InSession->GetStepTotal());
	}

	Refresh();
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Reading the run
// ───────────────────────────────────────────────────────────────────────────────────────────────

FText UFXR_TrainingPanel::GetProgressText() const
{
	const UFXR_TrainingSession* Current = Session.Get();
	if (!Current || Current->GetStepTotal() <= 0)
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("FlexXR", "PanelProgress", "Step {0} / {1}"),
		FText::AsNumber(Current->GetStepsCompleted()),
		FText::AsNumber(Current->GetStepTotal()));
}

float UFXR_TrainingPanel::GetProgressFraction() const
{
	const UFXR_TrainingSession* Current = Session.Get();
	const float Total = Current ? static_cast<float>(Current->GetStepTotal()) : 0.f;
	if (Total <= 0.f)
	{
		return 0.f;
	}

	return static_cast<float>(Current->GetStepsCompleted()) / Total;
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The briefing, which the runner has no opinion about
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingPanel::ShowExplanation(const FText& Body)
{
	// An empty body would put up a blank screen that nothing can dismiss, which looks exactly like
	// a frozen panel. Refused out loud instead.
	if (Body.IsEmpty())
	{
		UE_LOG(LogFXR, Warning, TEXT("%s was asked to show an explanation with no text."), *GetName());
		return;
	}

	ExplanationBody = Body;
	ExplainState = EFXR_ExplainState::Idle;
	Refresh();
}

void UFXR_TrainingPanel::SetExplanationAudio(EFXR_ExplainState State, float ElapsedSeconds, float DurationSeconds)
{
	ExplainState = State;

	if (ExplainAudioBar)
	{
		ExplainAudioBar->SetPercent(DurationSeconds > 0.f
			? FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.f, 1.f)
			: 0.f);
	}

	if (ExplainTimeLabel)
	{
		ExplainTimeLabel->SetText(FXR_PanelStyle::Clock(ElapsedSeconds));
	}

	RefreshExplanation();
}

void UFXR_TrainingPanel::DismissExplanation()
{
	if (ExplanationBody.IsEmpty())
	{
		return;
	}

	ExplanationBody = FText::GetEmpty();
	Refresh();
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Session events
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingPanel::HandleStepActivated(FName InStepId, FText InInstruction)
{
	StepId = InStepId;
	Instruction = InInstruction;

	// A new step is its own answer to the last complaint.
	ClearMistake();
	StartClock();

	const UFXR_TrainingSession* Current = Session.Get();
	const int32 Total = Current ? Current->GetStepTotal() : 0;

	// Completed plus one, not an index into the graph: a procedure that allows steps in any order
	// has no single current step, and what a trainee reads here is how far through they are.
	const int32 Number = Current ? Current->GetStepsCompleted() + 1 : 0;

	Refresh();
	OnStepChanged(StepId, Instruction, Number, Total);
}

void UFXR_TrainingPanel::HandleMistake(FFXR_Mistake Mistake)
{
	MistakeText = Mistake.Message.IsEmpty() ? DefaultMistakeMessage : Mistake.Message;

	Refresh();
	OnMistakeShown(Mistake, MistakeText);

	UWorld* World = GetWorld();
	if (MistakeDwellSeconds <= 0.f || !World)
	{
		return;
	}

	// Re-armed rather than stacked: a second slip extends the dwell instead of taking the message
	// down while the trainee is still making it.
	World->GetTimerManager().SetTimer(MistakeTimer, this, &UFXR_TrainingPanel::ClearMistake, MistakeDwellSeconds, false);
}

void UFXR_TrainingPanel::HandleFinished(FFXR_SessionReport InReport)
{
	Report = InReport;
	bFinished = true;

	Instruction = FText::GetEmpty();
	StepId = NAME_None;
	ClearMistake();
	StopClock();

	// A briefing left up would hide the report behind it.
	ExplanationBody = FText::GetEmpty();

	Refresh();
	OnSessionComplete(Report);
}

void UFXR_TrainingPanel::ClearMistake()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MistakeTimer);
	}

	// Silent when there was nothing up, so a bind or a step change does not fire a hide at a panel
	// that is already hiding it.
	if (MistakeText.IsEmpty())
	{
		return;
	}

	MistakeText = FText::GetEmpty();

	Refresh();
	OnMistakeCleared();
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Buttons
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingPanel::HandleBeginClicked()
{
	OnBeginRequested.Broadcast();

	if (bButtonsDriveSession)
	{
		if (UFXR_TrainingSession* Current = Session.Get())
		{
			Current->StartSession();

			// StartSession fires OnStepActivated for the entry step, which refreshes everything
			// except the fact that a finished run is no longer finished.
			bFinished = false;
			Report = FFXR_SessionReport();
			Refresh();
		}
	}
}

void UFXR_TrainingPanel::HandleDoneClicked()
{
	OnRetryRequested.Broadcast();

	// Same call as Begin, deliberately: "Try again" is a new run of the same graph, and a report
	// that could be dismissed into a blank panel would be a dead end.
	if (bButtonsDriveSession)
	{
		HandleBeginClicked();
	}
}

void UFXR_TrainingPanel::HandleExplainPlayClicked()
{
	OnExplanationAudioToggled.Broadcast(true);

	// Moved to Playing on the panel's own authority so the transport appears on the press rather
	// than on a round trip. Whoever owns the sound corrects it with SetExplanationAudio.
	ExplainState = EFXR_ExplainState::Playing;
	RefreshExplanation();
}

void UFXR_TrainingPanel::HandleExplainPauseClicked()
{
	OnExplanationAudioToggled.Broadcast(false);

	ExplainState = EFXR_ExplainState::Read;
	RefreshExplanation();
}

void UFXR_TrainingPanel::HandleExplainConfirmClicked()
{
	OnExplanationConfirmed.Broadcast();
	DismissExplanation();
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Screens
// ───────────────────────────────────────────────────────────────────────────────────────────────

EFXR_PanelScreen UFXR_TrainingPanel::ResolveScreen() const
{
	// A briefing is the one thing the session has no opinion about, so it wins while it is up.
	if (!ExplanationBody.IsEmpty())
	{
		return EFXR_PanelScreen::Explanation;
	}

	if (bFinished)
	{
		return EFXR_PanelScreen::Complete;
	}

	const UFXR_TrainingSession* Current = Session.Get();
	return (Current && Current->IsRunning()) ? EFXR_PanelScreen::Procedure : EFXR_PanelScreen::Start;
}

void UFXR_TrainingPanel::ApplyScreen(EFXR_PanelScreen NewScreen)
{
	const bool bChanged = NewScreen != Screen;
	Screen = NewScreen;

	if (ScreenSwitcher)
	{
		ScreenSwitcher->SetActiveWidgetIndex(static_cast<int32>(Screen));
	}

	// The bar and the meter sit above the switcher so one copy serves every screen that wants
	// them. The two that do not are the two that are about one thing at a time.
	const bool bWantsChrome = Screen == EFXR_PanelScreen::Start || Screen == EFXR_PanelScreen::Procedure;
	SetShown(TopBar, bWantsChrome);
	SetShown(StepMeter, Screen == EFXR_PanelScreen::Procedure);

	if (bChanged)
	{
		OnScreenChanged(Screen);
	}
}

void UFXR_TrainingPanel::Refresh()
{
	ApplyScreen(ResolveScreen());

	RefreshChrome();
	RefreshStart();
	RefreshExplanation();
	RefreshProcedure();
	RefreshComplete();
}

void UFXR_TrainingPanel::RefreshChrome()
{
	const UFXR_TrainingSession* Current = Session.Get();
	const FText Course = Current ? Current->GetCourseName() : FText::GetEmpty();

	if (CrumbRootLabel)
	{
		CrumbRootLabel->SetText(Course);
	}

	// A standalone procedure has no course, and a crumb with a trailing separator and nothing after
	// it reads as a missing value rather than as an absent one.
	SetShown(CrumbRootLabel, !Course.IsEmpty());
	SetShown(CrumbSeparator, !Course.IsEmpty());

	if (CrumbLeafLabel)
	{
		CrumbLeafLabel->SetText(Current ? Current->GetProcedureName() : FText::GetEmpty());
	}

	const bool bRunning = Current && Current->IsRunning();

	if (StatusPip)
	{
		StatusPip->SetBrush(FXR_PanelStyle::Dot(
			bRunning ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text4(), 14.f));
	}

	if (StatusLabel)
	{
		// Terse during a run, because the bar is glanced at rather than read. The spelled-out form
		// is what ProgressLabel is for.
		StatusLabel->SetText(bRunning
			? FText::Format(NSLOCTEXT("FlexXR", "PanelStatusRunning", "{0} / {1}"),
				FText::AsNumber(Current->GetStepsCompleted()),
				FText::AsNumber(Current->GetStepTotal()))
			: (bFinished
				? NSLOCTEXT("FlexXR", "PanelStatusFinished", "Finished")
				: NSLOCTEXT("FlexXR", "PanelStatusIdle", "Not started")));
	}
}

void UFXR_TrainingPanel::RefreshStart()
{
	const UFXR_TrainingSession* Current = Session.Get();

	if (StartTitleLabel)
	{
		StartTitleLabel->SetText(Current ? Current->GetProcedureName() : FText::GetEmpty());
	}

	if (StartStepsValue)
	{
		StartStepsValue->SetText(FText::AsNumber(Current ? Current->GetStepTotal() : 0));
	}

	if (StartModeValue)
	{
		StartModeValue->SetText(Current ? ModeToText(Current->GetMode()) : FText::GetEmpty());
	}
}

void UFXR_TrainingPanel::RefreshExplanation()
{
	if (ExplanationLabel)
	{
		ExplanationLabel->SetText(ExplanationBody);
	}

	if (ExplainPip)
	{
		// Lit while it is speaking, quiet once it has been heard: the pip is the element bound to
		// the highlight pulse, so it says "this panel is talking about that object, now".
		const bool bLive = ExplainState == EFXR_ExplainState::Playing;
		ExplainPip->SetBrush(FXR_PanelStyle::Dot(
			bLive ? FXR_PanelStyle::Amber() : FXR_PanelStyle::Text4(), 16.f));
	}

	if (ExplainFootSwitcher)
	{
		ExplainFootSwitcher->SetActiveWidgetIndex(static_cast<int32>(ExplainState));
	}
}

void UFXR_TrainingPanel::RefreshProcedure()
{
	const UFXR_TrainingSession* Current = Session.Get();

	if (InstructionLabel)
	{
		InstructionLabel->SetText(Instruction);
	}

	if (ProgressLabel)
	{
		ProgressLabel->SetText(GetProgressText());
	}

	if (ProgressFill)
	{
		ProgressFill->SetPercent(GetProgressFraction());
	}

	if (MistakeLabel)
	{
		MistakeLabel->SetText(MistakeText);
	}

	SetShown(MistakeCard, HasMistake());

	// The foot carries the run's facts and gives that space up to a complaint, rather than pushing
	// the instruction off the panel to make room for one.
	if (FootSwitcher)
	{
		FootSwitcher->SetActiveWidgetIndex(HasMistake() ? 1 : 0);
	}

	if (ModeLabel)
	{
		ModeLabel->SetText(Current ? ModeToText(Current->GetMode()) : FText::GetEmpty());
	}

	if (ElapsedLabel)
	{
		ElapsedLabel->SetText(FXR_PanelStyle::Clock(Current ? Current->GetElapsedSeconds() : 0.f));
	}

	// The row count is the compiled graph's length, so it is settled the moment a run starts and
	// never changes during one. Rebuilt only when it disagrees — a new run, or a panel that came
	// up mid-procedure.
	const int32 Total = Current ? Current->GetStepTotal() : 0;
	if (Rows.Num() != Total)
	{
		BuildStepWidgets();
	}

	RefreshStepWidgets();
}

void UFXR_TrainingPanel::RefreshComplete()
{
	const bool bFailed = Report.Outcome == EFXR_SessionOutcome::Failed;
	const bool bPassed = Report.Outcome == EFXR_SessionOutcome::Completed;

	// The ring always means the score. Only its colour carries the verdict, so there is nothing to
	// re-learn between a pass and a failure.
	const FLinearColor Verdict = bFailed
		? FXR_PanelStyle::Red()
		: (bPassed ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text3());

	// Red is the event; a red headline is not readable, so text takes the lighter tint of it.
	const FLinearColor VerdictText = bFailed
		? FXR_PanelStyle::Danger()
		: (bPassed ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text3());

	if (ScoreRing)
	{
		// Out of 100. Scoring Weights can start a run somewhere else, in which case the arc is
		// still a fraction of 100 rather than of that starting score.
		ScoreRing->SetValue(FMath::Clamp(Report.Score / 100.f, 0.f, 1.f));
		ScoreRing->SetSliderProgressColor(Verdict);
	}

	if (ScoreLabel)
	{
		ScoreLabel->SetText(FText::AsNumber(FMath::RoundToInt(Report.Score)));
		ScoreLabel->SetColorAndOpacity(FSlateColor(VerdictText));
	}

	if (CompleteEyebrowLabel)
	{
		CompleteEyebrowLabel->SetText(bFailed
			? NSLOCTEXT("FlexXR", "PanelFailed", "Procedure failed")
			: (bPassed
				? NSLOCTEXT("FlexXR", "PanelPassed", "Procedure complete")
				: NSLOCTEXT("FlexXR", "PanelStopped", "Procedure stopped")));
		CompleteEyebrowLabel->SetColorAndOpacity(FSlateColor(VerdictText));
	}

	if (CompleteTitleLabel)
	{
		CompleteTitleLabel->SetText(Report.ProcedureName);
	}

	// One line a passed run does not need. Only a failure has something to explain.
	if (ReasonLabel)
	{
		ReasonLabel->SetText(FText::Format(
			NSLOCTEXT("FlexXR", "PanelFailedReason", "Ran out of time on “{0}”."),
			Report.FailedStepName));
	}

	SetShown(ReasonLabel, bFailed && !Report.FailedStepName.IsEmpty());

	if (ResultStepsValue)
	{
		ResultStepsValue->SetText(FText::Format(NSLOCTEXT("FlexXR", "PanelStepsOf", "{0} of {1}"),
			FText::AsNumber(Report.StepsCompleted),
			FText::AsNumber(Report.StepsTotal)));
	}

	if (ResultMistakesValue)
	{
		ResultMistakesValue->SetText(FText::AsNumber(Report.Mistakes.Num()));
	}

	if (ResultHintsValue)
	{
		ResultHintsValue->SetText(FText::AsNumber(Report.HintsUsed));
	}

	if (ResultTimeValue)
	{
		ResultTimeValue->SetText(FXR_PanelStyle::Clock(Report.DurationSeconds));
	}

	if (DoneLabel)
	{
		DoneLabel->SetText(bPassed
			? NSLOCTEXT("FlexXR", "PanelDone", "Done")
			: NSLOCTEXT("FlexXR", "PanelTryAgain", "Try again"));
	}

	if (SummaryLabel)
	{
		SummaryLabel->SetText(BuildSummaryText());
	}

	SetShown(CompletionCard, bFinished);
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The step list and the meter
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingPanel::BuildStepWidgets()
{
	Rows.Reset();
	MeterSegments.Reset();

	if (StepList)
	{
		StepList->ClearChildren();
	}

	if (StepMeter)
	{
		StepMeter->ClearChildren();
	}

	const UFXR_TrainingSession* Current = Session.Get();
	if (!Current || !WidgetTree)
	{
		return;
	}

	// One call gives the step ids, labels, instructions and statuses in compiled order, which is
	// everything both the list and the meter need. Built on demand and not per frame.
	const FFXR_SessionReport Snapshot = Current->BuildReport();
	const int32 Total = Snapshot.Steps.Num();

	for (int32 Index = 0; Index < Total; ++Index)
	{
		const FFXR_StepRecord& Step = Snapshot.Steps[Index];

		// ── the meter ───────────────────────────────────────────────────────────────────────────
		if (StepMeter)
		{
			UImage* Segment = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Segment->SetBrush(FXR_PanelStyle::Band(FXR_PanelStyle::Track()));

			// The meter's height lives on its segments. The bound widget is the box itself so that
			// collapsing it takes the whole meter with it, which leaves nothing to carry a height.
			Segment->SetDesiredSizeOverride(FVector2D(0.f, FXR_PanelStyle::MeterH));

			if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(StepMeter->AddChild(Segment)))
			{
				BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
				BoxSlot->SetHorizontalAlignment(HAlign_Fill);
				BoxSlot->SetVerticalAlignment(VAlign_Fill);

				// The gap belongs between segments, not after the last one, or the meter stops
				// short of the panel edge by exactly one gap.
				if (Index < Total - 1)
				{
					BoxSlot->SetPadding(FMargin(0.f, 0.f, FXR_PanelStyle::MeterGap, 0.f));
				}
			}

			MeterSegments.Add(Segment);
		}

		// ── the row ─────────────────────────────────────────────────────────────────────────────
		if (!StepList)
		{
			continue;
		}

		// A 3 px band between rows rather than a border on one side, which a single brush cannot
		// express — and 3 px because a hairline crawls without temporal AA.
		if (Index > 0)
		{
			UImage* Rule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Rule->SetBrush(FXR_PanelStyle::Band(FXR_PanelStyle::Divider()));
			Rule->SetDesiredSizeOverride(FVector2D(0.f, FXR_PanelStyle::RuleW));
			StepList->AddChild(Rule);
		}

		FFXR_PanelStepRow Entry;

		Entry.Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Entry.Row->SetPadding(FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY));
		Entry.Row->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* Line = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Entry.Row->AddChild(Line);

		// A border sizes to its content, so the disc needs to be told how big it is. The brush
		// rounds to half its own height, which at a square box is a circle with no arithmetic.
		USizeBox* NumeralBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		NumeralBox->SetWidthOverride(FXR_PanelStyle::NumeralSize);
		NumeralBox->SetHeightOverride(FXR_PanelStyle::NumeralSize);

		Entry.Numeral = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Entry.Numeral->SetHorizontalAlignment(HAlign_Center);
		Entry.Numeral->SetVerticalAlignment(VAlign_Center);
		Entry.Numeral->SetPadding(FMargin(0.f));
		NumeralBox->AddChild(Entry.Numeral);

		Entry.NumeralLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry.NumeralLabel->SetFont(FXR_PanelStyle::TypeNumeral());
		Entry.NumeralLabel->SetText(FText::AsNumber(Index + 1));
		Entry.Numeral->AddChild(Entry.NumeralLabel);

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Line->AddChild(NumeralBox)))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		Entry.TextLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());

		// The authored instruction, not the label: the heading above already carries the label, and
		// what a trainee needs from the open row is the sentence telling them what to do.
		Entry.TextLabel->SetText(Step.Instruction.IsEmpty() ? Step.DisplayName : Step.Instruction);

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Line->AddChild(Entry.TextLabel)))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		Entry.ChevronLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry.ChevronLabel->SetFont(FXR_PanelStyle::Type(28, FXR_PanelStyle::FontSemiBold()));
		Entry.ChevronLabel->SetText(FText::FromString(Chevron));
		Entry.ChevronLabel->SetColorAndOpacity(FSlateColor(FXR_PanelStyle::Text4()));

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(Line->AddChild(Entry.ChevronLabel)))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}

		StepList->AddChild(Entry.Row);
		Rows.Add(Entry);
	}
}

void UFXR_TrainingPanel::RefreshStepWidgets()
{
	const UFXR_TrainingSession* Current = Session.Get();
	if (!Current || Rows.IsEmpty())
	{
		return;
	}

	const FFXR_SessionReport Snapshot = Current->BuildReport();
	UWidget* ScrollTo = nullptr;

	for (int32 Index = 0; Index < Rows.Num(); ++Index)
	{
		if (!Snapshot.Steps.IsValidIndex(Index))
		{
			break;
		}

		const EFXR_StepStatus Status = Snapshot.Steps[Index].Status;
		const bool bOpen = Status == EFXR_StepStatus::Active;
		const bool bDone = Status == EFXR_StepStatus::Complete;

		if (MeterSegments.IsValidIndex(Index) && MeterSegments[Index])
		{
			MeterSegments[Index]->SetBrush(FXR_PanelStyle::Band(
				bDone ? FXR_PanelStyle::Green()
				: (bOpen ? FXR_PanelStyle::Blue() : FXR_PanelStyle::Track())));
		}

		const FFXR_PanelStepRow& Entry = Rows[Index];

		if (Entry.Row)
		{
			// The open row is tinted rather than outlined, so the list keeps a single edge.
			Entry.Row->SetBrush(FXR_PanelStyle::Box(
				bOpen ? FXR_PanelStyle::ActiveRow() : FLinearColor::Transparent, 0.f));
			Entry.Row->SetPadding(bOpen
				? FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY + 14.f)
				: FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY));
		}

		if (Entry.Numeral)
		{
			// A disc when the step has a verdict, a ring while it does not. Montserrat carries no
			// check mark, so a finished step is a green disc with its own number rather than a tick
			// — and a missing-glyph box would be worse than no tick at all.
			Entry.Numeral->SetBrush(bOpen
				? FXR_PanelStyle::Dot(FXR_PanelStyle::Blue(), FXR_PanelStyle::NumeralSize)
				: (bDone
					? FXR_PanelStyle::Dot(FXR_PanelStyle::Green(), FXR_PanelStyle::NumeralSize)
					: FXR_PanelStyle::RingedPill(FLinearColor::Transparent, FXR_PanelStyle::Divider(), FXR_PanelStyle::RuleW)));
		}

		if (Entry.NumeralLabel)
		{
			Entry.NumeralLabel->SetColorAndOpacity(FSlateColor(
				bOpen ? FXR_PanelStyle::OnBlue()
				: (bDone ? FXR_PanelStyle::OnGreen() : FXR_PanelStyle::Text3())));
		}

		if (Entry.TextLabel)
		{
			Entry.TextLabel->SetFont(bOpen ? FXR_PanelStyle::TypeRowActive() : FXR_PanelStyle::TypeRow());
			Entry.TextLabel->SetLineHeightPercentage(
				bOpen ? FXR_PanelStyle::LineRowActive : FXR_PanelStyle::LineRow);
			Entry.TextLabel->SetColorAndOpacity(FSlateColor(
				bDone ? FXR_PanelStyle::Text2() : FXR_PanelStyle::Text()));

			// Every row is one line except the open one, which opens to its full text. The trainee
			// is on this step; this step is the one that has to be readable. A closed row ellipses
			// rather than being cut, so a truncated line says that it is truncated.
			Entry.TextLabel->SetAutoWrapText(bOpen);
			Entry.TextLabel->SetTextOverflowPolicy(
				bOpen ? ETextOverflowPolicy::Clip : ETextOverflowPolicy::Ellipsis);
		}

		SetShown(Entry.ChevronLabel, bOpen);

		if (bOpen && !ScrollTo)
		{
			ScrollTo = Entry.Row;
		}
	}

	// Centred, not merely brought into view. The minimum scroll that satisfies "visible" puts the
	// open step hard against the bottom edge, under the fade, which is the one place it must not be.
	if (ScrollTo && StepScroll)
	{
		StepScroll->ScrollWidgetIntoView(ScrollTo, true, EDescendantScrollDestination::Center);
	}
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The clock
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingPanel::StartClock()
{
	UWorld* World = GetWorld();
	if (!World || !ElapsedLabel || ClockTimer.IsValid())
	{
		return;
	}

	// The one thing on this panel that has to tick. Everything else is written when it changes, and
	// a timer costs a fraction of what a property binding on every widget would.
	World->GetTimerManager().SetTimer(ClockTimer, this, &UFXR_TrainingPanel::TickClock, ClockPeriod, true);
}

void UFXR_TrainingPanel::StopClock()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ClockTimer);
	}

	ClockTimer.Invalidate();
}

void UFXR_TrainingPanel::TickClock()
{
	const UFXR_TrainingSession* Current = Session.Get();
	if (!Current || !Current->IsRunning())
	{
		StopClock();
		return;
	}

	if (ElapsedLabel)
	{
		ElapsedLabel->SetText(FXR_PanelStyle::Clock(Current->GetElapsedSeconds()));
	}
}

// ───────────────────────────────────────────────────────────────────────────────────────────────

FText UFXR_TrainingPanel::BuildSummaryText() const
{
	return FText::Format(
		NSLOCTEXT("FlexXR", "PanelSummary", "{0} of {1} steps     {2} mistakes     {3}"),
		FText::AsNumber(Report.StepsCompleted),
		FText::AsNumber(Report.StepsTotal),
		FText::AsNumber(Report.Mistakes.Num()),
		FXR_PanelStyle::Clock(Report.DurationSeconds));
}

UFXR_TrainingSession* UFXR_TrainingPanel::FindSessionInWorld() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UFXR_TrainingSession* First = nullptr;
	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UFXR_TrainingSession* Candidate = It->FindComponentByClass<UFXR_TrainingSession>())
		{
			++Found;
			if (!First)
			{
				First = Candidate;
			}
		}
	}

	// One session is the ordinary case and wiring a panel by hand is a step someone forgets. Two is
	// a decision the panel is not entitled to make silently.
	if (Found > 1)
	{
		UE_LOG(LogFXR, Warning,
			TEXT("%s found %d training sessions and bound the first. Call Bind To Session to choose one."),
			*GetName(), Found);
	}

	return First;
}
