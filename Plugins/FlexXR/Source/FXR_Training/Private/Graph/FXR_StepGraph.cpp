// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Graph/FXR_StepGraph.h"

bool UFXR_StepGraph::Compile(TArray<FFXR_CompiledStep>& OutSteps, int32& OutEntryIndex, TArray<FString>& OutErrors) const
{
	OutSteps.Reset();
	OutEntryIndex = INDEX_NONE;
	OutErrors.Reset();

	// Index by name first so transitions can be resolved in one pass and the runner never has to
	// look a name up again.
	TMap<FName, int32> ByName;
	ByName.Reserve(Steps.Num());

	for (int32 Index = 0; Index < Steps.Num(); ++Index)
	{
		const FName StepId = Steps[Index].StepId;
		if (StepId.IsNone())
		{
			OutErrors.Add(FString::Printf(TEXT("Step %d has no Step Id."), Index));
			continue;
		}

		if (ByName.Contains(StepId))
		{
			OutErrors.Add(FString::Printf(TEXT("Step Id '%s' is used more than once; the later one is unreachable."), *StepId.ToString()));
			continue;
		}

		ByName.Add(StepId, Index);
	}

	// An object on the Always Wrong list that a step also needs is wrong everywhere except that
	// step - almost certainly a slip rather than the intent, so it is said out loud. Not fatal:
	// the step that needs it still completes on it, because completion is judged first.
	for (const FFXR_WrongActionDefinition& Wrong : AlwaysWrong)
	{
		if (Wrong.InteractionId.IsNone())
		{
			OutErrors.Add(TEXT("An Always Wrong entry has no Interaction Id."));
			continue;
		}

		for (const FFXR_StepDefinition& Definition : Steps)
		{
			if (Definition.Transitions.ContainsByPredicate([&Wrong](const FFXR_TransitionDefinition& Transition)
				{ return Transition.InteractionId == Wrong.InteractionId; }))
			{
				OutErrors.Add(FString::Printf(
					TEXT("'%s' is on the Always Wrong list but completes step '%s', so it counts as wrong at every other step."),
					*Wrong.InteractionId.ToString(), *Definition.StepId.ToString()));
			}
		}
	}

	OutSteps.Reserve(Steps.Num());
	for (const FFXR_StepDefinition& Definition : Steps)
	{
		FFXR_CompiledStep& Compiled = OutSteps.AddDefaulted_GetRef();
		Compiled.StepId = Definition.StepId;
		Compiled.TimeoutSeconds = Definition.TimeoutSeconds;
		Compiled.bHardLock = Definition.bHardLock;

		if (Definition.Transitions.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("Step '%s' has no transitions, so nothing can ever complete it."), *Definition.StepId.ToString()));
		}

		Compiled.Transitions.Reserve(Definition.Transitions.Num());
		for (const FFXR_TransitionDefinition& Transition : Definition.Transitions)
		{
			FFXR_CompiledTransition& Out = Compiled.Transitions.AddDefaulted_GetRef();
			Out.InteractionId = Transition.InteractionId;
			Out.Phase = Transition.Phase;

			if (Transition.InteractionId.IsNone())
			{
				OutErrors.Add(FString::Printf(TEXT("Step '%s' has a transition with no Interaction Id."), *Definition.StepId.ToString()));
			}

			Out.NextSteps.Reserve(Transition.NextSteps.Num());
			for (const FName NextName : Transition.NextSteps)
			{
				if (const int32* Found = ByName.Find(NextName))
				{
					Out.NextSteps.Add(*Found);
				}
				else
				{
					OutErrors.Add(FString::Printf(TEXT("Step '%s' points at '%s', which is not a step in this graph."),
						*Definition.StepId.ToString(), *NextName.ToString()));
				}
			}
		}

		Compiled.WrongActions.Reserve(Definition.WrongActions.Num() + AlwaysWrong.Num());
		for (const FFXR_WrongActionDefinition& Wrong : Definition.WrongActions)
		{
			FFXR_CompiledWrongAction& Out = Compiled.WrongActions.AddDefaulted_GetRef();
			Out.InteractionId = Wrong.InteractionId;
			Out.Message = Wrong.Message;
		}

		// The graph's own list, folded into every step at compile time so the runner never learns
		// it exists. Skipped where the step already names the object - its message is the more
		// specific - and where the object completes this step, which the list cannot mean here.
		for (const FFXR_WrongActionDefinition& Wrong : AlwaysWrong)
		{
			if (Wrong.InteractionId.IsNone())
			{
				continue;
			}

			const bool bNamedHere = Compiled.WrongActions.ContainsByPredicate(
				[&Wrong](const FFXR_CompiledWrongAction& Existing) { return Existing.InteractionId == Wrong.InteractionId; });

			const bool bCompletesHere = Definition.Transitions.ContainsByPredicate(
				[&Wrong](const FFXR_TransitionDefinition& Transition) { return Transition.InteractionId == Wrong.InteractionId; });

			if (bNamedHere || bCompletesHere)
			{
				continue;
			}

			FFXR_CompiledWrongAction& Out = Compiled.WrongActions.AddDefaulted_GetRef();
			Out.InteractionId = Wrong.InteractionId;
			Out.Message = Wrong.Message;
		}
	}

	if (const int32* Entry = ByName.Find(EntryStep))
	{
		OutEntryIndex = *Entry;
	}
	else
	{
		OutErrors.Add(EntryStep.IsNone()
			? TEXT("No Entry Step is set, so the procedure has no beginning.")
			: FString::Printf(TEXT("Entry Step '%s' is not a step in this graph."), *EntryStep.ToString()));
	}

	// A graph with a broken link still runs; one with no way in does not.
	return OutEntryIndex != INDEX_NONE;
}
