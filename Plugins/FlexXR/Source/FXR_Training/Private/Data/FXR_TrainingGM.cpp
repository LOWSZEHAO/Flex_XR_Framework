// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Data/FXR_TrainingGM.h"

#include "Data/FXR_TrainingSteps.h"

const FFXR_GMStep* UFXR_TrainingGM::FindSetup(FName StepId) const
{
	return StepSetup.FindByPredicate([StepId](const FFXR_GMStep& Setup) { return Setup.StepId == StepId; });
}

TArray<FString> UFXR_TrainingGM::GetStepIdOptions() const
{
	return Steps ? Steps->GetStepIdStrings() : TArray<FString>();
}

bool UFXR_TrainingGM::Compile(TArray<FFXR_CompiledStep>& OutSteps, TArray<TArray<FFXR_StepAction>>& OutEvents,
	TArray<FString>& OutErrors) const
{
	OutSteps.Reset();
	OutEvents.Reset();
	OutErrors.Reset();

	if (!Steps)
	{
		OutErrors.Add(TEXT("No Steps asset is set, so there is nothing to run."));
		return false;
	}

	TSet<FName> Seen;
	OutSteps.Reserve(Steps->Steps.Num());
	OutEvents.Reserve(Steps->Steps.Num());

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
			OutErrors.Add(FString::Printf(TEXT("Step Id '%s' is used twice in %s; the second is skipped."),
				*Info.StepId.ToString(), *Steps->GetName()));
			continue;
		}

		Seen.Add(Info.StepId);

		const FFXR_GMStep* Setup = Info.StepId.IsNone() ? nullptr : FindSetup(Info.StepId);
		if (!Setup && !Info.StepId.IsNone())
		{
			OutErrors.Add(FString::Printf(
				TEXT("Step '%s' has no setup in %s: it fires no events, and Exam has no objects to judge it by."),
				*Info.StepId.ToString(), *GetName()));
		}

		FFXR_CompiledStep& Out = OutSteps.AddDefaulted_GetRef();
		Out.StepId = Info.StepId;

		TArray<FFXR_StepAction>& Events = OutEvents.AddDefaulted_GetRef();

		if (Setup)
		{
			Out.Objects = Setup->StepObjects;
			Out.TimeoutSeconds = Setup->TimeoutSeconds;
			Out.bCritical = Setup->bCritical;

			// Copied, not pointed at. The session keeps these for the whole run, and an asset edited
			// during play must not leave it holding pointers into an array that has moved.
			Events = Setup->Events;
		}
	}

	// The drift the Step Id dropdown exists to prevent, caught for anything that predates it or
	// was typed by hand: setup for a step the Steps asset does not have, which would never fire.
	TSet<FName> SetupSeen;
	for (const FFXR_GMStep& Setup : StepSetup)
	{
		if (!Steps->FindStep(Setup.StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s has setup for '%s', which is not a step in %s; it never fires."),
				*GetName(), *Setup.StepId.ToString(), *Steps->GetName()));
		}
		else if (SetupSeen.Contains(Setup.StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("%s sets up '%s' twice; only the first is used."),
				*GetName(), *Setup.StepId.ToString()));
		}

		SetupSeen.Add(Setup.StepId);
	}

	return !OutSteps.IsEmpty();
}
