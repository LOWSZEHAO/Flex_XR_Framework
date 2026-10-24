// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Runtime/FXR_StepRunner.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Marks "never happened", so an entry that never opened isn't confused with one opened at t=0. */
	constexpr float NeverHappened = -1.f;
}

void FFXR_StepRunner::Start(TArray<FFXR_CompiledStep>&& InEntries, EFXR_DeliveryMode InMode, int32 InMaxMistakes)
{
	Entries = MoveTemp(InEntries);
	Mode = InMode;
	Elapsed = 0.f;
	StepOpenedAt = 0.f;
	Current = INDEX_NONE;
	bReachedEnd = false;
	bFailed = false;
	FailedEntry = INDEX_NONE;
	FailReason = EFXR_FailReason::None;

	// Only Exam has a mistake limit.
	MaxMistakes = InMode == EFXR_DeliveryMode::Exam ? FMath::Max(0, InMaxMistakes) : 0;

	const int32 Count = Entries.Num();
	Status.Init(EFXR_StepStatus::Pending, Count);
	OpenedAt.Init(NeverHappened, Count);
	ClosedAt.Init(NeverHappened, Count);
	MistakeCounts.Init(0, Count);
	Mistakes.Reset();

	bRunning = Count > 0;
	if (bRunning)
	{
		Activate(0);
	}
}

void FFXR_StepRunner::Stop()
{
	bRunning = false;
}

EFXR_StepStatus FFXR_StepRunner::GetStatus(int32 EntryIndex) const
{
	return Status.IsValidIndex(EntryIndex) ? Status[EntryIndex] : EFXR_StepStatus::Pending;
}

float FFXR_StepRunner::GetOpenedAt(int32 EntryIndex) const
{
	return OpenedAt.IsValidIndex(EntryIndex) ? OpenedAt[EntryIndex] : NeverHappened;
}

float FFXR_StepRunner::GetClosedAt(int32 EntryIndex) const
{
	return ClosedAt.IsValidIndex(EntryIndex) ? ClosedAt[EntryIndex] : NeverHappened;
}

int32 FFXR_StepRunner::GetMistakeCount(int32 EntryIndex) const
{
	return MistakeCounts.IsValidIndex(EntryIndex) ? MistakeCounts[EntryIndex] : 0;
}

void FFXR_StepRunner::Activate(int32 EntryIndex)
{
	Current = EntryIndex;
	Status[EntryIndex] = EFXR_StepStatus::Active;
	OpenedAt[EntryIndex] = Elapsed;

	if (EntryIndex == 0 || Entries[EntryIndex - 1].StepIndex != Entries[EntryIndex].StepIndex)
	{
		StepOpenedAt = Elapsed;
	}

	if (OnEntryActivated)
	{
		OnEntryActivated(EntryIndex);
	}
}

bool FFXR_StepRunner::CompleteStep(FName Id)
{
	if (!bRunning || !Entries.IsValidIndex(Current))
	{
		return false;
	}

	// An id that isn't the open entry's: ignore it. This is what makes it safe to wire
	// "On Grabbed -> Complete Step" once. A step with sub-steps never matches its own id here.
	if (!Id.IsNone() && Entries[Current].GetCompletionId() != Id)
	{
		return false;
	}

	const int32 Finished = Current;
	Status[Finished] = EFXR_StepStatus::Complete;
	ClosedAt[Finished] = Elapsed;

	if (OnEntryCompleted)
	{
		OnEntryCompleted(Finished);
	}

	if (Entries.IsValidIndex(Finished + 1))
	{
		Activate(Finished + 1);
		return true;
	}

	Current = INDEX_NONE;
	bRunning = false;
	bReachedEnd = true;

	if (OnFinished)
	{
		OnFinished();
	}

	return true;
}

bool FFXR_StepRunner::UsesObject(const FFXR_CompiledStep& Entry, FName InteractionId)
{
	return Entry.Objects.Contains(InteractionId);
}

void FFXR_StepRunner::HandleEvent(const FFXR_InteractionEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_StepRunner_HandleEvent);

	if (!bRunning || !Entries.IsValidIndex(Current))
	{
		return;
	}

	// Count a mistake once, on Began. A wrong grab also sends Ended when released; that isn't a
	// second mistake.
	if (Event.Phase != EFXR_InteractionPhase::Began || Event.InteractionId.IsNone())
	{
		return;
	}

	const FFXR_CompiledStep& Open = Entries[Current];

	// 1. The open entry's own object is never a mistake.
	if (UsesObject(Open, Event.InteractionId))
	{
		return;
	}

	// 2. Used by an entry that's already finished: ignore. Procedures reuse objects, and this wins
	//    over out-of-order so an object used earlier and needed again later isn't flagged.
	// 3. Needed by an entry that isn't open yet: out of order. Sub-steps count as entries, so
	//    picking up the pin before the extinguisher is out of order even within one step.
	bool bNeededLater = false;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		if (!UsesObject(Entries[Index], Event.InteractionId))
		{
			continue;
		}

		if (Status[Index] == EFXR_StepStatus::Pending)
		{
			bNeededLater = true;
		}
		else
		{
			return;
		}
	}

	if (bNeededLater)
	{
		RecordMistake(Event.InteractionId, EFXR_MistakeKind::OutOfOrder, FText::GetEmpty());
		return;
	}

	// 4. Anything else that's exposed is the wrong object. Only objects with Expose to Training
	//    reach the runner, so there's no separate list to maintain.
	RecordMistake(Event.InteractionId, EFXR_MistakeKind::WrongAction, FText::GetEmpty());
}

void FFXR_StepRunner::RecordMistake(FName InteractionId, EFXR_MistakeKind Kind, const FText& Message)
{
	FFXR_Mistake& Mistake = Mistakes.AddDefaulted_GetRef();
	if (Entries.IsValidIndex(Current))
	{
		Mistake.StepId = Entries[Current].StepId;
		Mistake.SubStepId = Entries[Current].SubStepId;
	}
	Mistake.InteractionId = InteractionId;
	Mistake.Kind = Kind;
	Mistake.TimeSeconds = Elapsed;
	Mistake.Message = Message;

	// Per-entry count, so the report can show which part people get wrong.
	if (MistakeCounts.IsValidIndex(Current))
	{
		++MistakeCounts[Current];
	}

	if (OnMistake)
	{
		OnMistake(Mistake);
	}

	// Training just records it. Exam checks for failure after reporting the mistake, so listeners
	// see the mistake before the result. Timeouts are failed by Tick directly.
	if (Mode != EFXR_DeliveryMode::Exam || Kind == EFXR_MistakeKind::Timeout)
	{
		return;
	}

	if (Entries.IsValidIndex(Current) && Entries[Current].bCritical)
	{
		Fail(Current, EFXR_FailReason::CriticalMistake);
	}
	else if (MaxMistakes > 0 && Mistakes.Num() >= MaxMistakes)
	{
		Fail(Current, EFXR_FailReason::TooManyMistakes);
	}
}

void FFXR_StepRunner::Tick(float DeltaSeconds)
{
	if (!bRunning)
	{
		return;
	}

	Elapsed += DeltaSeconds;

	// Timeouts only apply in Exam.
	if (Mode != EFXR_DeliveryMode::Exam || !Entries.IsValidIndex(Current))
	{
		return;
	}

	// Two timers: the entry's own, and the step's, which keeps running across sub-steps.
	const FFXR_CompiledStep& Open = Entries[Current];
	const bool bEntryOver = Open.TimeoutSeconds > 0.f && Elapsed - OpenedAt[Current] >= Open.TimeoutSeconds;
	const bool bStepOver = Open.StepTimeoutSeconds > 0.f && Elapsed - StepOpenedAt >= Open.StepTimeoutSeconds;

	if (bEntryOver || bStepOver)
	{
		RecordMistake(NAME_None, EFXR_MistakeKind::Timeout, FText::GetEmpty());
		Fail(Current, EFXR_FailReason::Timeout);
	}
}

void FFXR_StepRunner::Fail(int32 EntryIndex, EFXR_FailReason Reason)
{
	// Only fail once, even if two failure conditions hit in the same frame.
	if (!bRunning)
	{
		return;
	}

	bRunning = false;
	bFailed = true;
	FailedEntry = EntryIndex;
	FailReason = Reason;

	// The entry stays Active (it was never finished), but gets a close time because the run
	// ended on it.
	if (ClosedAt.IsValidIndex(EntryIndex))
	{
		ClosedAt[EntryIndex] = Elapsed;
	}

	Current = INDEX_NONE;

	if (OnFinished)
	{
		OnFinished();
	}
}
