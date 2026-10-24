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
	/** Montserrat has no check mark glyph, so a finished row is a green disc instead. */
	const FString Chevron = TEXT("›");

	/** Clock update rate. Twice a second so the seconds never look stuck. */
	constexpr float ClockPeriod = 0.5f;

	/** "2", or "2 of 3" when there's a limit. */
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

	/** Collapsed, not Hidden: Hidden still takes up space. */
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

	// Designer preview only. The editor doesn't call NativeConstruct for the preview, so none of this
	// runs in game.
	if (IsDesignTime())
	{
		FillDesignPreview();
	}
}

void UFXR_TrainingPanel::FillDesignPreview()
{
	// Use whatever screen the switcher is on in the designer. Changing it re-runs the preview.
	const int32 Index = ScreenSwitcher ? ScreenSwitcher->GetActiveWidgetIndex() : 0;
	const EFXR_PanelScreen Previewed = static_cast<EFXR_PanelScreen>(FMath::Clamp(Index, 0, 3));

	// Same chrome rule as ApplyScreen, but without moving the switcher or firing OnScreenChanged
	// in the editor.
	SetShown(TopBar, Previewed == EFXR_PanelScreen::Start || Previewed == EFXR_PanelScreen::Procedure);
	SetShown(StepMeter, Previewed == EFXR_PanelScreen::Procedure);

	// Sample content matching the design mockup. Culture-invariant so it never ends up in
	// localization.
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

	// -- Start -----------------------------------------------------------------------------------
	if (StartTitleLabel) { StartTitleLabel->SetText(Procedure); }
	if (StartStepsValue) { StartStepsValue->SetText(INVTEXT("5")); }
	if (StartModeValue) { StartModeValue->SetText(INVTEXT("Exam")); }

	// -- Explanation -----------------------------------------------------------------------------
	if (ExplanationLabel)
	{
		ExplanationLabel->SetText(INVTEXT("A fire has started in the waste bin beside the workbench.\n\n"
			"Collect the extinguisher from the wall bracket before the smoke reaches the ceiling."));
	}

	// Keep whatever foot state the designer picked, and match the pip to it.
	const bool bPlaying = ExplainFootSwitcher
		&& ExplainFootSwitcher->GetActiveWidgetIndex() == static_cast<int32>(EFXR_ExplainState::Playing);
	if (ExplainPip)
	{
		ExplainPip->SetBrush(FXR_PanelStyle::Dot(bPlaying ? FXR_PanelStyle::Amber() : FXR_PanelStyle::Text4(), 16.f));
	}
	if (ExplainAudioBar) { ExplainAudioBar->SetPercent(0.42f); }
	if (ExplainTimeLabel) { ExplainTimeLabel->SetText(INVTEXT("0:12")); }

	// -- Procedure -------------------------------------------------------------------------------
	if (StepTitleLabel) { StepTitleLabel->SetText(INVTEXT("Grip the safety pin and pull it free of the handle.")); }

	// Leave the body switcher where the designer put it, so flipping it previews the Exam layout.
	if (ExamProgressLabel) { ExamProgressLabel->SetText(INVTEXT("2 / 5")); }
	if (ExamMistakesValue) { ExamMistakesValue->SetText(INVTEXT("1 of 3")); }
	if (ModeLabel) { ModeLabel->SetText(INVTEXT("Training")); }
	if (ElapsedLabel) { ElapsedLabel->SetText(INVTEXT("1:46")); }

	// Fill the mistake text too, so flipping FootSwitcher shows a real message.
	if (MistakeLabel) { MistakeLabel->SetText(INVTEXT("Pin first \u2014 the handle will not move.")); }

	{
		// Five steps with the third one open.
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

		// Same calls as at runtime.
		BuildMeter(Sample.Num());
		RefreshMeter(Sample);

		BuildRows(Open.SubSteps);
		RefreshRows(Open.SubSteps);
	}

	// -- Complete --------------------------------------------------------------------------------
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

		// RefreshComplete only reads Report, so it can draw the sample directly.
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
		// Already bound, so BindToSession won't run; refresh once so the widgets get filled.
		Refresh();
	}
}

void UFXR_TrainingPanel::NativeDestruct()
{
	// Unbind and stop both timers so nothing fires on a destroyed widget.
	StopClock();
	BindToSession(nullptr);

	Super::NativeDestruct();
}

void UFXR_TrainingPanel::BindButtons()
{
	// Remove first so reconstructing doesn't add a second binding.
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

	// Drop the old rows; a different procedure will have different steps.
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

	// If the run is already going, fill the panel now instead of waiting for the next step.
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

// -----------------------------------------------------------------------------------------------
// Progress
// -----------------------------------------------------------------------------------------------

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

// -----------------------------------------------------------------------------------------------
// Briefing
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingPanel::ShowExplanation(const FText& Body)
{
	// An empty briefing would be a blank screen nothing can dismiss. Refuse it and log.
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

// -----------------------------------------------------------------------------------------------
// Session events
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingPanel::HandleStepActivated(FName InStepId, FText InInstruction)
{
	StepId = InStepId;
	Instruction = InInstruction;

	// A new step clears the last mistake message.
	ClearMistake();
	StartClock();

	const UFXR_TrainingSession* Current = Session.Get();
	const int32 Total = Current ? Current->GetStepTotal() : 0;

	// Step number shown to the trainee: steps done + 1.
	const int32 Number = Current ? Current->GetStepsCompleted() + 1 : 0;

	Refresh();
	OnStepChanged(StepId, Instruction, Number, Total);
}

void UFXR_TrainingPanel::HandleSubStepActivated(FName InStepId, FName InSubStepId)
{
	// Same page, the open row just moves down. Clear the last mistake message like a step change does.
	ClearMistake();
	Refresh();
}

void UFXR_TrainingPanel::HandleMistake(FFXR_Mistake Mistake)
{
	const UFXR_TrainingSession* Current = Session.Get();

	if (Current && Current->GetMode() == EFXR_DeliveryMode::Exam)
	{
		// No hints in Exam: just say a mistake counted, and how many are left if there's a limit.
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

	// Restart the timer on each new mistake instead of stacking timers.
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

	// Close any briefing so it doesn't cover the report.
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

	// Nothing to clear, so don't fire OnMistakeCleared.
	if (MistakeText.IsEmpty())
	{
		return;
	}

	MistakeText = FText::GetEmpty();

	Refresh();
	OnMistakeCleared();
}

// -----------------------------------------------------------------------------------------------
// Buttons
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingPanel::HandleBeginClicked()
{
	OnBeginRequested.Broadcast();

	if (bButtonsDriveSession)
	{
		if (UFXR_TrainingSession* Current = Session.Get())
		{
			Current->StartSession();

			// StartSession refreshes through OnStepActivated, but bFinished still needs resetting here.
			bFinished = false;
			Report = FFXR_SessionReport();
			Refresh();
		}
	}
}

void UFXR_TrainingPanel::HandleDoneClicked()
{
	OnRetryRequested.Broadcast();

	// Try again = start a new run of the same procedure.
	if (bButtonsDriveSession)
	{
		HandleBeginClicked();
	}
}

void UFXR_TrainingPanel::HandleExplainPlayClicked()
{
	OnExplanationAudioToggled.Broadcast(true);

	// Switch to Playing right away so the button feels instant. Whoever plays the sound can
	// correct it with SetExplanationAudio.
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

// -----------------------------------------------------------------------------------------------
// Screens
// -----------------------------------------------------------------------------------------------

EFXR_PanelScreen UFXR_TrainingPanel::ResolveScreen() const
{
	// A briefing overrides everything else while it's up.
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

	// The bar and meter sit above the switcher and are shared. Explanation and Complete don't use them.
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

	// No course: hide the crumb and its separator so it doesn't look like a missing value.
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
		// Short form for the top bar. ProgressLabel has the long form.
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
		// Lit while the audio is playing.
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

	// Exam uses the other body of the switcher: progress and mistakes only.
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

	// The mistake message replaces the foot instead of pushing the content around.
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

	// One report gives everything below (steps, sub-steps, statuses). Only built on change.
	const FFXR_SessionReport Snapshot = Current->BuildReport();
	const int32 Open = Snapshot.Steps.IndexOfByPredicate(
		[](const FFXR_StepRecord& Step) { return Step.Status == EFXR_StepStatus::Active; });

	// Heading = the open step's instruction.
	if (StepTitleLabel)
	{
		StepTitleLabel->SetText(!Snapshot.Steps.IsValidIndex(Open)
			? FText::GetEmpty()
			: (Snapshot.Steps[Open].Instruction.IsEmpty()
				? FText::FromName(Snapshot.Steps[Open].StepId)
				: Snapshot.Steps[Open].Instruction));
	}

	// One segment per step; built once per run.
	if (MeterSegments.Num() != Snapshot.Steps.Num())
	{
		BuildMeter(Snapshot.Steps.Num());
	}

	RefreshMeter(Snapshot.Steps);

	// Rows are rebuilt when the open step changes; ticking off a sub-step only restyles them.
	const TArray<FFXR_SubStepRecord> Lines = Snapshot.Steps.IsValidIndex(Open)
		? Snapshot.Steps[Open].SubSteps
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

	// Training has nothing to grade, so a finished run shows a full green ring with a tick.
	const bool bTrainingDone = bPassed && Report.Mode == EFXR_DeliveryMode::Training;

	// The ring is always the score; its colour shows pass/fail.
	const FLinearColor Verdict = bFailed
		? FXR_PanelStyle::Red()
		: (bPassed ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text3());

	// Plain red text is hard to read on the panel, so text uses a lighter red.
	const FLinearColor VerdictText = bFailed
		? FXR_PanelStyle::Danger()
		: (bPassed ? FXR_PanelStyle::Green() : FXR_PanelStyle::Text3());

	if (ScoreRing)
	{
		// Out of 100, even if Scoring Weights uses a different starting score.
		ScoreRing->SetValue(bTrainingDone ? 1.f : FMath::Clamp(Report.Score / 100.f, 0.f, 1.f));
		ScoreRing->SetSliderProgressColor(Verdict);
	}

	if (ScoreLabel)
	{
		// A stopped run shows a dash, not 0.
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

	// Only failures get a reason line. Name the sub-step if the failure happened in one.
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

// -----------------------------------------------------------------------------------------------
// Meter and rows
// -----------------------------------------------------------------------------------------------

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
		// Segment height comes from the brush (SetDesiredSizeOverride doesn't stick on widgets built in
		// code, see FXR_PanelStyle::Band). The bound widget is the box, so collapsing it hides the meter.
		UImage* Segment = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		Segment->SetBrush(FXR_PanelStyle::Band(FXR_PanelStyle::Track(), FXR_PanelStyle::MeterH));

		if (UHorizontalBoxSlot* BoxSlot = Cast<UHorizontalBoxSlot>(StepMeter->AddChild(Segment)))
		{
			BoxSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			BoxSlot->SetHorizontalAlignment(HAlign_Fill);
			BoxSlot->SetVerticalAlignment(VAlign_Fill);

			// Gap only between segments, not after the last one.
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

		// 3 px divider between rows. Thinner lines shimmer in VR without temporal AA.
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

		// SizeBox gives the disc its size; the pill brush makes it a circle.
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
	// No sub-steps: hide the list entirely, otherwise the empty card still draws an outline.
	SetShown(StepScroll, !Lines.IsEmpty(), ESlateVisibility::Visible);

	UWidget* ScrollTo = nullptr;

	for (int32 Index = 0; Index < Rows.Num() && Lines.IsValidIndex(Index); ++Index)
	{
		const EFXR_StepStatus Status = Lines[Index].Status;
		const bool bOpen = Status == EFXR_StepStatus::Active;
		const bool bDone = Status == EFXR_StepStatus::Complete;

		const FFXR_PanelStepRow& Entry = Rows[Index];

		if (Entry.Row)
		{
			// Open row is tinted instead of outlined.
			Entry.Row->SetBrush(FXR_PanelStyle::Box(
				bOpen ? FXR_PanelStyle::ActiveRow() : FLinearColor::Transparent, 0.f));
			Entry.Row->SetPadding(bOpen
				? FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY + 14.f)
				: FMargin(FXR_PanelStyle::RowPadX, FXR_PanelStyle::RowPadY));
		}

		if (Entry.Numeral)
		{
			// Disc for open/done, ring for pending. No check mark glyph in Montserrat, so done rows keep
			// their number on a green disc.
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

			// Only the open row wraps to show its full text; the rest are one line with an ellipsis.
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

	// Centre the open row. Just "bring into view" leaves it at the bottom edge under the fade.
	if (ScrollTo && StepScroll)
	{
		StepScroll->ScrollWidgetIntoView(ScrollTo, true, EDescendantScrollDestination::Center);
	}
}

// -----------------------------------------------------------------------------------------------
// The clock
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingPanel::StartClock()
{
	UWorld* World = GetWorld();
	if (!World || !ElapsedLabel || ClockTimer.IsValid())
	{
		return;
	}

	// The only thing on the panel that ticks. Everything else updates on change.
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

// -----------------------------------------------------------------------------------------------

FText UFXR_TrainingPanel::BuildSummaryText() const
{
	return FText::Format(
		NSLOCTEXT("FlexXR", "PanelSummary", "{0} of {1} steps     {2} mistakes     {3}"),
		FText::AsNumber(Report.StepsCompleted),
		FText::AsNumber(Report.StepsTotal),
		FText::AsNumber(Report.Mistakes.Num()),
		FXR_PanelStyle::Clock(Report.DurationSeconds));
}

