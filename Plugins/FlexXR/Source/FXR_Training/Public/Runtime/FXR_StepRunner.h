// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Events/FXR_InteractionEvent.h"
#include "Types/FXR_TrainingTypes.h"

/**
 * One way out of a step. A step completes when any of its transitions fires, and the transition
 * that fired decides what opens next.
 *
 * This single shape covers both things a real procedure needs. Several next steps on one transition
 * is a fan-out, which is how "gloves and goggles, either order" is expressed. Several transitions is
 * a branch, which is how "the wrong extinguisher on an electrical fire" leads somewhere else.
 */
struct FFXR_CompiledTransition
{
	/** The interaction that fires this transition. */
	FName InteractionId;

	/** Which phase of that interaction counts. */
	EFXR_InteractionPhase Phase = EFXR_InteractionPhase::Ended;

	/** Steps to open. Resolved to indices at compile time; empty means this route ends the run. */
	TArray<int32> NextSteps;
};

/** Something the author called out as wrong while this step is open. */
struct FFXR_CompiledWrongAction
{
	FName InteractionId;
	FText Message;
};

/**
 * A step in the form the runner consumes. Names are resolved to indices here and never looked up
 * again, which is what keeps event handling free of hashing and allocation.
 *
 * ADR-004 makes this the stable contract: authoring front-ends (the DataAsset, a CSV import, a
 * visual editor later) all compile down to an array of these, and the runner never learns which one
 * produced it.
 */
struct FFXR_CompiledStep
{
	FName StepId;

	/** Any one of these completes the step. */
	TArray<FFXR_CompiledTransition> Transitions;

	TArray<FFXR_CompiledWrongAction> WrongActions;

	/** Seconds before the hint escalates. Zero or less means the step never times out. */
	float TimeoutSeconds = 0.f;

	/** Opt-in interlock for legally mandated gating. The exception, never the philosophy. */
	bool bHardLock = false;
};

/**
 * FFXR_StepRunner — the judge.
 *
 * Deliberately free of UObject, world and tick group: it is fed events and a delta time, and it
 * answers with outcomes. That is what makes it unit-testable without an engine running and what
 * makes a session reproducible, which replay depends on (ADR-004).
 *
 * It never touches the world. It cannot disable an interactable, open a door, or stop a trainee
 * doing the wrong thing, because a procedure the trainee cannot fail measures nothing.
 */
struct FXR_TRAINING_API FFXR_StepRunner
{
	/** Begin a run. Steps are moved in; the entry step is the first thing opened. */
	void Start(TArray<FFXR_CompiledStep>&& InSteps, int32 EntryIndex, EFXR_DeliveryMode InMode);

	/** End the run early. Active steps are left as they are, for the report. */
	void Stop();

	/** Offer an interaction to the run. Safe to call when nothing is running. */
	void HandleEvent(const FFXR_InteractionEvent& Event);

	/** Advance timeouts. Safe to call when nothing is running. */
	void Tick(float DeltaSeconds);

	bool IsRunning() const { return bRunning; }
	EFXR_DeliveryMode GetMode() const { return Mode; }
	float GetElapsedSeconds() const { return Elapsed; }

	const TArray<FFXR_CompiledStep>& GetSteps() const { return Steps; }
	const TArray<int32>& GetActiveSteps() const { return ActiveSteps; }
	const TArray<FFXR_Mistake>& GetMistakes() const { return Mistakes; }

	EFXR_StepStatus GetStatus(int32 StepIndex) const;

	/** Hints are suppressed entirely in Exam, and held back until a mistake in Practice. */
	bool ShouldShowHints() const;

	//~ Observers. Set once when the session starts; never reassigned per frame.
	TFunction<void(int32 /*StepIndex*/)> OnStepActivated;
	TFunction<void(int32 /*StepIndex*/)> OnStepCompleted;
	TFunction<void(const FFXR_Mistake&)> OnMistake;
	TFunction<void(int32 /*StepIndex*/, int32 /*HintLevel*/)> OnHintEscalated;
	TFunction<void()> OnFinished;

private:
	void Activate(int32 StepIndex);
	void Complete(int32 StepIndex, const FFXR_CompiledTransition& Via);
	void RecordMistake(FName StepId, FName InteractionId, EFXR_MistakeKind Kind, const FText& Message);
	bool Matches(const FFXR_CompiledTransition& Transition, const FFXR_InteractionEvent& Event) const;

	TArray<FFXR_CompiledStep> Steps;
	TArray<EFXR_StepStatus> Status;

	/** Seconds each step has been open, parallel to ActiveSteps. */
	TArray<float> ActiveElapsed;

	/** How many times each active step has escalated, parallel to ActiveSteps. */
	TArray<int32> ActiveHintLevel;

	TArray<int32> ActiveSteps;
	TArray<FFXR_Mistake> Mistakes;

	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Guided;
	float Elapsed = 0.f;
	bool bRunning = false;

	/** Practice mode opens up once the trainee has got something wrong. */
	bool bAnyMistakeYet = false;
};
