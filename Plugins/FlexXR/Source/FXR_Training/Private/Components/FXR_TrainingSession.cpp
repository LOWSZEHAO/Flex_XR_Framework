// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Components/FXR_TrainingSession.h"

#include "Data/FXR_TrainingSteps.h"
#include "Detection/FXR_InteractionSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Events/FXR_EventBus.h"
#include "GameFramework/Actor.h"
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

const UFXR_TrainingSteps* UFXR_TrainingSession::GetStepsAsset() const
{
	return GM ? GM->Steps.Get() : nullptr;
}

UFXR_InteractableBase* UFXR_TrainingSession::ResolveTarget(FName InteractionId) const
{
	const UFXR_InteractionSubsystem* Registry = UFXR_InteractionSubsystem::Get(this);
	return Registry ? Registry->FindByInteractionId(InteractionId) : nullptr;
}

UFXR_TrainingSession* UFXR_TrainingSession::FindInWorld(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}

	UFXR_TrainingSession* First = nullptr;
	int32 Found = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (UFXR_TrainingSession* Candidate = It->FindComponentByClass<UFXR_TrainingSession>())
		{
			++Found;
			if (!First)
			{
				First = Candidate;
			}
		}
	}

	// One session is the ordinary case. Two is a decision nothing here is entitled to make silently.
	if (Found > 1)
	{
		UE_LOG(LogFXRTraining, Warning,
			TEXT("Found %d training sessions and used the first. Call it on the session you mean instead."), Found);
	}

	return First;
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Running
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingSession::StartSession()
{
	if (!GM)
	{
		UE_LOG(LogFXRTraining, Warning, TEXT("%s has no GM assigned; nothing to run."), *GetOwner()->GetName());
		return;
	}

	TArray<FFXR_CompiledStep> Compiled;
	TArray<FString> Errors;
	const bool bRunnable = GM->Compile(Compiled, StepEvents, Errors);

	for (const FString& Error : Errors)
	{
		UE_LOG(LogFXRTraining, Warning, TEXT("%s: %s"), *GM->GetName(), *Error);
	}

	if (!bRunnable)
	{
		UE_LOG(LogFXRTraining, Error, TEXT("%s cannot run; see the warnings above."), *GM->GetName());
		return;
	}

	ValidateEvents(Compiled);
	ReleaseAll();

	// Bound before Start, because Start opens the first step and that callback is what fires it.
	Runner.OnStepActivated = [this](int32 StepIndex)
	{
		FireStepEvents(StepIndex);
		SetGuidance(StepIndex, true);

		const FName StepId = Runner.GetSteps()[StepIndex].StepId;
		const UFXR_TrainingSteps* Steps = GetStepsAsset();
		const FFXR_StepInfo* Info = Steps ? Steps->FindStep(StepId) : nullptr;
		OnStepActivated.Broadcast(StepId, Info ? Info->Instruction : FText::GetEmpty());
	};

	Runner.OnStepCompleted = [this](int32 StepIndex)
	{
		SetGuidance(StepIndex, false);
		OnStepCompleted.Broadcast(Runner.GetSteps()[StepIndex].StepId, FText::GetEmpty());
	};

	Runner.OnMistake = [this](const FFXR_Mistake& Mistake)
	{
		OnMistake.Broadcast(Mistake);
	};

	Runner.OnFinished = [this]()
	{
		// Built before the highlight comes down, so the report describes the run and not the cleanup.
		const FFXR_SessionReport Report = BuildReport();
		ReleaseAll();
		OnSessionFinished.Broadcast(Report);
	};

	// Exam opens what it was told to before the first step arrives, so that step's own events land
	// on the world the rest of the run will see.
	if (Mode == EFXR_DeliveryMode::Exam)
	{
		FireExamStartEvents();
	}

	Runner.Start(MoveTemp(Compiled), Mode, GetMaxMistakes());
}

void UFXR_TrainingSession::StopSession()
{
	Runner.Stop();
	ReleaseAll();
}

bool UFXR_TrainingSession::CompleteStep(FName StepId)
{
	return Runner.CompleteStep(StepId);
}

void UFXR_TrainingSession::HandleBusEvent(const FFXR_InteractionEvent& Event)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_TrainingSession_HandleBusEvent);
	Runner.HandleEvent(Event);
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// The GM's events
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingSession::FireStepEvents(int32 StepIndex)
{
	if (!StepEvents.IsValidIndex(StepIndex))
	{
		return;
	}

	for (const FFXR_StepAction& Action : StepEvents[StepIndex])
	{
		// Already fired when the run began.
		if (Mode == EFXR_DeliveryMode::Exam && Action.bAutoStartInExam)
		{
			continue;
		}

		FireEvent(Action);
	}
}

void UFXR_TrainingSession::FireExamStartEvents()
{
	for (const TArray<FFXR_StepAction>& Events : StepEvents)
	{
		for (const FFXR_StepAction& Action : Events)
		{
			if (Action.bAutoStartInExam)
			{
				FireEvent(Action);
			}
		}
	}
}

void UFXR_TrainingSession::FireEvent(const FFXR_StepAction& Action) const
{
	UWorld* World = GetWorld();
	UClass* Class = Action.Blueprint.LoadSynchronous();
	if (!World || !Class || Action.EventName.IsNone())
	{
		return; // reported once by ValidateEvents rather than on every firing
	}

	UFunction* Function = Class->FindFunctionByName(Action.EventName);
	if (!Function || Function->NumParms > 0)
	{
		return;
	}

	for (TActorIterator<AActor> It(World, Class); It; ++It)
	{
		if (!Action.Tag.IsNone() && !It->ActorHasTag(Action.Tag))
		{
			continue;
		}

		It->ProcessEvent(Function, nullptr);
	}
}

void UFXR_TrainingSession::ValidateEvents(const TArray<FFXR_CompiledStep>& Steps) const
{
	const UWorld* World = GetWorld();

	for (int32 StepIndex = 0; StepIndex < StepEvents.Num(); ++StepIndex)
	{
		const FString Step = Steps.IsValidIndex(StepIndex) ? Steps[StepIndex].StepId.ToString() : FString::FromInt(StepIndex);

		for (const FFXR_StepAction& Action : StepEvents[StepIndex])
		{
			if (Action.Blueprint.IsNull())
			{
				UE_LOG(LogFXRTraining, Warning, TEXT("%s, step '%s': an event has no Blueprint set."), *GM->GetName(), *Step);
				continue;
			}

			UClass* Class = Action.Blueprint.LoadSynchronous();
			if (!Class)
			{
				UE_LOG(LogFXRTraining, Warning, TEXT("%s, step '%s': %s could not be loaded."),
					*GM->GetName(), *Step, *Action.Blueprint.ToString());
				continue;
			}

			const UFunction* Function = Action.EventName.IsNone() ? nullptr : Class->FindFunctionByName(Action.EventName);
			if (!Function)
			{
				UE_LOG(LogFXRTraining, Warning, TEXT("%s, step '%s': %s has no event '%s'."),
					*GM->GetName(), *Step, *Class->GetName(), *Action.EventName.ToString());
				continue;
			}

			if (Function->NumParms > 0)
			{
				UE_LOG(LogFXRTraining, Warning, TEXT("%s, step '%s': %s.%s takes inputs, and the GM can only call events that take none."),
					*GM->GetName(), *Step, *Class->GetName(), *Action.EventName.ToString());
				continue;
			}

			int32 Matches = 0;
			if (World)
			{
				for (TActorIterator<AActor> It(World, Class); It; ++It)
				{
					if (Action.Tag.IsNone() || It->ActorHasTag(Action.Tag))
					{
						++Matches;
					}
				}
			}

			if (Matches == 0)
			{
				UE_LOG(LogFXRTraining, Warning, TEXT("%s, step '%s': there is no %s in this level%s, so '%s' reaches nothing."),
					*GM->GetName(), *Step, *Class->GetName(),
					Action.Tag.IsNone() ? TEXT("") : *FString::Printf(TEXT(" tagged '%s'"), *Action.Tag.ToString()),
					*Action.EventName.ToString());
			}
		}
	}
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Training's highlight
// ───────────────────────────────────────────────────────────────────────────────────────────────

void UFXR_TrainingSession::SetGuidance(int32 StepIndex, bool bOn)
{
	const TArray<FFXR_CompiledStep>& Steps = Runner.GetSteps();
	if (!Steps.IsValidIndex(StepIndex))
	{
		return;
	}

	UFXR_HighlightSubsystem* Highlight = UFXR_HighlightSubsystem::Get(this);
	if (!Highlight)
	{
		return;
	}

	// Exam shows no hints, and the highlight is the loudest one there is.
	const bool bWant = bOn && bHighlightCurrentStep && Mode == EFXR_DeliveryMode::Training;

	for (const FName Id : Steps[StepIndex].Objects)
	{
		UFXR_InteractableBase* Target = ResolveTarget(Id);
		if (!Target)
		{
			continue;
		}

		const bool bAlready = GuidedIds.Contains(Id);
		if (bWant && !bAlready)
		{
			Highlight->SetGuidance(Target, true);
			GuidedIds.Add(Id);
		}
		else if (!bWant && bAlready)
		{
			Highlight->SetGuidance(Target, false);
			GuidedIds.Remove(Id);
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
}

// ───────────────────────────────────────────────────────────────────────────────────────────────
// Reading the run
// ───────────────────────────────────────────────────────────────────────────────────────────────

FName UFXR_TrainingSession::GetCurrentStepId() const
{
	const TArray<FFXR_CompiledStep>& Steps = Runner.GetSteps();
	return Steps.IsValidIndex(Runner.GetCurrentStep()) ? Steps[Runner.GetCurrentStep()].StepId : NAME_None;
}

TArray<FName> UFXR_TrainingSession::GetActiveStepIds() const
{
	TArray<FName> Out;
	const FName Current = GetCurrentStepId();
	if (!Current.IsNone())
	{
		Out.Add(Current);
	}
	return Out;
}

int32 UFXR_TrainingSession::GetStepsCompleted() const
{
	int32 Completed = 0;
	const int32 Total = Runner.GetSteps().Num();
	for (int32 Index = 0; Index < Total; ++Index)
	{
		if (Runner.GetStatus(Index) == EFXR_StepStatus::Complete)
		{
			++Completed;
		}
	}
	return Completed;
}

FText UFXR_TrainingSession::GetActiveInstruction() const
{
	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	const FFXR_StepInfo* Info = Steps ? Steps->FindStep(GetCurrentStepId()) : nullptr;
	return Info ? Info->Instruction : FText::GetEmpty();
}

FText UFXR_TrainingSession::GetProcedureName() const
{
	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	if (!Steps)
	{
		return FText::GetEmpty();
	}

	// An unnamed procedure is far more likely to be unfinished authoring than a choice, and a blank
	// title on a wall reads as a bug. The asset name is a poor title and a useful one.
	return Steps->DisplayName.IsEmpty() ? FText::FromName(Steps->GetFName()) : Steps->DisplayName;
}

FText UFXR_TrainingSession::GetCourseName() const
{
	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	return Steps ? Steps->CourseName : FText::GetEmpty();
}

FFXR_SessionReport UFXR_TrainingSession::BuildReport() const
{
	FFXR_SessionReport Report;
	Report.GraphName = GM ? GM->GetFName() : NAME_None;
	Report.ProcedureName = GetProcedureName();
	Report.CourseName = GetCourseName();
	Report.Mode = Runner.GetMode();
	Report.FailReason = Runner.GetFailReason();
	Report.MistakeLimit = Runner.GetMaxMistakes();
	Report.DurationSeconds = Runner.GetElapsedSeconds();
	Report.Mistakes = Runner.GetMistakes();

	// Ordered so the sharper answer wins: a failed run has also not reached the end, and a report
	// taken mid-session is neither. A run with no steps never started.
	if (Runner.Failed())
	{
		Report.Outcome = EFXR_SessionOutcome::Failed;
	}
	else if (Runner.ReachedEnd())
	{
		Report.Outcome = EFXR_SessionOutcome::Completed;
	}
	else if (Runner.IsRunning() || Runner.GetSteps().IsEmpty())
	{
		Report.Outcome = EFXR_SessionOutcome::InProgress;
	}
	else
	{
		Report.Outcome = EFXR_SessionOutcome::Stopped;
	}

	const UFXR_TrainingSteps* StepsAsset = GetStepsAsset();
	const TArray<FFXR_CompiledStep>& Steps = Runner.GetSteps();
	Report.StepsTotal = Steps.Num();
	Report.Steps.Reserve(Steps.Num());

	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		FFXR_StepRecord& Record = Report.Steps.AddDefaulted_GetRef();
		Record.StepId = Steps[Index].StepId;
		Record.Status = Runner.GetStatus(Index);
		Record.OpenedAtSeconds = Runner.GetStepOpenedAt(Index);
		Record.ClosedAtSeconds = Runner.GetStepClosedAt(Index);
		Record.MistakeCount = Runner.GetStepMistakeCount(Index);

		if (Record.OpenedAtSeconds >= 0.f && Record.ClosedAtSeconds >= 0.f)
		{
			Record.DurationSeconds = Record.ClosedAtSeconds - Record.OpenedAtSeconds;
		}

		if (const FFXR_StepInfo* Info = StepsAsset ? StepsAsset->FindStep(Record.StepId) : nullptr)
		{
			Record.DisplayName = Info->DisplayName;
			Record.Instruction = Info->Instruction;
		}

		if (Record.Status == EFXR_StepStatus::Complete)
		{
			++Report.StepsCompleted;
		}
	}

	float Score = Scoring.StartingScore;
	for (const FFXR_Mistake& Mistake : Report.Mistakes)
	{
		switch (Mistake.Kind)
		{
		case EFXR_MistakeKind::WrongAction:
			Score -= Scoring.WrongAction;
			break;
		case EFXR_MistakeKind::OutOfOrder:
			Score -= Scoring.OutOfOrder;
			break;
		case EFXR_MistakeKind::Timeout:
			Score -= Scoring.Timeout;
			break;
		}
	}

	// The score is earned by finishing, and it is an Exam measure: Training cannot go wrong, so the
	// panel shows a tick there instead. A failed run scores nothing - partial credit on a failed exam
	// is a mixed message - and a stopped run is not a verdict, so it has no score either. A run still
	// going keeps its provisional score for anything watching it live.
	const bool bEarned = Report.Outcome == EFXR_SessionOutcome::Completed
		|| Report.Outcome == EFXR_SessionOutcome::InProgress;
	Report.Score = bEarned ? FMath::Max(0.f, Score) : 0.f;

	if (Report.Outcome == EFXR_SessionOutcome::Failed && Report.Steps.IsValidIndex(Runner.GetFailedStep()))
	{
		const FFXR_StepRecord& Failed = Report.Steps[Runner.GetFailedStep()];
		Report.FailedStepId = Failed.StepId;
		Report.FailedStepName = Failed.DisplayName.IsEmpty() ? FText::FromName(Failed.StepId) : Failed.DisplayName;
	}

	return Report;
}
