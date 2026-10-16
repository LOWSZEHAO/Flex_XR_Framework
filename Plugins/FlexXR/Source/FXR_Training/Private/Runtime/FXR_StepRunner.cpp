// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Runtime/FXR_StepRunner.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Sentinel for "this never happened", so a report can tell a step that never opened from one at t=0. */
	constexpr float NeverHappened = -1.f;
}

void FFXR_StepRunner::Start(TArray<FFXR_CompiledStep>&& InSteps, EFXR_DeliveryMode InMode, int32 InMaxMistakes)
{
	Steps = MoveTemp(InSteps);
	Mode = InMode;
	Elapsed = 0.f;
	Current = INDEX_NONE;
	bReachedEnd = false;
	bFailed = false;
	FailedStep = INDEX_NONE;
	FailReason = EFXR_FailReason::None;

	// Held only in Exam. Training cannot fail, so a limit there would be a number with nothing to do.
	MaxMistakes = InMode == EFXR_DeliveryMode::Exam ? FMath::Max(0, InMaxMistakes) : 0;

	const int32 Count = Steps.Num();
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

EFXR_StepStatus FFXR_StepRunner::GetStatus(int32 StepIndex) const
{
	return Status.IsValidIndex(StepIndex) ? Status[StepIndex] : EFXR_StepStatus::Pending;
}

float FFXR_StepRunner::GetStepOpenedAt(int32 StepIndex) const
{
	return OpenedAt.IsValidIndex(StepIndex) ? OpenedAt[StepIndex] : NeverHappened;
}

float FFXR_StepRunner::GetStepClosedAt(int32 StepIndex) const
{
	return ClosedAt.IsValidIndex(StepIndex) ? ClosedAt[StepIndex] : NeverHappened;
}

int32 FFXR_StepRunner::GetStepMistakeCount(int32 StepIndex) const
{
	return MistakeCounts.IsValidIndex(StepIndex) ? MistakeCounts[StepIndex] : 0;
}

void FFXR_StepRunner::Activate(int32 StepIndex)
{
	Current = StepIndex;
	Status[StepIndex] = EFXR_StepStatus::Active;
	OpenedAt[StepIndex] = Elapsed;

	if (OnStepActivated)
	{
		OnStepActivated(StepIndex);
	}
}

bool FFXR_StepRunner::CompleteStep(FName StepId)
{
	if (!bRunning || !Steps.IsValidIndex(Current))
	{
		return false;
	}

	// Named, and not the open one: an event firing at a step it does not belong to. Doing nothing
	// is the whole point - it is what lets "On Grabbed -> Complete Step" be wired once and trusted.
	if (!StepId.IsNone() && Steps[Current].StepId != StepId)
	{
		return false;
	}

	const int32 Finished = Current;
	Status[Finished] = EFXR_StepStatus::Complete;
	ClosedAt[Finished] = Elapsed;

	if (OnStepCompleted)
	{
		OnStepCompleted(Finished);
	}

	if (Steps.IsValidIndex(Finished + 1))
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

bool FFXR_StepRunner::UsesObject(const FFXR_CompiledStep& Step, FName InteractionId)
{
	return Step.Objects.Contains(InteractionId);
}

void FFXR_StepRunner::HandleEvent(const FFXR_InteractionEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_StepRunner_HandleEvent);

	if (!bRunning || !Steps.IsValidIndex(Current))
	{
		return;
	}

	// A mistake is counted once per action, on its start. A grab arrives as Began and then Ended;
	// the release of a wrong grab is the same mistake, not a second one.
	if (Event.Phase != EFXR_InteractionPhase::Began || Event.InteractionId.IsNone())
	{
		return;
	}

	const FFXR_CompiledStep& Open = Steps[Current];

	// 1. The open step's own object is never wrong.
	if (UsesObject(Open, Event.InteractionId))
	{
		return;
	}

	// 2. Given a line of its own for this step: that line is the most specific thing to say.
	for (const FFXR_CompiledWrongMessage& Wrong : Open.WrongMessages)
	{
		if (Wrong.InteractionId == Event.InteractionId)
		{
			RecordMistake(Event.InteractionId, EFXR_MistakeKind::WrongAction, Wrong.Message);
			return;
		}
	}

	// 3. Used by a step that has finished: ignored. Procedures reuse things, and it outranks
	//    out-of-order so that an object a finished step used and a later step needs again reads as
	//    one in use rather than one reached for early.
	// 4. Needed by a step not yet open: out of order - the finding a gated world can never produce.
	bool bNeededLater = false;
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (!UsesObject(Steps[Index], Event.InteractionId))
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

	// 5. Anything else exposed is the wrong object. No list to keep: an object only reaches this
	//    runner when its author ticked Expose to Training, which is already the choice of what counts.
	RecordMistake(Event.InteractionId, EFXR_MistakeKind::WrongAction, FText::GetEmpty());
}

void FFXR_StepRunner::RecordMistake(FName InteractionId, EFXR_MistakeKind Kind, const FText& Message)
{
	FFXR_Mistake& Mistake = Mistakes.AddDefaulted_GetRef();
	Mistake.StepId = Steps.IsValidIndex(Current) ? Steps[Current].StepId : NAME_None;
	Mistake.InteractionId = InteractionId;
	Mistake.Kind = Kind;
	Mistake.TimeSeconds = Elapsed;
	Mistake.Message = Message;

	// Attributed to the step that was open, so a report can say which part of the procedure is the
	// one people get wrong.
	if (MistakeCounts.IsValidIndex(Current))
	{
		++MistakeCounts[Current];
	}

	if (OnMistake)
	{
		OnMistake(Mistake);
	}

	// Training records the mistake and moves on: it is teaching, and there is nothing to fail. Exam
	// judges it after it has been reported, so anything listening hears the mistake and then the
	// verdict in that order. A timeout is left to Tick, which fails the run on it by itself.
	if (Mode != EFXR_DeliveryMode::Exam || Kind == EFXR_MistakeKind::Timeout)
	{
		return;
	}

	if (Steps.IsValidIndex(Current) && Steps[Current].bCritical)
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

	// Timeouts are an Exam rule. Training is teaching, and being slow while being taught is not a
	// finding.
	if (Mode != EFXR_DeliveryMode::Exam || !Steps.IsValidIndex(Current))
	{
		return;
	}

	const FFXR_CompiledStep& Open = Steps[Current];
	if (Open.TimeoutSeconds > 0.f && Elapsed - OpenedAt[Current] >= Open.TimeoutSeconds)
	{
		RecordMistake(NAME_None, EFXR_MistakeKind::Timeout, FText::GetEmpty());
		Fail(Current, EFXR_FailReason::Timeout);
	}
}

void FFXR_StepRunner::Fail(int32 StepIndex, EFXR_FailReason Reason)
{
	// A run is judged once. Two routes to failure in the same frame must not report twice.
	if (!bRunning)
	{
		return;
	}

	bRunning = false;
	bFailed = true;
	FailedStep = StepIndex;
	FailReason = Reason;

	// Closed, but not Complete. The step is left Active because that is what it was - open, and never
	// finished - while the close time is recorded because the run ended on it.
	if (ClosedAt.IsValidIndex(StepIndex))
	{
		ClosedAt[StepIndex] = Elapsed;
	}

	Current = INDEX_NONE;

	if (OnFinished)
	{
		OnFinished();
	}
}
