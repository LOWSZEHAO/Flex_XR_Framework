// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Events/FXR_InteractionEvent.h"
#include "Types/FXR_TrainingTypes.h"

/**
 * One thing to complete: a sub-step, or a step without sub-steps. Built by UFXR_TrainingGM::Compile.
 *
 * A step with three sub-steps becomes three entries in a row with the same StepId and StepIndex.
 * The runner just walks the flat list; the session groups entries back into steps for the panel
 * and the report.
 *
 * Nothing here says how an entry finishes. The project calls Complete Step when it's done, so the
 * runner never needs to know about latches, sockets and so on (ADR-004 / ADR-011).
 */
struct FFXR_CompiledStep
{
	/** The step this entry is, or belongs to. */
	FName StepId;

	/** Index of the step in the Steps asset. Neighbouring entries with the same index are one step. */
	int32 StepIndex = INDEX_NONE;

	/** The sub-step, or None for a whole step. */
	FName SubStepId;

	/** Index in the step's Sub Steps list, or INDEX_NONE for a whole step. */
	int32 SubStepIndex = INDEX_NONE;

	/** Objects this entry uses. Fine while it's open, out of order before. */
	TArray<FName> Objects;

	/** Exam only: fail if open longer than this. 0 = no limit. */
	float TimeoutSeconds = 0.f;

	/** Exam only: limit for the whole step, counted from its first entry. Only set on sub-steps. */
	float StepTimeoutSeconds = 0.f;

	/** Exam only: any mistake while this is open fails the run. */
	bool bCritical = false;

	bool IsSubStep() const { return SubStepIndex != INDEX_NONE; }

	/** The id Complete Step needs for this entry: the sub-step id, or the step id. */
	FName GetCompletionId() const { return IsSubStep() ? SubStepId : StepId; }
};

/**
 * FFXR_StepRunner - runs the entries in order and judges what the trainee touches.
 *
 * No UObjects, no world, no tick group: it gets events, completions and delta time and reports
 * back through callbacks. That keeps it unit-testable and deterministic for replay (ADR-004).
 *
 * It doesn't change the world (the GM's events do) and doesn't decide when an entry is done
 * (Complete Step does). It decides whether a touch was a mistake, and in Exam whether the run failed.
 */
struct FXR_TRAINING_API FFXR_StepRunner
{
	/** Start at the first entry. */
	void Start(TArray<FFXR_CompiledStep>&& InEntries, EFXR_DeliveryMode InMode, int32 InMaxMistakes = 0);

	/** End the run early. The open entry stays as it was, for the report. */
	void Stop();

	/**
	 * Finish the open entry and open the next one, or end the run after the last.
	 *
	 * With an id, it only works if that entry is the open one, so an event that fires on every grab
	 * only advances things at the right moment. With no id it finishes whatever is open. Returns
	 * whether anything was finished.
	 */
	bool CompleteStep(FName Id);

	/** Judge an interaction. Safe to call when nothing is running. */
	void HandleEvent(const FFXR_InteractionEvent& Event);

	/** Advance time for Exam timeouts. Safe to call when nothing is running. */
	void Tick(float DeltaSeconds);

	bool IsRunning() const { return bRunning; }
	EFXR_DeliveryMode GetMode() const { return Mode; }
	float GetElapsedSeconds() const { return Elapsed; }

	const TArray<FFXR_CompiledStep>& GetEntries() const { return Entries; }
	const TArray<FFXR_Mistake>& GetMistakes() const { return Mistakes; }

	/** The open entry, or INDEX_NONE outside a run. */
	int32 GetCurrentEntry() const { return Current; }

	EFXR_StepStatus GetStatus(int32 EntryIndex) const;

	//~ Per-entry history, kept after the entry closes for the report.
	float GetOpenedAt(int32 EntryIndex) const;
	float GetClosedAt(int32 EntryIndex) const;
	int32 GetMistakeCount(int32 EntryIndex) const;

	/** True if the last entry was completed (not stopped or failed). */
	bool ReachedEnd() const { return bReachedEnd; }

	/** True if the run failed. Exam only. */
	bool Failed() const { return bFailed; }

	/** The entry the run failed on, or INDEX_NONE. */
	int32 GetFailedEntry() const { return FailedEntry; }

	EFXR_FailReason GetFailReason() const { return FailReason; }

	/** Mistake limit for this run. Always 0 in Training. */
	int32 GetMaxMistakes() const { return MaxMistakes; }

	//~ Callbacks, bound once by the session before Start.
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

	//~ Parallel to Entries.
	TArray<float> OpenedAt;
	TArray<float> ClosedAt;
	TArray<int32> MistakeCounts;
	TArray<FFXR_Mistake> Mistakes;

	int32 Current = INDEX_NONE;
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;
	float Elapsed = 0.f;

	/** When the current step's first entry opened, for StepTimeoutSeconds. */
	float StepOpenedAt = 0.f;
	bool bRunning = false;
	bool bReachedEnd = false;
	bool bFailed = false;
	int32 FailedEntry = INDEX_NONE;
	EFXR_FailReason FailReason = EFXR_FailReason::None;

	/** 0 = no limit. Only set in Exam. */
	int32 MaxMistakes = 0;
};
