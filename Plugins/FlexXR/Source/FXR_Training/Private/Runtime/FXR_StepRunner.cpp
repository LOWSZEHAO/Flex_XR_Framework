// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Runtime/FXR_StepRunner.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

namespace
{
	/** Sentinel for "this never happened", so a report can tell a skipped step from one at t=0. */
	constexpr float NeverHappened = -1.f;
}

void FFXR_StepRunner::Start(TArray<FFXR_CompiledStep>&& InSteps, int32 EntryIndex, EFXR_DeliveryMode InMode)
{
	Steps = MoveTemp(InSteps);
	Mode = InMode;
	Elapsed = 0.f;
	bAnyMistakeYet = false;
	bReachedEnd = false;

	const int32 Count = Steps.Num();
	Status.Init(EFXR_StepStatus::Pending, Count);
	OpenedAt.Init(NeverHappened, Count);
	ClosedAt.Init(NeverHappened, Count);
	HintLevelReached.Init(0, Count);
	MistakeCounts.Init(0, Count);

	ActiveSteps.Reset();
	Mistakes.Reset();

	bRunning = Steps.IsValidIndex(EntryIndex);
	if (bRunning)
	{
		Activate(EntryIndex);
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

int32 FFXR_StepRunner::GetStepHintLevel(int32 StepIndex) const
{
	return HintLevelReached.IsValidIndex(StepIndex) ? HintLevelReached[StepIndex] : 0;
}

int32 FFXR_StepRunner::GetStepMistakeCount(int32 StepIndex) const
{
	return MistakeCounts.IsValidIndex(StepIndex) ? MistakeCounts[StepIndex] : 0;
}

bool FFXR_StepRunner::ShouldShowHints() const
{
	switch (Mode)
	{
	case EFXR_DeliveryMode::Guided:
		return true;
	case EFXR_DeliveryMode::Practice:
		return bAnyMistakeYet;
	case EFXR_DeliveryMode::Exam:
	default:
		return false;
	}
}

void FFXR_StepRunner::Activate(int32 StepIndex)
{
	if (!Steps.IsValidIndex(StepIndex) || Status[StepIndex] != EFXR_StepStatus::Pending)
	{
		return; // already open, done, or bypassed; re-entering would restart its clock
	}

	Status[StepIndex] = EFXR_StepStatus::Active;
	OpenedAt[StepIndex] = Elapsed;
	ActiveSteps.Add(StepIndex);

	if (OnStepActivated)
	{
		OnStepActivated(StepIndex);
	}
}

bool FFXR_StepRunner::Matches(const FFXR_CompiledTransition& Transition, const FFXR_InteractionEvent& Event) const
{
	return Transition.InteractionId == Event.InteractionId && Transition.Phase == Event.Phase;
}

void FFXR_StepRunner::Complete(int32 StepIndex, const FFXR_CompiledTransition& Via)
{
	ActiveSteps.RemoveSingle(StepIndex);
	Status[StepIndex] = EFXR_StepStatus::Complete;
	ClosedAt[StepIndex] = Elapsed;

	if (OnStepCompleted)
	{
		OnStepCompleted(StepIndex);
	}

	// Every next step on the transition that fired opens at once. One of them is a plain sequence;
	// several is the parallel case.
	for (const int32 Next : Via.NextSteps)
	{
		Activate(Next);
	}

	// Nothing open and nothing opened means the procedure is over. A run can also end with steps
	// still Pending, which is exactly what a branch not taken looks like.
	if (ActiveSteps.IsEmpty())
	{
		bRunning = false;
		bReachedEnd = true;
		if (OnFinished)
		{
			OnFinished();
		}
	}
}

void FFXR_StepRunner::RecordMistake(FName StepId, FName InteractionId, EFXR_MistakeKind Kind, const FText& Message)
{
	FFXR_Mistake& Mistake = Mistakes.AddDefaulted_GetRef();
	Mistake.StepId = StepId;
	Mistake.InteractionId = InteractionId;
	Mistake.Kind = Kind;
	Mistake.TimeSeconds = Elapsed;
	Mistake.Message = Message;

	// Attributed to the step that was open, so a report can say which part of the procedure is the
	// one people get wrong.
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (Steps[Index].StepId == StepId)
		{
			++MistakeCounts[Index];
			break;
		}
	}

	bAnyMistakeYet = true;

	if (OnMistake)
	{
		OnMistake(Mistake);
	}
}

void FFXR_StepRunner::HandleEvent(const FFXR_InteractionEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_StepRunner_HandleEvent);

	if (!bRunning)
	{
		return;
	}

	// Completion is resolved before anything mutates, because completing a step opens others and
	// walking a set while it changes would let one event cascade through a chain.
	int32 CompletedIndex = INDEX_NONE;
	FFXR_CompiledTransition FiredTransition;

	for (const int32 StepIndex : ActiveSteps)
	{
		for (const FFXR_CompiledTransition& Transition : Steps[StepIndex].Transitions)
		{
			if (Matches(Transition, Event))
			{
				CompletedIndex = StepIndex;
				FiredTransition = Transition;
				break;
			}
		}

		if (CompletedIndex != INDEX_NONE)
		{
			break;
		}
	}

	if (CompletedIndex != INDEX_NONE)
	{
		Complete(CompletedIndex, FiredTransition);
		return;
	}

	// Not a completion. Anything an open step called out as wrong is the most specific thing we can
	// say, so it wins over the generic out-of-order reading.
	for (const int32 StepIndex : ActiveSteps)
	{
		const FFXR_CompiledStep& Step = Steps[StepIndex];
		for (const FFXR_CompiledWrongAction& Wrong : Step.WrongActions)
		{
			if (Wrong.InteractionId == Event.InteractionId)
			{
				RecordMistake(Step.StepId, Event.InteractionId, EFXR_MistakeKind::WrongAction, Wrong.Message);
				return;
			}
		}
	}

	// Otherwise: did they just do something that belongs to a step which has not opened yet? That is
	// the finding a gated world can never produce, and it is the reason this runner only watches.
	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		if (Status[Index] != EFXR_StepStatus::Pending)
		{
			continue;
		}

		for (const FFXR_CompiledTransition& Transition : Steps[Index].Transitions)
		{
			if (Matches(Transition, Event))
			{
				const FName OpenStep = ActiveSteps.IsEmpty() ? NAME_None : Steps[ActiveSteps[0]].StepId;
				RecordMistake(OpenStep, Event.InteractionId, EFXR_MistakeKind::OutOfOrder, FText::GetEmpty());
				return;
			}
		}
	}
}

void FFXR_StepRunner::Tick(float DeltaSeconds)
{
	if (!bRunning)
	{
		return;
	}

	Elapsed += DeltaSeconds;

	for (const int32 StepIndex : ActiveSteps)
	{
		const FFXR_CompiledStep& Step = Steps[StepIndex];
		if (Step.TimeoutSeconds <= 0.f)
		{
			continue;
		}

		const float OpenFor = Elapsed - OpenedAt[StepIndex];

		// Escalation is per elapsed multiple of the timeout, so someone who is lost gets a
		// progressively louder hint rather than one nag and then silence.
		const int32 WantedLevel = FMath::FloorToInt32(OpenFor / Step.TimeoutSeconds);
		if (WantedLevel > HintLevelReached[StepIndex])
		{
			HintLevelReached[StepIndex] = WantedLevel;

			// The first expiry is the one worth recording. After that it is the same finding louder.
			if (WantedLevel == 1)
			{
				RecordMistake(Step.StepId, NAME_None, EFXR_MistakeKind::Timeout, FText::GetEmpty());
			}

			if (OnHintEscalated)
			{
				OnHintEscalated(StepIndex, WantedLevel);
			}
		}
	}
}
