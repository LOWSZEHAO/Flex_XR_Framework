// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Data/FXR_TrainingGM.h"

#include "Data/FXR_TrainingSteps.h"

const FFXR_GMStep* UFXR_TrainingGM::FindSetup(FName Id) const
{
	return StepSetup.FindByPredicate([Id](const FFXR_GMStep& Setup) { return Setup.StepId == Id; });
}

TArray<FString> UFXR_TrainingGM::GetStepIdOptions() const
{
	return Steps ? Steps->GetCompletableIdStrings() : TArray<FString>();
}

bool UFXR_TrainingGM::Compile(TArray<FFXR_CompiledStep>& OutEntries, TArray<TArray<FFXR_StepAction>>& OutEvents,
	TArray<FString>& OutErrors) const
{
	OutEntries.Reset();
	OutEvents.Reset();
	OutErrors.Reset();

	if (!Steps)
	{
		OutErrors.Add(TEXT("No Steps asset is set, so there is nothing to run."));
		return false;
	}

	OutEntries.Reserve(Steps->Steps.Num());
	OutEvents.Reserve(Steps->Steps.Num());

	// One entry, with its setup copied in. Copied, not pointed at: the session keeps these for the
	// whole run, and an asset edited during play must not leave it holding pointers into an array
	// that has moved.
	auto AddEntry = [this, &OutEntries, &OutEvents, &OutErrors](FFXR_CompiledStep&& Entry)
	{
		const FName Id = Entry.GetCompletionId();
		const FFXR_GMStep* Setup = Id.IsNone() ? nullptr : FindSetup(Id);
		if (!Setup && !Id.IsNone())
		{
			OutErrors.Add(FString::Printf(
				TEXT("'%s' has no setup in %s: it fires no events, and Exam has no objects to judge it by."),
				*Id.ToString(), *GetName()));
		}

		TArray<FFXR_StepAction>& Events = OutEvents.AddDefaulted_GetRef();
		if (Setup)
		{
			Entry.Objects = Setup->StepObjects;
			Entry.TimeoutSeconds = Setup->TimeoutSeconds;
			Entry.bCritical = Setup->bCritical;
			Events = Setup->Events;
		}

		OutEntries.Add(MoveTemp(Entry));
	};

	// Steps and sub-steps share one set of ids. Complete Step and the GM both name things by id
	// alone, so an id that meant two things would leave both of them guessing.
	TSet<FName> Seen;

	for (int32 Index = 0; Index < Steps->Steps.Num(); ++Index)
	{
		const FFXR_StepInfo& Info = Steps->Steps[Index];

		if (Info.StepId.IsNone())
		{
			// Still run: Complete Step with no id finishes whatever is open, so the procedure can
			// get past it. But nothing in the GM can be aimed at it, and that is worth saying.
			OutErrors.Add(FString::Printf(TEXT("Step %d in %s has no Step Id, so nothing in the GM can be set up for it."),
				Index, *Steps->GetName()));
		}
		else if (Seen.Contains(Info.StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("Id '%s' is used twice in %s; the second step is skipped."),
				*Info.StepId.ToString(), *Steps->GetName()));
			continue;
		}

		Seen.Add(Info.StepId);

		if (Info.SubSteps.IsEmpty())
		{
			FFXR_CompiledStep Entry;
			Entry.StepId = Info.StepId;
			Entry.StepIndex = Index;
			AddEntry(MoveTemp(Entry));
			continue;
		}

		const int32 EntriesBefore = OutEntries.Num();

		for (int32 SubIndex = 0; SubIndex < Info.SubSteps.Num(); ++SubIndex)
		{
			const FFXR_SubStepInfo& Sub = Info.SubSteps[SubIndex];

			if (Sub.SubStepId.IsNone())
			{
				OutErrors.Add(FString::Printf(TEXT("Sub-step %d of '%s' has no Sub Step Id, so nothing in the GM can be set up for it."),
					SubIndex, *Info.StepId.ToString()));
			}
			else if (Seen.Contains(Sub.SubStepId))
			{
				OutErrors.Add(FString::Printf(TEXT("Id '%s' is used twice in %s; the sub-step of '%s' is skipped."),
					*Sub.SubStepId.ToString(), *Steps->GetName(), *Info.StepId.ToString()));
				continue;
			}

			Seen.Add(Sub.SubStepId);

			FFXR_CompiledStep Entry;
			Entry.StepId = Info.StepId;
			Entry.StepIndex = Index;
			Entry.SubStepId = Sub.SubStepId;
			Entry.SubStepIndex = SubIndex;
			AddEntry(MoveTemp(Entry));
		}

		// Every sub-step was a duplicate, so the step has nothing left in it to complete and simply
		// does not appear in the run. Said, because it is a step the trainee will never see.
		if (OutEntries.Num() == EntriesBefore)
		{
			OutErrors.Add(FString::Printf(TEXT("Step '%s' has no usable sub-steps and is skipped."), *Info.StepId.ToString()));
		}
	}

	// The drift the Step Id dropdown exists to prevent, caught for anything that predates it or
	// was typed by hand: setup the run will never use.
	TSet<FName> Completable;
	for (const FFXR_CompiledStep& Entry : OutEntries)
	{
		Completable.Add(Entry.GetCompletionId());
	}

	TSet<FName> SetupSeen;
	for (const FFXR_GMStep& Setup : StepSetup)
	{
		if (!Completable.Contains(Setup.StepId))
		{
			const FFXR_StepInfo* Parent = Steps->FindStep(Setup.StepId);
			OutErrors.Add(Parent && !Parent->SubSteps.IsEmpty()
				? FString::Printf(TEXT("%s sets up '%s', which has sub-steps; set up each of them instead. This entry never fires."),
					*GetName(), *Setup.StepId.ToString())
				: FString::Printf(TEXT("%s has setup for '%s', which is not a step or sub-step in %s; it never fires."),
					*GetName(), *Setup.StepId.ToString(), *Steps->GetName()));
		}
		else if (SetupSeen.Contains(Setup.StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s sets up '%s' twice; only the first is used."),
				*GetName(), *Setup.StepId.ToString()));
		}

		SetupSeen.Add(Setup.StepId);
	}

	return !OutEntries.IsEmpty();
}
