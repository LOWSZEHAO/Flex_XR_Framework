// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Events/FXR_InteractionEvent.h"
#include "Types/FXR_TrainingTypes.h"

/**
 * A step in the form the runner consumes, compiled from the Steps asset and the GM.
 *
 * ADR-004 makes this the stable contract: whatever authors a procedure compiles down to an array of
 * these, and the runner never learns which assets produced it. Note what is absent - nothing here
 * says how a step finishes. The project decides that, by calling Complete Step from whatever event
 * means "done", so the runner never has to know what a latch or a socket is.
 */
struct FFXR_CompiledStep
{
	FName StepId;

	/** The objects this step is about. Never wrong while it is open; out of order before it. */
	TArray<FName> Objects;

	/** Exam only: open longer than this and the run fails. Zero or less is no limit. */
	float TimeoutSeconds = 0.f;

	/** Exam only: any mistake while this step is open fails the run. */
	bool bCritical = false;
};

/**
 * FFXR_StepRunner — the judge.
 *
 * Deliberately free of UObject, world and tick group: it is fed events, completions and a delta
 * time, and it answers with outcomes. That is what makes it testable without an engine running and
 * a session reproducible, which replay depends on (ADR-004).
 *
 * Steps run in order. It never changes the world - the GM's events do that - and it never decides
 * a step is done; Complete Step does. What it does decide is what every touch of an exposed object
 * meant, and in Exam, whether the run has failed.
 */
struct FXR_TRAINING_API FFXR_StepRunner
{
	/** Begin a run at the first step. Steps are moved in. */
	void Start(TArray<FFXR_CompiledStep>&& InSteps, EFXR_DeliveryMode InMode, int32 InMaxMistakes = 0);

	/** End the run early. The open step is left as it is, for the report. */
	void Stop();

	/**
	 * Finish the open step and open the next, or end the run after the last.
	 *
	 * With a Step Id, only that step can be finished, and only while it is the one open - so an
	 * event that fires every time something is grabbed advances the procedure once, at the step it
	 * belongs to, and does nothing the rest of the time. With no Step Id, it finishes whatever is
	 * open. Returns whether a step was finished.
	 */
	bool CompleteStep(FName StepId);

	/** Judge an interaction. Safe to call when nothing is running. Never completes a step. */
	void HandleEvent(const FFXR_InteractionEvent& Event);

	/** Advance Exam's timeouts. Safe to call when nothing is running. */
	void Tick(float DeltaSeconds);

	bool IsRunning() const { return bRunning; }
	EFXR_DeliveryMode GetMode() const { return Mode; }
	float GetElapsedSeconds() const { return Elapsed; }

	const TArray<FFXR_CompiledStep>& GetSteps() const { return Steps; }
	const TArray<FFXR_Mistake>& GetMistakes() const { return Mistakes; }

	/** The open step, or INDEX_NONE before the run and after it. */
	int32 GetCurrentStep() const { return Current; }

	EFXR_StepStatus GetStatus(int32 StepIndex) const;

	//~ Per-step history, kept after a step closes so a report can be built from it.
	float GetStepOpenedAt(int32 StepIndex) const;
	float GetStepClosedAt(int32 StepIndex) const;
	int32 GetStepMistakeCount(int32 StepIndex) const;

	/** True when the last step was finished, rather than the run being stopped or failed. */
	bool ReachedEnd() const { return bReachedEnd; }

	/** True when the run ended against the trainee. Exam only. */
	bool Failed() const { return bFailed; }

	/** The step that ended a failed run, or INDEX_NONE. */
	int32 GetFailedStep() const { return FailedStep; }

	EFXR_FailReason GetFailReason() const { return FailReason; }

	/** The mistake limit this run is held to. Zero when there is none, which is every Training run. */
	int32 GetMaxMistakes() const { return MaxMistakes; }

	//~ Observers. Set once when the session starts; never reassigned per frame.
	TFunction<void(int32 /*StepIndex*/)> OnStepActivated;
	TFunction<void(int32 /*StepIndex*/)> OnStepCompleted;
	TFunction<void(const FFXR_Mistake&)> OnMistake;
	TFunction<void()> OnFinished;

private:
	void Activate(int32 StepIndex);
	void RecordMistake(FName InteractionId, EFXR_MistakeKind Kind, const FText& Message);
	void Fail(int32 StepIndex, EFXR_FailReason Reason);

	static bool UsesObject(const FFXR_CompiledStep& Step, FName InteractionId);

	TArray<FFXR_CompiledStep> Steps;
	TArray<EFXR_StepStatus> Status;

	//~ Parallel to Steps: a closed step keeps its record.
	TArray<float> OpenedAt;
	TArray<float> ClosedAt;
	TArray<int32> MistakeCounts;
	TArray<FFXR_Mistake> Mistakes;

	int32 Current = INDEX_NONE;
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;
	float Elapsed = 0.f;
	bool bRunning = false;
	bool bReachedEnd = false;
	bool bFailed = false;
	int32 FailedStep = INDEX_NONE;
	EFXR_FailReason FailReason = EFXR_FailReason::None;

	/** Exam's limit. Zero is no limit, and only an Exam run is ever given one. */
	int32 MaxMistakes = 0;
};
