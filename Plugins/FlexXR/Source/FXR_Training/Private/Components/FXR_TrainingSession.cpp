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

	// Normally there's one. With more than one, use the first and warn instead of guessing silently.
	if (Found > 1)
	{
		UE_LOG(LogFXRTraining, Warning,
			TEXT("Found %d training sessions and used the first. Call it on the session you mean instead."), Found);
	}

	return First;
}

// -----------------------------------------------------------------------------------------------
// Running
// -----------------------------------------------------------------------------------------------

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

	StepCount = 0;
	for (int32 Index = 0; Index < Compiled.Num(); ++Index)
	{
		if (Index == 0 || Compiled[Index - 1].StepIndex != Compiled[Index].StepIndex)
		{
			++StepCount;
		}
	}

	// Bound before Start, since Start opens the first entry. Step events wrap the sub-step ones:
	// step opens, then its first sub-step; last sub-step completes, then the step.
	Runner.OnEntryActivated = [this](int32 EntryIndex)
	{
		FireEntryEvents(EntryIndex);
		SetGuidance(EntryIndex, true);

		const FFXR_CompiledStep& Entry = Runner.GetEntries()[EntryIndex];

		if (IsFirstOfStep(EntryIndex))
		{
			const FFXR_StepInfo* Info = GetStepInfo(Entry);
			OnStepActivated.Broadcast(Entry.StepId, Info ? Info->Instruction : FText::GetEmpty());
		}

		if (Entry.IsSubStep())
		{
			OnSubStepActivated.Broadcast(Entry.StepId, Entry.SubStepId);
		}
	};

	Runner.OnEntryCompleted = [this](int32 EntryIndex)
	{
		SetGuidance(EntryIndex, false);

		const FFXR_CompiledStep& Entry = Runner.GetEntries()[EntryIndex];

		if (Entry.IsSubStep())
		{
			OnSubStepCompleted.Broadcast(Entry.StepId, Entry.SubStepId);
		}

		if (IsLastOfStep(EntryIndex))
		{
			OnStepCompleted.Broadcast(Entry.StepId, FText::GetEmpty());
		}
	};

	Runner.OnMistake = [this](const FFXR_Mistake& Mistake)
	{
		OnMistake.Broadcast(Mistake);
	};

	Runner.OnFinished = [this]()
	{
		// Build the report before clearing the highlight.
		const FFXR_SessionReport Report = BuildReport();
		ReleaseAll();
		OnSessionFinished.Broadcast(Report);
	};

	// Fire the Exam auto-start events before the first step, so its own events run on top of them.
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

// -----------------------------------------------------------------------------------------------
// GM events
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingSession::FireEntryEvents(int32 EntryIndex)
{
	if (!StepEvents.IsValidIndex(EntryIndex))
	{
		return;
	}

	for (const FFXR_StepAction& Action : StepEvents[EntryIndex])
	{
		if (Action.FiresOnArrival(Mode))
		{
			FireEvent(Action);
		}
	}
}

void UFXR_TrainingSession::FireExamStartEvents()
{
	for (const TArray<FFXR_StepAction>& Events : StepEvents)
	{
		for (const FFXR_StepAction& Action : Events)
		{
			if (Action.FiresAtExamStart())
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

void UFXR_TrainingSession::ValidateEvents(const TArray<FFXR_CompiledStep>& Entries) const
{
	const UWorld* World = GetWorld();

	for (int32 EntryIndex = 0; EntryIndex < StepEvents.Num(); ++EntryIndex)
	{
		const FString Step = Entries.IsValidIndex(EntryIndex)
			? Entries[EntryIndex].GetCompletionId().ToString()
			: FString::FromInt(EntryIndex);

		for (const FFXR_StepAction& Action : StepEvents[EntryIndex])
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

// -----------------------------------------------------------------------------------------------
// Highlight
// -----------------------------------------------------------------------------------------------

void UFXR_TrainingSession::SetGuidance(int32 EntryIndex, bool bOn)
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	if (!Entries.IsValidIndex(EntryIndex))
	{
		return;
	}

	UFXR_HighlightSubsystem* Highlight = UFXR_HighlightSubsystem::Get(this);
	if (!Highlight)
	{
		return;
	}

	// No highlight in Exam.
	const bool bWant = bOn && bHighlightCurrentStep && Mode == EFXR_DeliveryMode::Training;

	for (const FName Id : Entries[EntryIndex].Objects)
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

// -----------------------------------------------------------------------------------------------
// Report and queries
// -----------------------------------------------------------------------------------------------

bool UFXR_TrainingSession::IsFirstOfStep(int32 EntryIndex) const
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	return Entries.IsValidIndex(EntryIndex)
		&& (EntryIndex == 0 || Entries[EntryIndex - 1].StepIndex != Entries[EntryIndex].StepIndex);
}

bool UFXR_TrainingSession::IsLastOfStep(int32 EntryIndex) const
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	return Entries.IsValidIndex(EntryIndex)
		&& (EntryIndex == Entries.Num() - 1 || Entries[EntryIndex + 1].StepIndex != Entries[EntryIndex].StepIndex);
}

const FFXR_StepInfo* UFXR_TrainingSession::GetStepInfo(const FFXR_CompiledStep& Entry) const
{
	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	return Steps && Steps->Steps.IsValidIndex(Entry.StepIndex) ? &Steps->Steps[Entry.StepIndex] : nullptr;
}

const FFXR_SubStepInfo* UFXR_TrainingSession::GetSubStepInfo(const FFXR_CompiledStep& Entry) const
{
	const FFXR_StepInfo* Step = Entry.IsSubStep() ? GetStepInfo(Entry) : nullptr;
	return Step && Step->SubSteps.IsValidIndex(Entry.SubStepIndex) ? &Step->SubSteps[Entry.SubStepIndex] : nullptr;
}

FName UFXR_TrainingSession::GetCurrentStepId() const
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	const int32 Current = Runner.GetCurrentEntry();
	return Entries.IsValidIndex(Current) ? Entries[Current].StepId : NAME_None;
}

FName UFXR_TrainingSession::GetCurrentSubStepId() const
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	const int32 Current = Runner.GetCurrentEntry();
	return Entries.IsValidIndex(Current) ? Entries[Current].SubStepId : NAME_None;
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

int32 UFXR_TrainingSession::GetStepTotal() const
{
	if (!Runner.GetEntries().IsEmpty())
	{
		return StepCount;
	}

	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	return Steps ? Steps->Steps.Num() : 0;
}

int32 UFXR_TrainingSession::GetStepsCompleted() const
{
	int32 Completed = 0;
	const int32 Total = Runner.GetEntries().Num();
	for (int32 Index = 0; Index < Total; ++Index)
	{
		if (Runner.GetStatus(Index) == EFXR_StepStatus::Complete && IsLastOfStep(Index))
		{
			++Completed;
		}
	}
	return Completed;
}

FText UFXR_TrainingSession::GetActiveInstruction() const
{
	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	const int32 Current = Runner.GetCurrentEntry();
	const FFXR_StepInfo* Info = Entries.IsValidIndex(Current) ? GetStepInfo(Entries[Current]) : nullptr;
	return Info ? Info->Instruction : FText::GetEmpty();
}

FText UFXR_TrainingSession::GetProcedureName() const
{
	const UFXR_TrainingSteps* Steps = GetStepsAsset();
	if (!Steps)
	{
		return FText::GetEmpty();
	}

	// No name usually means it hasn't been filled in yet. Show the asset name instead of a blank title.
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

	// Order matters: a failed run also didn't reach the end, and a mid-run report is neither.
	if (Runner.Failed())
	{
		Report.Outcome = EFXR_SessionOutcome::Failed;
	}
	else if (Runner.ReachedEnd())
	{
		Report.Outcome = EFXR_SessionOutcome::Completed;
	}
	else if (Runner.IsRunning() || Runner.GetEntries().IsEmpty())
	{
		Report.Outcome = EFXR_SessionOutcome::InProgress;
	}
	else
	{
		Report.Outcome = EFXR_SessionOutcome::Stopped;
	}

	const TArray<FFXR_CompiledStep>& Entries = Runner.GetEntries();
	Report.StepsTotal = GetStepTotal();
	Report.Steps.Reserve(Report.StepsTotal);

	// Group entries back into steps. A step is complete when all its entries are. Its close time is
	// the close time of the furthest entry it reached (still -1 if stopped mid-step, the failure time
	// if it failed).
	bool bAllComplete = true;
	for (int32 Index = 0; Index < Entries.Num(); ++Index)
	{
		const FFXR_CompiledStep& Entry = Entries[Index];
		const EFXR_StepStatus Status = Runner.GetStatus(Index);

		if (IsFirstOfStep(Index))
		{
			FFXR_StepRecord& Fresh = Report.Steps.AddDefaulted_GetRef();
			Fresh.StepId = Entry.StepId;
			Fresh.OpenedAtSeconds = Runner.GetOpenedAt(Index);

			if (const FFXR_StepInfo* Info = GetStepInfo(Entry))
			{
				Fresh.Instruction = Info->Instruction;
			}

			bAllComplete = true;
		}

		FFXR_StepRecord& Record = Report.Steps.Last();
		Record.MistakeCount += Runner.GetMistakeCount(Index);
		bAllComplete &= Status == EFXR_StepStatus::Complete;

		if (Status != EFXR_StepStatus::Pending)
		{
			Record.Status = EFXR_StepStatus::Active;
			Record.ClosedAtSeconds = Runner.GetClosedAt(Index);
		}

		if (Entry.IsSubStep())
		{
			FFXR_SubStepRecord& Sub = Record.SubSteps.AddDefaulted_GetRef();
			Sub.SubStepId = Entry.SubStepId;
			Sub.Status = Status;
			Sub.OpenedAtSeconds = Runner.GetOpenedAt(Index);
			Sub.ClosedAtSeconds = Runner.GetClosedAt(Index);
			Sub.MistakeCount = Runner.GetMistakeCount(Index);

			if (Sub.OpenedAtSeconds >= 0.f && Sub.ClosedAtSeconds >= 0.f)
			{
				Sub.DurationSeconds = Sub.ClosedAtSeconds - Sub.OpenedAtSeconds;
			}

			if (const FFXR_SubStepInfo* Info = GetSubStepInfo(Entry))
			{
				Sub.DisplayName = Info->DisplayName;
			}
		}

		if (!IsLastOfStep(Index))
		{
			continue;
		}

		if (bAllComplete)
		{
			Record.Status = EFXR_StepStatus::Complete;
			++Report.StepsCompleted;
		}

		if (Record.OpenedAtSeconds >= 0.f && Record.ClosedAtSeconds >= 0.f)
		{
			Record.DurationSeconds = Record.ClosedAtSeconds - Record.OpenedAtSeconds;
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

	// Score only counts for a completed (or still running) run. Training shows a tick instead of a
	// score. A failed run scores 0 and a stopped run has no score.
	const bool bEarned = Report.Outcome == EFXR_SessionOutcome::Completed
		|| Report.Outcome == EFXR_SessionOutcome::InProgress;
	Report.Score = bEarned ? FMath::Max(0.f, Score) : 0.f;

	if (Report.Outcome == EFXR_SessionOutcome::Failed && Entries.IsValidIndex(Runner.GetFailedEntry()))
	{
		const FFXR_CompiledStep& Failed = Entries[Runner.GetFailedEntry()];

		const FFXR_StepInfo* Info = GetStepInfo(Failed);
		Report.FailedStepId = Failed.StepId;
		Report.FailedStepName = Info && !Info->Instruction.IsEmpty() ? Info->Instruction : FText::FromName(Failed.StepId);

		if (Failed.IsSubStep())
		{
			const FFXR_SubStepInfo* Sub = GetSubStepInfo(Failed);
			Report.FailedSubStepId = Failed.SubStepId;
			Report.FailedSubStepName = Sub && !Sub->DisplayName.IsEmpty() ? Sub->DisplayName : FText::FromName(Failed.SubStepId);
		}
	}

	return Report;
}
