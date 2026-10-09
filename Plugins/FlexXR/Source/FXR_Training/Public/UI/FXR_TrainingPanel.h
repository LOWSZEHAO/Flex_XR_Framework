// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "Blueprint/UserWidget.h"
#include "CoreMinimal.h"
#include "Runtime/FXR_SessionReport.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_TrainingPanel.generated.h"

class UFXR_TrainingSession;

/**
 * UFXR_TrainingPanel — the in-world surface that says what to do and what went wrong.
 *
 * Reparent a Widget Blueprint to this and it is wired: it finds the session, follows the run, and
 * offers everything a panel has to show as bindable getters. No event graph is needed to make it
 * work, and the events are there for whatever should animate.
 *
 * Put it in an FXR_Panel to have it in the world, or a screen-space widget to have it on a flat
 * build. It is ordinary UMG and does not care which.
 *
 * It owns the one piece of presentation a step graph cannot express: **a complaint has to leave by
 * itself.** The runner records a mistake as a fact at a moment in time, which is right for a report
 * and useless on a wall. Left alone, the first thing a trainee got wrong would stay up for the rest
 * of the session, and a panel that always says something is wrong says nothing.
 *
 * It reads the session and never drives it. Starting a run, stopping one and scoring it are the
 * session's own calls, so a panel can be dropped from a level without changing what the procedure
 * does or what it is worth.
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

protected:
	/** How long a complaint stays up. Zero leaves it up until the next one; see the class comment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float MistakeDwellSeconds = 4.f;

	/** Shown when a wrong action was authored without a message of its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training")
	FText DefaultMistakeMessage;

private:
	UFUNCTION()
	void HandleStepActivated(FName InStepId, FText InInstruction);

	UFUNCTION()
	void HandleMistake(FFXR_Mistake Mistake);

	UFUNCTION()
	void HandleFinished(FFXR_SessionReport InReport);

	void ClearMistake();

	/** The only session in the level, or null. Warns rather than picking when there are several. */
	UFXR_TrainingSession* FindSessionInWorld() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<UFXR_TrainingSession> Session;

	FTimerHandle MistakeTimer;

	FText Instruction;
	FText MistakeText;
	FFXR_SessionReport Report;
	FName StepId;
	bool bFinished = false;
};
