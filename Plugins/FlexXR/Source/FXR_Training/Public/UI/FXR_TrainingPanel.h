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
	/** Nothing has happened yet. Names the procedure, states the mode, offers to begin. */
	Start,

	/** A briefing, before or between steps. Author-driven; the runner knows nothing about it. */
	Explanation,

	/** The run. The step list, the open step, and whatever just went wrong. */
	Procedure,

	/** The report. */
	Complete
};

/** What the explanation's audio is doing, which is all the panel needs to know about it. */
UENUM(BlueprintType)
enum class EFXR_ExplainState : uint8
{
	/** Not played. The foot offers to play it. */
	Idle,

	/** Playing. The foot is a transport: pause, a thin track, elapsed time. */
	Playing,

	/** Heard, or read without audio. The foot offers to move on. */
	Read
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_PanelRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FFXR_PanelAudioRequest, bool, bWantPlaying);

/**
 * The widgets of one step row, kept so that a step change restyles the list instead of rebuilding
 * it. The row count is fixed once a graph is compiled, so there is nothing to rebuild after the
 * first time, and the alternative churns three widgets per step per change for no reason.
 */
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
 * UFXR_TrainingPanel — the in-world surface that says what to do and what went wrong.
 *
 * Reparent a Widget Blueprint to this and it is wired: it finds the session, follows the run, and
 * drives every widget it recognises by name. No event graph is needed to make it work, and the
 * events are there for whatever should animate.
 *
 *
 * FOUR SCREENS, ONE ASSET
 *
 * Start, Explanation, Procedure and Complete all live in the same Widget Blueprint, as children of
 * one Widget Switcher, and this class decides which is up. One asset rather than four, because the
 * four share a surface, a bar, a type scale and a foot - and four assets would mean four copies of
 * all of it, drifting apart the first time one is edited.
 *
 * **The screen is derived, not set.** There is no state machine to keep in step with the session:
 * a run that is finished shows the report, a run that is going shows the procedure, and anything
 * else shows Start. An explanation is the one exception, because it is the one thing the runner has
 * no opinion about - call ShowExplanation and it takes over until it is dismissed.
 *
 *
 * IT READS THE SESSION
 *
 * Everything shown here is read from the session; the panel computes nothing about the procedure
 * and decides nothing about what a run is worth. The one place it reaches back is deliberate and
 * switchable: Begin and Try again call StartSession while `Buttons Drive Session` is set, which is
 * what makes a dropped-in panel work on its own (and the framework's rule is that a component
 * dropped in a level works). Clear the flag and the buttons only broadcast, leaving the flow to
 * whoever owns it. The request delegates fire either way.
 *
 *
 * ONE PIECE OF PRESENTATION THE GRAPH CANNOT EXPRESS
 *
 * **A complaint has to leave by itself.** The runner records a mistake as a fact at a moment in
 * time, which is right for a report and useless on a wall: left alone, the first thing a trainee
 * got wrong would stay up for the rest of the session, and a panel that always says something is
 * wrong says nothing.
 */
UCLASS(Abstract, ClassGroup = (FlexXR))
class FXR_TRAINING_API UFXR_TrainingPanel : public UUserWidget
{
	GENERATED_BODY()

public:
	UFXR_TrainingPanel(const FObjectInitializer& ObjectInitializer);

	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/**
	 * Follow this session, releasing whatever it was following. Null just unbinds.
	 *
	 * Only needed for a level with more than one session running — a panel left alone finds the
	 * single one by itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void BindToSession(UFXR_TrainingSession* InSession);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	UFXR_TrainingSession* GetSession() const { return Session.Get(); }

	/** Which screen is up. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	EFXR_PanelScreen GetScreen() const { return Screen; }

	/**
	 * Put up a briefing, and keep it up until it is dismissed.
	 *
	 * The step graph has no explanation of its own, on purpose: the runner judges actions, and a
	 * briefing is not an action. This is the seam for whoever does own that content — a cutscene
	 * beat, a chapter intro, a step's own preamble — and it costs the runner nothing.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void ShowExplanation(const FText& Body);

	/**
	 * Reflect what the audio is doing. The panel does not play it: whatever owns the sound owns the
	 * clock, and a panel that also played audio would be two sources of truth for one timeline.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void SetExplanationAudio(EFXR_ExplainState State, float ElapsedSeconds, float DurationSeconds);

	/** Take the briefing down. The screen falls back to whatever the session is doing. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training")
	void DismissExplanation();

	/** What to do now. Empty before the run starts and after it ends. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetInstruction() const { return Instruction; }

	/** The open step's id, for a panel that maps steps to its own art. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FName GetStepId() const { return StepId; }

	/** Formatted as "2 / 7". Empty while nothing is running. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetProgressText() const;

	/** The same thing as 0 to 1, for a progress bar. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	float GetProgressFraction() const;

	/** The current complaint, or empty. It clears itself after Mistake Dwell Seconds. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FText GetMistake() const { return MistakeText; }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool HasMistake() const { return !MistakeText.IsEmpty(); }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	bool IsFinished() const { return bFinished; }

	/** The run's report, filled in once it finishes. Score, mistakes, per-step timings. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Training")
	FFXR_SessionReport GetReport() const { return Report; }

	//~ Requests the panel makes of whoever owns the flow. They fire whether or not
	//~ Buttons Drive Session is set, so a project can listen without giving up the default.

	/** Begin was pressed on the Start screen. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnBeginRequested;

	/** Done, or Try again, was pressed on the report. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnRetryRequested;

	/** The trainee said they had read the briefing. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelRequest OnExplanationConfirmed;

	/** Play or pause was pressed on a briefing. True wants it playing. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Training")
	FFXR_PanelAudioRequest OnExplanationAudioToggled;

	/** A step opened. Step Number is 1-based, for showing. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnStepChanged(FName InStepId, const FText& InInstruction, int32 StepNumber, int32 StepTotal);

	/** Something went wrong. The message is the author's, or a fallback if they wrote none. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnMistakeShown(const FFXR_Mistake& Mistake, const FText& Message);

	/** The complaint's time is up. Hide whatever OnMistakeShown put on screen. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnMistakeCleared();

	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnSessionComplete(const FFXR_SessionReport& InReport);

	/** The panel changed screens. For whatever should animate the transition. */
	UFUNCTION(BlueprintImplementableEvent, Category = "FlexXR|Training")
	void OnScreenChanged(EFXR_PanelScreen NewScreen);

protected:
	/**
	 * Widgets this class drives directly, found by name.
	 *
	 * Name a widget in your Blueprint after one of these and it is filled in and kept current with
	 * no event graph at all, which is what keeps framework logic out of Blueprint. Leave any of
	 * them out and that part simply does not exist: **Optional** rather than required, because this
	 * is a base class for layouts nobody has designed yet, and one that refuses to compile without
	 * a widget named `ResultHintsValue` would be a base class people work around.
	 *
	 * Prefer these to property bindings. A bound getter runs every frame per widget whether
	 * anything changed or not; these are written when something does.
	 */

	//~ Chrome, above the switcher and shared by the screens that have a bar.

	/** The four screens, in EFXR_PanelScreen order. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ScreenSwitcher;

	/** Collapsed on the screens that have no bar, which is Explanation and Complete. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> TopBar;

	/** "Fire safety". Collapsed, with its separator, when the graph names no course. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CrumbRootLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CrumbSeparator;

	/** The procedure's own name. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CrumbLeafLabel;

	/** Green while a run is going, dim otherwise. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UImage> StatusPip;

	/** "2 / 5" during a run, "Not started" before one. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StatusLabel;

	/** Filled with one segment per step in the compiled graph. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StepMeter;

	//~ Start.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartTitleLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartStepsValue;

	/** The mode is stated, never offered. A trainee who can pick Guided is not sitting an exam. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StartModeValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> BeginButton;

	//~ Explanation.

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ExplanationLabel;

	/** Lit while the audio plays, quiet once it has been heard. */
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

	/** The open step's short label — the heading. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StepTitleLabel;

	/**
	 * The open step's full instruction.
	 *
	 * The generated panel does not use this: it puts the instruction in the open row, where the
	 * step's place in the procedure is visible at the same time. Kept because a simpler layout
	 * that wants the instruction as its headline is a reasonable thing to build.
	 */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> InstructionLabel;

	/** "Step 2 / 7", for a layout that wants it spelled out rather than as a bar. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ProgressLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> ProgressFill;

	/** Filled with one row per step. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> StepList;

	/** Scrolled so the open step is centred, not merely on screen. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UScrollBox> StepScroll;

	/** The run's facts, or the complaint. Index 0 is normal, 1 is the mistake. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> FootSwitcher;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ModeLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ElapsedLabel;

	/** Shown only while there is a complaint, for a layout that toggles rather than switches. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> MistakeCard;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> MistakeLabel;

	//~ Complete.

	/** Shown only once the run has finished, for a layout that overlays rather than switches. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> CompletionCard;

	/** The score as an arc. Always the score, whatever the verdict — only its colour changes. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<URadialSlider> ScoreRing;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ScoreLabel;

	/** "Procedure complete" or "Procedure failed". Carries the verdict, with the ring's colour. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompleteEyebrowLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> CompleteTitleLabel;

	/** Why the run ended. Collapsed on a pass: a pass needs no excuse. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ReasonLabel;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultStepsValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultMistakesValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultHintsValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> ResultTimeValue;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UButton> DoneButton;

	/** "Done" after a pass, "Try again" after a failure. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> DoneLabel;

	/** Steps, mistakes and elapsed time on one line, for a layout without the definition list. */
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> SummaryLabel;

	//~ Settings.

	/** How long a complaint stays up. Zero leaves it up until the next one; see the class comment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float MistakeDwellSeconds = 4.f;

	/** Shown when a wrong action was authored without a message of its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	FText DefaultMistakeMessage;

	/**
	 * Let Begin and Try again start the session themselves.
	 *
	 * On, so that a panel and a session dropped into a level work with nothing wired. Clear it when
	 * something else owns the flow — a menu, a chapter manager, a cutscene — and listen for
	 * On Begin Requested instead. The delegates fire either way.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	bool bButtonsDriveSession = true;

private:
	UFUNCTION()
	void HandleStepActivated(FName InStepId, FText InInstruction);

	UFUNCTION()
	void HandleMistake(FFXR_Mistake Mistake);

	UFUNCTION()
	void HandleFinished(FFXR_SessionReport InReport);

	//~ Button handlers. UFUNCTION because that is what OnClicked binds to.

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

	/** Push the current state into whichever of the bound widgets exist. */
	void Refresh();

	//~ Refresh, split by screen so each one reads as the thing it draws.
	void RefreshChrome();
	void RefreshStart();
	void RefreshExplanation();
	void RefreshProcedure();
	void RefreshComplete();

	/** What the session's state says should be up. */
	EFXR_PanelScreen ResolveScreen() const;

	/** Move the switcher, collapse the chrome that does not belong, and tell Blueprint. */
	void ApplyScreen(EFXR_PanelScreen NewScreen);

	/** One meter segment and one list row per compiled step. Called when a run starts. */
	void BuildStepWidgets();

	/** Restyle the rows and segments for the current statuses. Called whenever that changes. */
	void RefreshStepWidgets();

	/** Hook the buttons up once, at construction. */
	void BindButtons();

	/** The elapsed clock in the foot, which is the one thing on the panel that must tick. */
	void StartClock();
	void StopClock();

	UFUNCTION()
	void TickClock();

	/** Steps, mistakes and elapsed time for the one-line summary. */
	FText BuildSummaryText() const;

	/** The only session in the level, or null. Warns rather than picking when there are several. */
	UFXR_TrainingSession* FindSessionInWorld() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UFXR_TrainingSession> Session;

	/** Rows, parallel to the compiled step array. */
	UPROPERTY(Transient)
	TArray<FFXR_PanelStepRow> Rows;

	/** Meter segments, parallel to the same array. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> MeterSegments;

	FTimerHandle MistakeTimer;
	FTimerHandle ClockTimer;

	FText Instruction;
	FText MistakeText;

	/** Non-empty means a briefing is up and overrides the derived screen. */
	FText ExplanationBody;

	FFXR_SessionReport Report;
	FName StepId;
	EFXR_PanelScreen Screen = EFXR_PanelScreen::Start;
	EFXR_ExplainState ExplainState = EFXR_ExplainState::Idle;
	bool bFinished = false;
};
