// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Data/FXR_TrainingGM.h"

#include "Data/FXR_TrainingSteps.h"

const FFXR_GMSubStep* FFXR_GMStep::FindSubSetup(FName SubStepId) const
{
	return SubSteps.FindByPredicate([SubStepId](const FFXR_GMSubStep& Setup) { return Setup.SubStepId == SubStepId; });
}

const FFXR_GMStep* UFXR_TrainingGM::FindSetup(FName StepId) const
{
	return StepSetup.FindByPredicate([StepId](const FFXR_GMStep& Setup) { return Setup.StepId == StepId; });
}

TArray<FString> UFXR_TrainingGM::GetStepIdOptions() const
{
	return Steps ? Steps->GetStepIdStrings() : TArray<FString>();
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

	// One entry, with its events beside it. Everything is copied rather than pointed at: the session
	// keeps these for the whole run, and an asset edited during play must not leave it holding
	// pointers into an array that has moved.
	auto AddEntry = [this, &OutEntries, &OutEvents, &OutErrors](FFXR_CompiledStep&& Entry, TArray<FFXR_StepAction>&& Events)
	{
		const FName Id = Entry.GetCompletionId();
		if (!Id.IsNone() && Events.IsEmpty() && Entry.Objects.IsEmpty())
		{
			OutErrors.Add(FString::Printf(
				TEXT("'%s' has nothing set up in %s: it fires no events, and Exam has no objects to judge it by."),
				*Id.ToString(), *GetName()));
		}

		OutEntries.Add(MoveTemp(Entry));
		OutEvents.Add(MoveTemp(Events));
	};

	// Steps and sub-steps share one set of ids. Complete Step names things by id alone, so an id that
	// meant two things would leave it guessing.
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

		const FFXR_GMStep* Setup = Info.StepId.IsNone() ? nullptr : FindSetup(Info.StepId);

		if (Info.SubSteps.IsEmpty())
		{
			FFXR_CompiledStep Entry;
			Entry.StepId = Info.StepId;
			Entry.StepIndex = Index;

			TArray<FFXR_StepAction> Events;
			if (Setup)
			{
				Entry.Objects = Setup->StepObjects;
				Entry.TimeoutSeconds = Setup->TimeoutSeconds;
				Entry.bCritical = Setup->bCritical;
				Events = Setup->Events;
			}

			AddEntry(MoveTemp(Entry), MoveTemp(Events));
			continue;
		}

		// A step with sub-steps runs as one entry per sub-step. The step's own setup is shared out
		// across them: its objects and Critical hold in every part, its timeout runs over all of
		// them, and its events fire once, as the step arrives - with the first part, ahead of that
		// part's own.
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

			TArray<FFXR_StepAction> Events;
			if (Setup)
			{
				Entry.Objects = Setup->StepObjects;
				Entry.StepTimeoutSeconds = Setup->TimeoutSeconds;
				Entry.bCritical = Setup->bCritical;

				if (OutEntries.Num() == EntriesBefore)
				{
					Events = Setup->Events;
				}
			}

			if (const FFXR_GMSubStep* SubSetup = (Setup && !Sub.SubStepId.IsNone()) ? Setup->FindSubSetup(Sub.SubStepId) : nullptr)
			{
				for (const FName Object : SubSetup->StepObjects)
				{
					Entry.Objects.AddUnique(Object);
				}

				Entry.TimeoutSeconds = SubSetup->TimeoutSeconds;
				Entry.bCritical |= SubSetup->bCritical;
				Events.Append(SubSetup->Events);
			}

			AddEntry(MoveTemp(Entry), MoveTemp(Events));
		}

		// Every sub-step was a duplicate, so the step has nothing left in it to complete and simply
		// does not appear in the run. Said, because it is a step the trainee will never see.
		if (OutEntries.Num() == EntriesBefore)
		{
			OutErrors.Add(FString::Printf(TEXT("Step '%s' has no usable sub-steps and is skipped."), *Info.StepId.ToString()));
		}
	}

	// The drift the dropdowns exist to prevent, caught for anything that predates them or was typed
	// by hand: setup the run will never use.
	TSet<FName> SetupSeen;
	for (const FFXR_GMStep& Setup : StepSetup)
	{
		const FFXR_StepInfo* Info = Steps->FindStep(Setup.StepId);

		if (!Info)
		{
			OutErrors.Add(FString::Printf(TEXT("%s has setup for '%s', which is not a step in %s; it never fires."),
				*GetName(), *Setup.StepId.ToString(), *Steps->GetName()));
			continue;
		}

		if (SetupSeen.Contains(Setup.StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s sets up '%s' twice; only the first is used."),
				*GetName(), *Setup.StepId.ToString()));
			continue;
		}

		SetupSeen.Add(Setup.StepId);

		TSet<FName> SubSeen;
		for (const FFXR_GMSubStep& SubSetup : Setup.SubSteps)
		{
			const bool bKnown = Info->SubSteps.ContainsByPredicate(
				[&SubSetup](const FFXR_SubStepInfo& Sub) { return Sub.SubStepId == SubSetup.SubStepId; });

			if (!bKnown)
			{
				OutErrors.Add(FString::Printf(TEXT("%s sets up '%s' under '%s', which is not one of its sub-steps; it never fires."),
					*GetName(), *SubSetup.SubStepId.ToString(), *Setup.StepId.ToString()));
			}
			else if (SubSeen.Contains(SubSetup.SubStepId))
			{
				OutErrors.Add(FString::Printf(TEXT("%s sets up '%s' twice; only the first is used."),
					*GetName(), *SubSetup.SubStepId.ToString()));
			}

			SubSeen.Add(SubSetup.SubStepId);
		}
	}

	return !OutEntries.IsEmpty();
}
