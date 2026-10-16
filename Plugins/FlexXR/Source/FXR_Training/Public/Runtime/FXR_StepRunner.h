// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Events/FXR_InteractionEvent.h"
#include "Types/FXR_TrainingTypes.h"

/**
 * One thing the trainee completes, in the form the runner consumes: a sub-step, or a step that has
 * none. Compiled from the Steps asset and the GM.
 *
 * A step with three sub-steps arrives as three of these, in order, sharing the step's id and index.
 * The runner works down the list and never needs to know where one step ends and the next begins -
 * grouping entries back into steps is the session's job, for the panel and the report.
 *
 * ADR-004 makes this the stable contract: whatever authors a procedure compiles down to an array of
 * these, and the runner never learns which assets produced it. Note what is absent - nothing here
 * says how an entry finishes. The project decides that, by calling Complete Step from whatever
 * event means "done", so the runner never has to know what a latch or a socket is.
 */
struct FFXR_CompiledStep
{
	/** The step this is, or is part of. */
	FName StepId;

	/**
	 * The step's index in the Steps asset's list. Shared by all of its sub-steps, which is how the
	 * session groups entries back into steps: a change of index between neighbours is a new step.
	 */
	int32 StepIndex = INDEX_NONE;

	/** The sub-step this is. None when the entry is a whole step. */
	FName SubStepId;

	/** The sub-step's place in its step's Sub Steps list, or INDEX_NONE when the entry is a whole step. */
	int32 SubStepIndex = INDEX_NONE;

	/** The objects this entry is about. Never wrong while it is open; out of order before it. */
	TArray<FName> Objects;

	/** Exam only: open longer than this and the run fails. Zero or less is no limit. */
	float TimeoutSeconds = 0.f;

	/**
	 * Exam only: the whole step's limit, counted from when its first entry opened. Carried by every
	 * entry of a step with sub-steps; zero on a whole step, whose own limit is TimeoutSeconds.
	 */
	float StepTimeoutSeconds = 0.f;

	/** Exam only: any mistake while this entry is open fails the run. */
	bool bCritical = false;

	bool IsSubStep() const { return SubStepIndex != INDEX_NONE; }

	/** What Complete Step names to finish this: the sub-step's id, or the step's own when it has none. */
	FName GetCompletionId() const { return IsSubStep() ? SubStepId : StepId; }
};

/**
 * FFXR_StepRunner — the judge.
 *
 * Deliberately free of UObject, world and tick group: it is fed events, completions and a delta
 * time, and it answers with outcomes. That is what makes it testable without an engine running and
 * a session reproducible, which replay depends on (ADR-004).
 *
 * It works down a flat list of entries, strictly in order. It never changes the world - the GM's
 * events do that - and it never decides an entry is done; Complete Step does. What it does decide is
 * what every touch of an exposed object meant, and in Exam, whether the run has failed.
 */
struct FXR_TRAINING_API FFXR_StepRunner
{
	/** Begin a run at the first entry. Entries are moved in. */
	void Start(TArray<FFXR_CompiledStep>&& InEntries, EFXR_DeliveryMode InMode, int32 InMaxMistakes = 0);

	/** End the run early. The open entry is left as it is, for the report. */
	void Stop();

	/**
	 * Finish the open entry and open the next, or end the run after the last.
	 *
	 * With an id, only the entry it names can be finished, and only while it is the one open - so an
	 * event that fires every time something is grabbed advances the procedure once, where it belongs,
	 * and does nothing the rest of the time. With no id, it finishes whatever is open. Returns
	 * whether an entry was finished.
	 */
	bool CompleteStep(FName Id);

	/** Judge an interaction. Safe to call when nothing is running. Never completes anything. */
	void HandleEvent(const FFXR_InteractionEvent& Event);

	/** Advance Exam's timeouts. Safe to call when nothing is running. */
	void Tick(float DeltaSeconds);

	bool IsRunning() const { return bRunning; }
	EFXR_DeliveryMode GetMode() const { return Mode; }
	float GetElapsedSeconds() const { return Elapsed; }

	const TArray<FFXR_CompiledStep>& GetEntries() const { return Entries; }
	const TArray<FFXR_Mistake>& GetMistakes() const { return Mistakes; }

	/** The open entry, or INDEX_NONE before the run and after it. */
	int32 GetCurrentEntry() const { return Current; }

	EFXR_StepStatus GetStatus(int32 EntryIndex) const;

	//~ Per-entry history, kept after an entry closes so a report can be built from it.
	float GetOpenedAt(int32 EntryIndex) const;
	float GetClosedAt(int32 EntryIndex) const;
	int32 GetMistakeCount(int32 EntryIndex) const;

	/** True when the last entry was finished, rather than the run being stopped or failed. */
	bool ReachedEnd() const { return bReachedEnd; }

	/** True when the run ended against the trainee. Exam only. */
	bool Failed() const { return bFailed; }

	/** The entry that ended a failed run, or INDEX_NONE. */
	int32 GetFailedEntry() const { return FailedEntry; }

	EFXR_FailReason GetFailReason() const { return FailReason; }

	/** The mistake limit this run is held to. Zero when there is none, which is every Training run. */
	int32 GetMaxMistakes() const { return MaxMistakes; }

	//~ Observers. Set once when the session starts; never reassigned per frame.
	TFunction<void(int32 /*EntryIndex*/)> OnEntryActivated;
	TFunction<void(int32 /*EntryIndex*/)> OnEntryCompleted;
	TFunction<void(const FFXR_Mistake&)> OnMistake;
	TFunction<void()> OnFinished;

private:
	void Activate(int32 EntryIndex);
	void RecordMistake(FName InteractionId, EFXR_MistakeKind Kind, const FText& Message);
	void Fail(int32 EntryIndex, EFXR_FailReason Reason);

	static bool UsesObject(const FFXR_CompiledStep& Entry, FName InteractionId);

	TArray<FFXR_CompiledStep> Entries;
	TArray<EFXR_StepStatus> Status;

	//~ Parallel to Entries: a closed entry keeps its record.
	TArray<float> OpenedAt;
	TArray<float> ClosedAt;
	TArray<int32> MistakeCounts;
	TArray<FFXR_Mistake> Mistakes;

	int32 Current = INDEX_NONE;
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;
	float Elapsed = 0.f;

	/** When the open entry's step began - its first entry's open time - for StepTimeoutSeconds. */
	float StepOpenedAt = 0.f;
	bool bRunning = false;
	bool bReachedEnd = false;
	bool bFailed = false;
	int32 FailedEntry = INDEX_NONE;
	EFXR_FailReason FailReason = EFXR_FailReason::None;

	/** Exam's limit. Zero is no limit, and only an Exam run is ever given one. */
	int32 MaxMistakes = 0;
};
