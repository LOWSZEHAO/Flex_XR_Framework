// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Runtime/FXR_StepRunner.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

void FFXR_StepRunner::Start(TArray<FFXR_CompiledStep>&& InSteps, int32 EntryIndex, EFXR_DeliveryMode InMode)
{
	Steps = MoveTemp(InSteps);
	Mode = InMode;
	Elapsed = 0.f;
	bAnyMistakeYet = false;

	Status.Init(EFXR_StepStatus::Pending, Steps.Num());
	ActiveSteps.Reset();
	ActiveElapsed.Reset();
	ActiveHintLevel.Reset();
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
	ActiveSteps.Add(StepIndex);
	ActiveElapsed.Add(0.f);
	ActiveHintLevel.Add(0);

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
	const int32 Slot = ActiveSteps.IndexOfByKey(StepIndex);
	if (Slot != INDEX_NONE)
	{
		ActiveSteps.RemoveAt(Slot, EAllowShrinking::No);
		ActiveElapsed.RemoveAt(Slot, EAllowShrinking::No);
		ActiveHintLevel.RemoveAt(Slot, EAllowShrinking::No);
	}

	Status[StepIndex] = EFXR_StepStatus::Complete;

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

	// Completion is resolved first and against a copy of the active set, because completing a step
	// opens others and mutating what we are walking would let one event cascade through a chain.
	int32 CompletedIndex = INDEX_NONE;
	FFXR_CompiledTransition FiredTransition;

	for (const int32 StepIndex : ActiveSteps)
	{
		const FFXR_CompiledStep& Step = Steps[StepIndex];
		for (const FFXR_CompiledTransition& Transition : Step.Transitions)
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

	for (int32 Slot = 0; Slot < ActiveSteps.Num(); ++Slot)
	{
		const int32 StepIndex = ActiveSteps[Slot];
		const FFXR_CompiledStep& Step = Steps[StepIndex];
		if (Step.TimeoutSeconds <= 0.f)
		{
			continue;
		}

		ActiveElapsed[Slot] += DeltaSeconds;

		// Escalation is per elapsed multiple of the timeout, so a trainee who is lost gets a
		// progressively louder hint rather than one nag and then silence.
		const int32 WantedLevel = FMath::FloorToInt32(ActiveElapsed[Slot] / Step.TimeoutSeconds);
		if (WantedLevel > ActiveHintLevel[Slot])
		{
			ActiveHintLevel[Slot] = WantedLevel;

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
