// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Components/FXR_TrainingSession.h"
#include "Graph/FXR_StepGraph.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Events/FXR_EventBus.h"
#include "Highlight/FXR_HighlightSubsystem.h"
#include "Interactable/FXR_InteractableBase.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"

DEFINE_LOG_CATEGORY_STATIC(LogFXRTraining, Log, All);

UFXR_TrainingSession::UFXR_TrainingSession()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UFXR_TrainingSession::BeginPlay()
{
	Super::BeginPlay();

	if (UFXR_EventBus* Bus = UFXR_EventBus::Get(this))
	{
		BusHandle = Bus->OnInteractionEvent().AddUObject(this, &UFXR_TrainingSession::HandleBusEvent);
	}
	else
	{
		UE_LOG(LogFXRTraining, Warning, TEXT("No event bus in this world; the session will never see an interaction."));
	}

	if (bAutoStart)
	{
		StartSession();
	}
}

void UFXR_TrainingSession::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (BusHandle.IsValid())
	{
		if (UFXR_EventBus* Bus = UFXR_EventBus::Get(this))
		{
			Bus->OnInteractionEvent().Remove(BusHandle);
		}
		BusHandle.Reset();
	}

	ReleaseAll();
	Runner.Stop();

	Super::EndPlay(EndPlayReason);
}

void UFXR_TrainingSession::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	Runner.Tick(DeltaTime);
}

UFXR_InteractableBase* UFXR_TrainingSession::ResolveTarget(FName InteractionId) const
{
	const UFXR_InteractionSubsystem* Registry = UFXR_InteractionSubsystem::Get(this);
	return Registry ? Registry->FindByInteractionId(InteractionId) : nullptr;
}

void UFXR_TrainingSession::LockAllGatedSteps()
{
	for (const FFXR_CompiledStep& Step : Runner.GetSteps())
	{
		if (!Step.bHardLock)
		{
			continue;
		}

		for (const FFXR_CompiledTransition& Transition : Step.Transitions)
		{
			if (UFXR_InteractableBase* Target = ResolveTarget(Transition.InteractionId))
			{
				Target->SetInteractionEnabled(false);
				LockedIds.Add(Transition.InteractionId);
			}
		}
	}
}

void UFXR_TrainingSession::ApplyStepPresentation(int32 StepIndex, bool bOpen)
{
	const TArray<FFXR_CompiledStep>& Steps = Runner.GetSteps();
	if (!Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	const FFXR_CompiledStep& Step = Steps[StepIndex];
	UFXR_HighlightSubsystem* Highlight = UFXR_HighlightSubsystem::Get(this);

	// Hints are a presentation decision; whether the step is open is not. Exam runs light nothing up
	// and still score identically.
	const bool bWantHighlight = bOpen && bHighlightCurrentStep && Runner.ShouldShowHints();

	for (const FFXR_CompiledTransition& Transition : Step.Transitions)
	{
		UFXR_InteractableBase* Target = ResolveTarget(Transition.InteractionId);
		if (!Target)
		{
			continue;
		}

		if (Step.bHardLock)
		{
			// Opt-in interlock: open the way while the step is live, close it again after. This is
			// the same switch a game uses for a cutscene, and the only place training touches state.
			Target->SetInteractionEnabled(bOpen);
			if (bOpen)
			{
				LockedIds.Remove(Transition.InteractionId);
			}
			else
			{
				LockedIds.Add(Transition.InteractionId);
			}
		}

		if (Highlight)
		{
			const bool bAlready = GuidedIds.Contains(Transition.InteractionId);
			if (bWantHighlight && !bAlready)
			{
				Highlight->SetGuidance(Target, true);
				GuidedIds.Add(Transition.InteractionId);
			}
			else if (!bWantHighlight && bAlready)
			{
				Highlight->SetGuidance(Target, false);
				GuidedIds.Remove(Transition.InteractionId);
			}
		}
	}
}

void UFXR_TrainingSession::ReleaseAll()
{
	UFXR_HighlightSubsystem* Highlight = UFXR_HighlightSubsystem::Get(this);

	for (const FName Id : GuidedIds)
	{
		if (UFXR_InteractableBase* Target = ResolveTarget(Id))
		{
			if (Highlight)
			{
				Highlight->SetGuidance(Target, false);
			}
		}
	}
	GuidedIds.Reset();

	// Only ever re-enable what this session closed. Anything disabled for its own reasons stays that way.
	for (const FName Id : LockedIds)
	{
		if (UFXR_InteractableBase* Target = ResolveTarget(Id))
		{
			Target->SetInteractionEnabled(true);
		}
	}
	LockedIds.Reset();
}

void UFXR_TrainingSession::StartSession()
{
	if (!Graph)
	{
		UE_LOG(LogFXRTraining, Warning, TEXT("No step graph assigned; nothing to run."));
		return;
	}

	TArray<FFXR_CompiledStep> Compiled;
	TArray<FString> Errors;
	int32 EntryIndex = INDEX_NONE;
	const bool bRunnable = Graph->Compile(Compiled, EntryIndex, Errors);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogFXRTraining, Warning, TEXT("%s: %s"), *Graph->GetName(), *Error);
	}

	if (!bRunnable)
	{
		UE_LOG(LogFXRTraining, Error, TEXT("%s cannot run; see the warnings above."), *Graph->GetName());
		return;
	}

	ReleaseAll();

	// Bound before Start, because Start opens the entry step and that callback is what presents it.
	Runner.OnStepActivated = [this](int32 StepIndex)
	{
		ApplyStepPresentation(StepIndex, true);
		const FFXR_CompiledStep& Step = Runner.GetSteps()[StepIndex];
		const FFXR_StepDefinition* Authored = Graph ? Graph->Steps.FindByPredicate(
			[&Step](const FFXR_StepDefinition& Candidate) { return Candidate.StepId == Step.StepId; }) : nullptr;
		OnStepActivated.Broadcast(Step.StepId, Authored ? Authored->Instruction : FText::GetEmpty());
	};

	Runner.OnStepCompleted = [this](int32 StepIndex)
	{
		ApplyStepPresentation(StepIndex, false);
		OnStepCompleted.Broadcast(Runner.GetSteps()[StepIndex].StepId, FText::GetEmpty());
	};

	Runner.OnMistake = [this](const FFXR_Mistake& Mistake)
	{
		OnMistake.Broadcast(Mistake);
	};

	Runner.OnHintEscalated = [this](int32 StepIndex, int32 /*HintLevel*/)
	{
		// Practice mode stays quiet until something goes wrong, and a timeout counts, so the
		// presentation is re-evaluated rather than assumed unchanged.
		ApplyStepPresentation(StepIndex, true);
	};

	Runner.OnFinished = [this]()
	{
		ReleaseAll();
		OnSessionFinished.Broadcast();
	};

	Runner.Start(MoveTemp(Compiled), EntryIndex, Mode);
	LockAllGatedSteps();

	// The entry step opened during Start, before the locks went on, so give it back its way through.
	for (const int32 StepIndex : Runner.GetActiveSteps())
	{
		ApplyStepPresentation(StepIndex, true);
	}
}

void UFXR_TrainingSession::StopSession()
{
	Runner.Stop();
	ReleaseAll();
}

TArray<FName> UFXR_TrainingSession::GetActiveStepIds() const
{
	TArray<FName> Out;
	const TArray<FFXR_CompiledStep>& Steps = Runner.GetSteps();
	Out.Reserve(Runner.GetActiveSteps().Num());
	for (const int32 Index : Runner.GetActiveSteps())
	{
		if (Steps.IsValidIndex(Index))
		{
			Out.Add(Steps[Index].StepId);
		}
	}
	return Out;
}

void UFXR_TrainingSession::HandleBusEvent(const FFXR_InteractionEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_TrainingSession_HandleBusEvent);
	Runner.HandleEvent(Event);
}
