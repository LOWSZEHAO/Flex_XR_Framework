// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Runtime/FXR_SessionReport.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_TrainingPanel.generated.h"

class UBorder;
class UButton;
class UFXR_TrainingSession;
class UImage;
class UPanelWidget;
class UProgressBar;
class URadialSlider;
class UScrollBox;
class UTextBlock;
class UWidget;
class UWidgetSwitcher;

/** Which of the panel's four screens is up. */
UENUM(BlueprintType)
enum class EFXR_PanelScreen : uint8
{
	/** Before the run: procedure name, mode, Begin button. */
	Start,

	/** A briefing shown before or between steps. Driven by the project, not the runner. */
	Explanation,

	/** During the run: current step, its sub-steps, mistakes. */
	Procedure,

	/** The report. */
	Complete
};

/** State of the briefing's audio. The panel doesn't play it, it just shows it. */
UENUM(BlueprintType)
enum class EFXR_ExplainState : uint8
{
	/** Not played yet. Shows the play button. */
	Idle,

	/** Playing. Shows pause, a progress bar and the time. */
	Playing,

	/** Heard (or read without audio). Shows the continue button. */
	Read
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_PanelRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_PanelAudioRequest, bool, bWantPlaying);

/** Widgets of one list row, kept so a status change restyles the row instead of rebuilding it. */
USTRUCT()
struct FFXR_PanelStepRow
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<UBorder> Row;

	UPROPERTY()
	TObjectPtr<UBorder> Numeral;

	UPROPERTY()
	TObjectPtr<UTextBlock> NumeralLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> TextLabel;

	UPROPERTY()
	TObjectPtr<UTextBlock> ChevronLabel;
};

/**
 * UFXR_TrainingPanel - base class for the in-world training panel.
 *
 * Reparent a Widget Blueprint to this. It finds the session, follows the run and fills in any
 * widget whose name matches one of the BindWidgetOptional properties below, so it works without
 * an event graph. The BlueprintImplementableEvents are there for animation.
 *
 * All four screens (Start, Explanation, Procedure, Complete) live in one Widget Blueprint under a
 * Widget Switcher, so they share the same frame, bar and styles. The screen is worked out from the
 * session state rather than tracked separately: finished -> Complete, running -> Procedure,
 * otherwise Start. ShowExplanation overrides that until it's dismissed.
 *
 * The panel only reads the session. The exception is Begin / Try again, which call StartSession
 * while Buttons Drive Session is on, so a panel dropped into a level works by itself.
 *
 * Mistake messages clear themselves after Mistake Dwell Seconds; otherwise the first mistake
 * would stay on screen for the rest of the run.
 */
UCLASS(Abstract, ClassGroup = (FlexXR))
class FXR_TRAINING_API UFXR_TrainingPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFXR_TrainingPanel(const FObjectInitializer& ObjectInitializer);

	/** Designer only: fills the screens with sample content (see FillDesignPreview). */
	virtual void NativePreConstruct() override;

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** Follow this session instead of the one it found. Null unbinds. Only needed with several sessions. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void BindToSession(UFXR_TrainingSession* InSession);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	UFXR_TrainingSession* GetSession() const { return Session.Get(); }

	/** Which screen is up. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	EFXR_PanelScreen GetScreen() const { return Screen; }

	/**
	 * Show a briefing until it's dismissed. The runner doesn't know about briefings; this is for
	 * whatever owns that content (an intro, a cutscene beat, a step preamble).
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void ShowExplanation(const FText& Body);

	/** Mirror the briefing audio's state. The panel doesn't play audio itself. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void SetExplanationAudio(EFXR_ExplainState State, float ElapsedSeconds, float DurationSeconds);

	/** Hide the briefing. The screen goes back to whatever the session is doing. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void DismissExplanation();

	/** The current instruction. Empty outside a run. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetInstruction() const { return Instruction; }

	/** The open step's id. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FName GetStepId() const { return StepId; }

	/** "2 / 7". Empty when nothing is running. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetProgressText() const;

	/** Same as a 0..1 fraction, for a progress bar. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	float GetProgressFraction() const;

	/** The current mistake message, or empty. Clears itself after Mistake Dwell Seconds. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetMistake() const { return MistakeText; }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool HasMistake() const { return !MistakeText.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool IsFinished() const { return bFinished; }

	/** The finished run's report. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FFXR_SessionReport GetReport() const { return Report; }

	//~ Requests from the panel's buttons. They fire whether or not Buttons Drive Session is on.

	/** Begin was pressed on the Start screen. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnBeginRequested;

	/** Done, or Try again, was pressed on the report. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnRetryRequested;

	/** The trainee confirmed the briefing. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnExplanationConfirmed;

	/** Play or pause pressed on the briefing. True = play. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelAudioRequest OnExplanationAudioToggled;

	/** A step opened. Step Number is 1-based. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnStepChanged(FName InStepId, const FText& InInstruction, int32 StepNumber, int32 StepTotal);

	/** A mistake happened. Message is the mistake's text, or the default message. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnMistakeShown(const FFXR_Mistake& Mistake, const FText& Message);

	/** The mistake message timed out. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnMistakeCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnSessionComplete(const FFXR_SessionReport& InReport);

	/** The panel switched screens. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnScreenChanged(EFXR_PanelScreen NewScreen);

protected:
	/**
	 * Widgets the panel drives directly, matched by name.
	 *
	 * Name a widget in the Blueprint after one of these and it gets filled in and updated, no event
	 * graph needed. They're all optional so a different layout can leave any of them out.
	 *
	 * These are written only when something changes, which is cheaper than property bindings that
	 * run every frame.
	 */

	//~ Top bar, shared by the screens that have it.

	/** The four screens, in EFXR_PanelScreen order. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ScreenSwitcher;

	/** Hidden on Explanation and Complete. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TopBar;

	/** Course name, e.g. "Fire safety". Hidden (with its separator) when there's no course. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CrumbRootLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CrumbSeparator;

	/** Procedure name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CrumbLeafLabel;

	/** Green while running, grey otherwise. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StatusPip;

	/** "2 / 5" while running, "Not started" before. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusLabel;

	/** Filled with one segment per step. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StepMeter;

	//~ Start.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartTitleLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartStepsValue;

	/** Shows the mode. The trainee can't change it here. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartModeValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BeginButton;

	//~ Explanation.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExplanationLabel;

	/** Lit while the audio plays. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> ExplainPip;

	/** Idle / Playing / Read, in EFXR_ExplainState order. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ExplainFootSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExplainPlayButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExplainPauseButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> ExplainConfirmButton;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ExplainAudioBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExplainTimeLabel;

	//~ Procedure.

	/**
	 * Procedure body. Index 0 is the Training layout (heading + rows), index 1 the Exam layout
	 * (progress and mistake count only, since showing the steps in an exam gives away the answers).
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ProcedureBodySwitcher;

	/** Exam: steps done out of total, "2 / 5". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExamProgressLabel;

	/** Exam: mistake count, "1 of 3" when there's a limit. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExamMistakesValue;

	/** Heading: the open step's instruction. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StepTitleLabel;

	/** The open step's instruction. Not used by the generated panel (the heading already shows it). */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InstructionLabel;

	/** "Step 2 / 7", for a layout that wants text instead of the meter. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressFill;

	/**
	 * Filled with one row per sub-step of the open step. A step without sub-steps has no rows and the
	 * scroll box is collapsed.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StepList;

	/** Scrolled so the open row is centred. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StepScroll;

	/** Index 0: mode and clock. Index 1: the mistake message. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> FootSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ElapsedLabel;

	/** Shown only while there's a mistake message, for layouts that toggle instead of switch. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MistakeCard;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MistakeLabel;

	//~ Complete.

	/** Shown only after the run, for layouts that overlay instead of switch. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CompletionCard;

	/** Score ring. In Exam it shows the score (colour = pass/fail). After Training it's full with a tick. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URadialSlider> ScoreRing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreLabel;

	/** Shown inside the ring instead of the score after a Training run. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ScoreTick;

	/** "Procedure complete" / "Procedure failed". */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompleteEyebrowLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompleteTitleLabel;

	/** Why the run failed. Hidden otherwise. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultStepsValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultMistakesValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultTimeValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DoneButton;

	/** "Done" after a pass, "Try again" otherwise. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DoneLabel;

	/** Steps, mistakes and time on one line, for layouts without the result rows. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SummaryLabel;

	//~ Settings.

	/** How long a mistake message stays up. 0 = until the next one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float MistakeDwellSeconds = 4.f;

	/** Shown when a mistake has no message of its own (which is currently always). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	FText DefaultMistakeMessage;


	/**
	 * Let Begin / Try again start the session. On by default so a panel and a session dropped into a
	 * level just work. Turn it off if something else controls the flow and listen to On Begin
	 * Requested instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bButtonsDriveSession = true;

#if WITH_EDITORONLY_DATA
	/** Preview the report as a failed run. It's the taller version, so it's the one to check for fit. Editor only. */
	UPROPERTY(EditAnywhere, Category = "FlexXR|Preview")
	bool bPreviewFailedRun = false;

	/** Preview the report as a finished Training run (full ring + tick). Editor only. */
	UPROPERTY(EditAnywhere, Category = "FlexXR|Preview")
	bool bPreviewTrainingRun = false;

	/** Preview the sample step with three sub-steps; off shows a step without any. Editor only. */
	UPROPERTY(EditAnywhere, Category = "FlexXR|Preview")
	bool bPreviewSubSteps = true;
#endif

private:
	UFUNCTION()
	void HandleStepActivated(FName InStepId, FText InInstruction);

	UFUNCTION()
	void HandleSubStepActivated(FName InStepId, FName InSubStepId);

	UFUNCTION()
	void HandleMistake(FFXR_Mistake Mistake);

	UFUNCTION()
	void HandleFinished(FFXR_SessionReport InReport);

	//~ Button handlers (UFUNCTION so OnClicked can bind them).

	UFUNCTION()
	void HandleBeginClicked();

	UFUNCTION()
	void HandleDoneClicked();

	UFUNCTION()
	void HandleExplainPlayClicked();

	UFUNCTION()
	void HandleExplainPauseClicked();

	UFUNCTION()
	void HandleExplainConfirmClicked();

	void ClearMistake();

	/** Push the current state into whichever bound widgets exist. */
	void Refresh();

	//~ One refresh per screen.
	void RefreshChrome();
	void RefreshStart();
	void RefreshExplanation();
	void RefreshProcedure();
	void RefreshComplete();

	/** Which screen the session state calls for. */
	EFXR_PanelScreen ResolveScreen() const;

	/** Switch screens, hide the chrome that doesn't belong, and notify Blueprint. */
	void ApplyScreen(EFXR_PanelScreen NewScreen);

	//~ The meter and rows take data instead of reading the session, so the designer preview can
	//~ use the same code with sample data.

	/** One meter segment per step. Called when a run starts. */
	void BuildMeter(int32 StepCount);

	/** Colour the segments for these statuses. */
	void RefreshMeter(const TArray<FFXR_StepRecord>& Steps);

	/** One row per sub-step of the open step. Called when the open step changes. */
	void BuildRows(const TArray<FFXR_SubStepRecord>& Lines);

	/** Restyle the rows for these statuses; collapse the list if there are none. */
	void RefreshRows(const TArray<FFXR_SubStepRecord>& Lines);

	/**
	 * Fill the screen the designer is showing with sample content and hide chrome that screen doesn't
	 * have. Without this the designer showed an empty list and a top bar on screens that don't have
	 * one, which made the layout look wrong when it wasn't.
	 */
	void FillDesignPreview();

	/** Bind the buttons once, on construct. */
	void BindButtons();

	/** The elapsed-time clock, the only thing on the panel that ticks. */
	void StartClock();
	void StopClock();

	UFUNCTION()
	void TickClock();

	/** Steps, mistakes and time for the one-line summary. */
	FText BuildSummaryText() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UFXR_TrainingSession> Session;

	/** Rows for the current page, parallel to its lines. */
	UPROPERTY(Transient)
	TArray<FFXR_PanelStepRow> Rows;

	/** Meter segments, one per step. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MeterSegments;

	FTimerHandle MistakeTimer;
	FTimerHandle ClockTimer;

	FText Instruction;
	FText MistakeText;

	/** Non-empty while a briefing is showing; overrides the derived screen. */
	FText ExplanationBody;

	FFXR_SessionReport Report;
	FName StepId;

	/** The step the rows were built for, or INDEX_NONE. */
	int32 PageStep = INDEX_NONE;
	EFXR_PanelScreen Screen = EFXR_PanelScreen::Start;
	EFXR_ExplainState ExplainState = EFXR_ExplainState::Idle;
	bool bFinished = false;
};
