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

	/** "2", or "2 of 3" when there is a limit to count against. */
	FText MistakeCountText(int32 Count, int32 Limit)
	{
		return Limit > 0
			? FText::Format(NSLOCTEXT("FlexXR", "PanelMistakeCountOf", "{0} of {1}"),
				FText::AsNumber(Count), FText::AsNumber(Limit))
			: FText::AsNumber(Count);
	}

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
	DefaultMistakeMessage = NSLOCTEXT("FlexXR", "PanelDefaultMistake", "Wrong object.");
}

void UFXR_TrainingPanel::NativePreConstruct()
{
	Super::NativePreConstruct();

	// The designer only. At runtime the session drives every widget, and the editor never calls
	// NativeConstruct on a design-time preview, so nothing here can leak into a run.
	if (IsDesignTime())
	{
		FillDesignPreview();
	}
}

void UFXR_TrainingPanel::FillDesignPreview()
{
	// Whichever screen the switcher is on. Changing its Active Widget Index in the designer rebuilds
	// the preview, which runs this again.
	const int32 Index = ScreenSwitcher ? ScreenSwitcher->GetActiveWidgetIndex() : 0;
	const EFXR_PanelScreen Previewed = static_cast<EFXR_PanelScreen>(FMath::Clamp(Index, 0, 3));

	// The same chrome rule as ApplyScreen, applied without moving the switcher and without firing
	// OnScreenChanged into a Blueprint graph from inside the editor.
	SetShown(TopBar, Previewed == EFXR_PanelScreen::Start || Previewed == EFXR_PanelScreen::Procedure);
	SetShown(StepMeter, Previewed == EFXR_PanelScreen::Procedure);

	// The design page's own sample content, so the preview can be held up against it board for
	// board. Culture-invariant: a trainee never sees any of this, and none of it should be gathered
	// for translation.
	const FText Course = INVTEXT("Fire safety");
	const FText Procedure = INVTEXT("Extinguisher drill");
	const bool bRunning = Previewed == EFXR_PanelScreen::Procedure;

	if (CrumbRootLabel) { CrumbRootLabel->SetText(Course); }
	if (CrumbLeafLabel) { CrumbLeafLabel->SetText(Procedure); }
	if (StatusLabel) { StatusLabel->SetText(bRunning ? INVTEXT("2 / 5") : INVTEXT("Not started")); }
	if (StatusPip)
	{
		StatusPip->SetBrush(FXR_PanelStyle::Dot(bRunning ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text4(), 14.f));
	}

	// ── Start ───────────────────────────────────────────────────────────────────────────────────
	if (StartTitleLabel) { StartTitleLabel->SetText(Procedure); }
	if (StartStepsValue) { StartStepsValue->SetText(INVTEXT("5")); }
	if (StartModeValue) { StartModeValue->SetText(INVTEXT("Exam")); }

	// ── Explanation ─────────────────────────────────────────────────────────────────────────────
	if (ExplanationLabel)
	{
		ExplanationLabel->SetText(INVTEXT("A fire has started in the waste bin beside the workbench.\n\n"
			"Collect the extinguisher from the wall bracket before the smoke reaches the ceiling."));
	}

	// The foot is left on whichever state the designer chose, and the pip follows it.
	const bool bPlaying = ExplainFootSwitcher
		&& ExplainFootSwitcher->GetActiveWidgetIndex() == static_cast<int32>(EFXR_ExplainState::Playing);
	if (ExplainPip)
	{
		ExplainPip->SetBrush(FXR_PanelStyle::Dot(bPlaying ? FXR_PanelStyle::Amber() : FXR_PanelStyle::Text4(), 16.f));
	}
	if (ExplainAudioBar) { ExplainAudioBar->SetPercent(0.42f); }
	if (ExplainTimeLabel) { ExplainTimeLabel->SetText(INVTEXT("0:12")); }

	// ── Procedure ───────────────────────────────────────────────────────────────────────────────
	if (StepTitleLabel) { StepTitleLabel->SetText(INVTEXT("Pull the safety pin")); }

	// Left on whichever body the designer chose, so flipping ProcedureBodySwitcher previews Exam.
	if (ExamProgressLabel) { ExamProgressLabel->SetText(INVTEXT("2 / 5")); }
	if (ExamMistakesValue) { ExamMistakesValue->SetText(INVTEXT("1 of 3")); }
	if (ModeLabel) { ModeLabel->SetText(INVTEXT("Training")); }
	if (ElapsedLabel) { ElapsedLabel->SetText(INVTEXT("1:46")); }

	// Filled whichever foot is showing, so flipping FootSwitcher to the mistake shows a real one.
	if (MistakeLabel) { MistakeLabel->SetText(INVTEXT("Pin first \u2014 the handle will not move.")); }

	{
		// Five steps with the third open: the meter is the procedure, the page is that one step.
		TArray<FFXR_StepRecord> Sample;
		Sample.SetNum(5);
		for (int32 Step = 0; Step < Sample.Num(); ++Step)
		{
			Sample[Step].Status = Step < 2 ? EFXR_StepStatus::Complete
				: (Step == 2 ? EFXR_StepStatus::Active : EFXR_StepStatus::Pending);
		}

		FFXR_StepRecord& Open = Sample[2];
		Open.Instruction = INVTEXT("Grip the safety pin and pull it free of the handle");

#if WITH_EDITORONLY_DATA
		if (bPreviewSubSteps)
		{
			const TCHAR* Parts[] = {
				TEXT("Twist the pin to break the tamper seal"),
				TEXT("Pull the pin straight out of the handle"),
				TEXT("Set the pin down clear of your feet"),
			};

			for (int32 Part = 0; Part < UE_ARRAY_COUNT(Parts); ++Part)
			{
				FFXR_SubStepRecord& Sub = Open.SubSteps.AddDefaulted_GetRef();
				Sub.DisplayName = FText::AsCultureInvariant(Parts[Part]);
				Sub.Status = Part < 1 ? EFXR_StepStatus::Complete
					: (Part == 1 ? EFXR_StepStatus::Active : EFXR_StepStatus::Pending);
			}
		}
#endif

		// The same calls a run makes, so what the designer shows is what the code draws.
		BuildMeter(Sample.Num());
		RefreshMeter(Sample);

		const TArray<FFXR_SubStepRecord> Lines = PageLines(Open);
		BuildRows(Lines);
		RefreshRows(Lines);
	}

	// ── Complete ────────────────────────────────────────────────────────────────────────────────
	{
		FFXR_SessionReport Sample;
		Sample.ProcedureName = Procedure;
		Sample.CourseName = Course;
		Sample.StepsTotal = 5;

#if WITH_EDITORONLY_DATA
		const bool bFailed = bPreviewFailedRun;
		const bool bTraining = bPreviewTrainingRun && !bPreviewFailedRun;
#else
		const bool bFailed = false;
		const bool bTraining = false;
#endif

		Sample.Mode = bTraining ? EFXR_DeliveryMode::Training : EFXR_DeliveryMode::Exam;

		if (bFailed)
		{
			Sample.Outcome = EFXR_SessionOutcome::Failed;
			Sample.Score = 0.f;
			Sample.StepsCompleted = 2;
			Sample.DurationSeconds = 130.f;
			Sample.Mistakes.SetNum(3);
			Sample.FailedStepName = INVTEXT("Pull the safety pin");
			Sample.FailReason = EFXR_FailReason::Timeout;
		}
		else
		{
			Sample.Outcome = EFXR_SessionOutcome::Completed;
			Sample.Score = 92.f;
			Sample.StepsCompleted = 5;
			Sample.DurationSeconds = 221.f;
			Sample.Mistakes.SetNum(bTraining ? 0 : 2);
		}

		// RefreshComplete reads nothing but the report, so it can draw the sample as it is.
		Report = Sample;
		RefreshComplete();
	}
}

void UFXR_TrainingPanel::NativeConstruct()
{
	Super::NativeConstruct();

	BindButtons();

	if (!Session.IsValid())
	{
		BindToSession(UFXR_TrainingSession::FindInWorld(this));
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
		Previous->OnSubStepActivated.RemoveAll(this);
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
	PageStep = INDEX_NONE;

	if (!InSession)
	{
		Refresh();
		return;
	}

	InSession->OnStepActivated.AddDynamic(this, &UFXR_TrainingPanel::HandleStepActivated);
	InSession->OnSubStepActivated.AddDynamic(this, &UFXR_TrainingPanel::HandleSubStepActivated);
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

void UFXR_TrainingPanel::HandleSubStepActivated(FName InStepId, FName InSubStepId)
{
	// The page and its heading stay; the open row moves down one. A complaint about the last part is
	// answered by moving on to the next, the same as between steps.
	ClearMistake();
	Refresh();
}

void UFXR_TrainingPanel::HandleMistake(FFXR_Mistake Mistake)
{
	const UFXR_TrainingSession* Current = Session.Get();

	if (Current && Current->GetMode() == EFXR_DeliveryMode::Exam)
	{
		// No hints in an exam. The author's message says what went wrong, which is the answer, so
		// the trainee is told only that it counted - and, when there is a limit, how close it is.
		const int32 Count = Current->GetMistakes().Num();
		const int32 Limit = Current->GetMaxMistakes();
		MistakeText = Limit > 0
			? FText::Format(NSLOCTEXT("FlexXR", "PanelExamMistakeOf", "Mistake {0} of {1}"),
				FText::AsNumber(Count), FText::AsNumber(Limit))
			: FText::Format(NSLOCTEXT("FlexXR", "PanelExamMistake", "Mistake {0}"), FText::AsNumber(Count));
	}
	else
	{
		MistakeText = Mistake.Message.IsEmpty() ? DefaultMistakeMessage : Mistake.Message;
	}

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

	// Exam shows progress and the mistake count and nothing else. The list and the heading are not
	// hidden so much as never chosen: the switcher holds two bodies, and an exam gets the other one.
	const bool bExam = Current && Current->GetMode() == EFXR_DeliveryMode::Exam;

	if (ProcedureBodySwitcher)
	{
		ProcedureBodySwitcher->SetActiveWidgetIndex(bExam ? 1 : 0);
	}

	if (ExamProgressLabel)
	{
		ExamProgressLabel->SetText(FText::Format(NSLOCTEXT("FlexXR", "PanelExamProgress", "{0} / {1}"),
			FText::AsNumber(Current ? Current->GetStepsCompleted() : 0),
			FText::AsNumber(Current ? Current->GetStepTotal() : 0)));
	}

	if (ExamMistakesValue)
	{
		ExamMistakesValue->SetText(Current
			? MistakeCountText(Current->GetMistakes().Num(), Current->GetMaxMistakes())
			: FText::AsNumber(0));
	}

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

	if (!Current)
	{
		return;
	}

	// One report for everything below: it carries the steps, their sub-steps, labels and statuses in
	// run order. Built on change, not per frame.
	const FFXR_SessionReport Snapshot = Current->BuildReport();
	const int32 Open = Snapshot.Steps.IndexOfByPredicate(
		[](const FFXR_StepRecord& Step) { return Step.Status == EFXR_StepStatus::Active; });

	// The heading is the open step's short label: the title of its page.
	if (StepTitleLabel)
	{
		StepTitleLabel->SetText(!Snapshot.Steps.IsValidIndex(Open)
			? FText::GetEmpty()
			: (Snapshot.Steps[Open].DisplayName.IsEmpty()
				? Snapshot.Steps[Open].Instruction
				: Snapshot.Steps[Open].DisplayName));
	}

	// The meter is the whole procedure, so its length is settled when a run starts.
	if (MeterSegments.Num() != Snapshot.Steps.Num())
	{
		BuildMeter(Snapshot.Steps.Num());
	}

	RefreshMeter(Snapshot.Steps);

	// The list is one step's page, so it is rebuilt when the open step changes. A sub-step being
	// ticked off only restyles it.
	const TArray<FFXR_SubStepRecord> Lines = Snapshot.Steps.IsValidIndex(Open)
		? PageLines(Snapshot.Steps[Open])
		: TArray<FFXR_SubStepRecord>();

	if (Open != PageStep || Rows.Num() != Lines.Num())
	{
		BuildRows(Lines);
		PageStep = Open;
	}

	RefreshRows(Lines);
}

void UFXR_TrainingPanel::RefreshComplete()
{
	const bool bFailed = Report.Outcome == EFXR_SessionOutcome::Failed;
	const bool bPassed = Report.Outcome == EFXR_SessionOutcome::Completed;

	// Training cannot go wrong, so it has nothing to grade: a finished run is a full green ring with
	// a tick, which says "done" without pretending to be a score.
	const bool bTrainingDone = bPassed && Report.Mode == EFXR_DeliveryMode::Training;

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
		ScoreRing->SetValue(bTrainingDone ? 1.f : FMath::Clamp(Report.Score / 100.f, 0.f, 1.f));
		ScoreRing->SetSliderProgressColor(Verdict);
	}

	if (ScoreLabel)
	{
		// A stopped run was never judged, so it shows no score rather than a zero that reads as one.
		ScoreLabel->SetText(Report.Outcome == EFXR_SessionOutcome::Stopped
			? FText::FromString(TEXT("\u2014"))
			: FText::AsNumber(FMath::RoundToInt(Report.Score)));
		ScoreLabel->SetColorAndOpacity(FSlateColor(VerdictText));
	}

	SetShown(ScoreLabel, !bTrainingDone);
	SetShown(ScoreTick, bTrainingDone);

	if (CompleteEyebrowLabel)
	{
		CompleteEyebrowLabel->SetText(bFailed
			? NSLOCTEXT("FlexXR", "PanelFailed", "Procedure failed")
			: (bTrainingDone
				? NSLOCTEXT("FlexXR", "PanelTrainingDone", "Training complete")
				: (bPassed
					? NSLOCTEXT("FlexXR", "PanelPassed", "Procedure complete")
					: NSLOCTEXT("FlexXR", "PanelStopped", "Procedure stopped"))));
		CompleteEyebrowLabel->SetColorAndOpacity(FSlateColor(VerdictText));
	}

	if (CompleteTitleLabel)
	{
		CompleteTitleLabel->SetText(Report.ProcedureName);
	}

	// One line a passed run does not need. Only a failure has something to explain, and there are
	// three ways to fail an exam. It names the narrowest thing that failed: the sub-step, if it was one.
	const FText FailedName = Report.FailedSubStepName.IsEmpty() ? Report.FailedStepName : Report.FailedSubStepName;

	FText Reason;
	switch (Report.FailReason)
	{
	case EFXR_FailReason::TooManyMistakes:
		Reason = FText::Format(NSLOCTEXT("FlexXR", "PanelFailedMistakes", "Too many mistakes: {0} of {1}."),
			FText::AsNumber(Report.Mistakes.Num()), FText::AsNumber(Report.MistakeLimit));
		break;
	case EFXR_FailReason::CriticalMistake:
		Reason = FText::Format(NSLOCTEXT("FlexXR", "PanelFailedCritical", "Critical mistake on \u201C{0}\u201D."),
			FailedName);
		break;
	case EFXR_FailReason::Timeout:
		Reason = FText::Format(NSLOCTEXT("FlexXR", "PanelFailedReason", "Ran out of time on \u201C{0}\u201D."),
			FailedName);
		break;
	default:
		break;
	}

	if (ReasonLabel)
	{
		ReasonLabel->SetText(Reason);
	}

	SetShown(ReasonLabel, bFailed && !Reason.IsEmpty());

	if (ResultStepsValue)
	{
		ResultStepsValue->SetText(FText::Format(NSLOCTEXT("FlexXR", "PanelStepsOf", "{0} of {1}"),
			FText::AsNumber(Report.StepsCompleted),
			FText::AsNumber(Report.StepsTotal)));
	}

	if (ResultMistakesValue)
	{
		ResultMistakesValue->SetText(MistakeCountText(Report.Mistakes.Num(), Report.MistakeLimit));
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
// The meter and the page
// ───────────────────────────────────────────────────────────────────────────────────────────────

TArray<FFXR_SubStepRecord> UFXR_TrainingPanel::PageLines(const FFXR_StepRecord& Step)
{
	if (!Step.SubSteps.IsEmpty())
	{
		return Step.SubSteps;
	}

	// A step without sub-steps is a page with one line - what to do - so every page reads the same
	// way, heading and numbered rows, whether it has parts or not.
	FFXR_SubStepRecord Line;
	Line.SubStepId = Step.StepId;
	Line.DisplayName = Step.Instruction.IsEmpty() ? Step.DisplayName : Step.Instruction;
	Line.Status = Step.Status;
	return { Line };
}

void UFXR_TrainingPanel::BuildMeter(int32 StepCount)
{
	MeterSegments.Reset();

	if (!StepMeter || !WidgetTree)
	{
		return;
	}

	StepMeter->ClearChildren();

	for (int32 Index = 0; Index < StepCount; ++Index)
	{
		// The meter's height lives in its segments' brushes. The bound widget is the box itself so
		// that collapsing it takes the whole meter with it, which leaves nothing else to carry a
		// height - and SetDesiredSizeOverride cannot, for the reason given on FXR_PanelStyle::Band.
		UImage* Segment = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Segment->SetBrush(FXR_PanelStyle::Band(FXR_PanelStyle::Track(), FXR_PanelStyle::MeterH));

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(StepMeter->AddChild(Segment)))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
			BoxSlot->SetVerticalAlignment(VAlign_Fill);

			// The gap belongs between segments, not after the last one, or the meter stops short of
			// the panel edge by exactly one gap.
			if (Index < StepCount - 1)
			{
				BoxSlot->SetPadding(FMargin(0.f, 0.f, FXR_PanelStyle::MeterGap, 0.f));
			}
		}

		MeterSegments.Add(Segment);
	}
}

void UFXR_TrainingPanel::RefreshMeter(const TArray<FFXR_StepRecord>& Steps)
{
	for (int32 Index = 0; Index < MeterSegments.Num() && Steps.IsValidIndex(Index); ++Index)
	{
		if (!MeterSegments[Index])
		{
			continue;
		}

		const EFXR_StepStatus Status = Steps[Index].Status;
		MeterSegments[Index]->SetBrush(FXR_PanelStyle::Band(
			Status == EFXR_StepStatus::Complete ? FXR_PanelStyle::Green()
			: (Status == EFXR_StepStatus::Active ? FXR_PanelStyle::Blue() : FXR_PanelStyle::Track()),
			FXR_PanelStyle::MeterH));
	}
}

void UFXR_TrainingPanel::BuildRows(const TArray<FFXR_SubStepRecord>& Lines)
{
	Rows.Reset();

	if (!StepList || !WidgetTree)
	{
		return;
	}

	StepList->ClearChildren();

	for (int32 Index = 0; Index < Lines.Num(); ++Index)
	{
		const FFXR_SubStepRecord& Line = Lines[Index];

		// A 3 px band between rows rather than a border on one side, which a single brush cannot
		// express — and 3 px because a hairline crawls without temporal AA.
		if (Index > 0)
		{
			UImage* Rule = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
			Rule->SetBrush(FXR_PanelStyle::Band(FXR_PanelStyle::Divider()));
			StepList->AddChild(Rule);
		}

		FFXR_PanelStepRow Entry;

		Entry.Row = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
		Entry.Row->SetPadding(FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY));
		Entry.Row->SetVerticalAlignment(VAlign_Center);

		UHorizontalBox* RowLine = WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass());
		Entry.Row->AddChild(RowLine);

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

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(RowLine->AddChild(NumeralBox)))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f));
		}

		Entry.TextLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry.TextLabel->SetText(Line.DisplayName.IsEmpty() ? FText::FromName(Line.SubStepId) : Line.DisplayName);

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(RowLine->AddChild(Entry.TextLabel)))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetVerticalAlignment(VAlign_Center);
		}

		Entry.ChevronLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
		Entry.ChevronLabel->SetFont(FXR_PanelStyle::Type(28, FXR_PanelStyle::FontSemiBold()));
		Entry.ChevronLabel->SetText(FText::FromString(Chevron));
		Entry.ChevronLabel->SetColorAndOpacity(FSlateColor(FXR_PanelStyle::Text4()));

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(RowLine->AddChild(Entry.ChevronLabel)))
		{
			BoxSlot->SetVerticalAlignment(VAlign_Center);
			BoxSlot->SetPadding(FMargin(12.f, 0.f, 0.f, 0.f));
		}

		StepList->AddChild(Entry.Row);
		Rows.Add(Entry);
	}
}

void UFXR_TrainingPanel::RefreshRows(const TArray<FFXR_SubStepRecord>& Lines)
{
	UWidget* ScrollTo = nullptr;

	for (int32 Index = 0; Index < Rows.Num() && Lines.IsValidIndex(Index); ++Index)
	{
		const EFXR_StepStatus Status = Lines[Index].Status;
		const bool bOpen = Status == EFXR_StepStatus::Active;
		const bool bDone = Status == EFXR_StepStatus::Complete;

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
			// A disc when the line has a verdict, a ring while it does not. Montserrat carries no
			// check mark, so a finished line is a green disc with its own number rather than a tick
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
			// is on this line; this line is the one that has to be readable. A closed row ellipses
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
	// open row hard against the bottom edge, under the fade, which is the one place it must not be.
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

