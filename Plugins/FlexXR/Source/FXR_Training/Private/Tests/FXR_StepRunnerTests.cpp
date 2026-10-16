// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Data/FXR_TrainingGM.h"
#include "Data/FXR_TrainingSteps.h"
#include "Misc/AutomationTest.h"
#include "Runtime/FXR_StepRunner.h"

#if WITH_DEV_AUTOMATION_TESTS

// FFXR_StepRunner is free of UObject and world for exactly this: the order a procedure runs in and
// what every touch means are rules, and rules can be pinned down without a level running.

namespace FXR_StepRunnerTests
{
	FFXR_CompiledStep Entry(FName StepId, int32 StepIndex, FName SubStepId = NAME_None, int32 SubStepIndex = INDEX_NONE,
		TArray<FName> Objects = {}, bool bCritical = false)
	{
		FFXR_CompiledStep Out;
		Out.StepId = StepId;
		Out.StepIndex = StepIndex;
		Out.SubStepId = SubStepId;
		Out.SubStepIndex = SubStepIndex;
		Out.Objects = MoveTemp(Objects);
		Out.bCritical = bCritical;
		return Out;
	}

	FFXR_InteractionEvent Grab(FName InteractionId)
	{
		FFXR_InteractionEvent Event;
		Event.InteractionId = InteractionId;
		Event.Phase = EFXR_InteractionPhase::Began;
		return Event;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_StepRunnerSubStepOrderTest,
	"FlexXR.Training.Runner.SubStepsRunInOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_StepRunnerSubStepOrderTest::RunTest(const FString& Parameters)
{
	using namespace FXR_StepRunnerTests;

	// A, then B in three parts, then C.
	TArray<FFXR_CompiledStep> Entries;
	Entries.Add(Entry(TEXT("A"), 0));
	Entries.Add(Entry(TEXT("B"), 1, TEXT("B1"), 0));
	Entries.Add(Entry(TEXT("B"), 1, TEXT("B2"), 1));
	Entries.Add(Entry(TEXT("B"), 1, TEXT("B3"), 2));
	Entries.Add(Entry(TEXT("C"), 2));

	FFXR_StepRunner Runner;
	TArray<int32> Opened;
	TArray<int32> Closed;
	int32 Finished = 0;
	Runner.OnEntryActivated = [&Opened](int32 Index) { Opened.Add(Index); };
	Runner.OnEntryCompleted = [&Closed](int32 Index) { Closed.Add(Index); };
	Runner.OnFinished = [&Finished]() { ++Finished; };

	Runner.Start(MoveTemp(Entries), EFXR_DeliveryMode::Training);
	TestEqual(TEXT("Opens on the first entry"), Runner.GetCurrentEntry(), 0);

	TestFalse(TEXT("A sub-step cannot be ticked before its step opens"), Runner.CompleteStep(TEXT("B1")));
	TestTrue(TEXT("The open step finishes by its own id"), Runner.CompleteStep(TEXT("A")));
	TestEqual(TEXT("The first sub-step opens with its step"), Runner.GetCurrentEntry(), 1);

	TestFalse(TEXT("A step with sub-steps is not finished by its own id"), Runner.CompleteStep(TEXT("B")));
	TestFalse(TEXT("Sub-steps cannot be done out of order"), Runner.CompleteStep(TEXT("B2")));
	TestEqual(TEXT("A refused tick leaves the open sub-step open"), Runner.GetCurrentEntry(), 1);

	TestTrue(TEXT("The open sub-step ticks by its id"), Runner.CompleteStep(TEXT("B1")));
	TestTrue(TEXT("Then the next"), Runner.CompleteStep(TEXT("B2")));
	TestTrue(TEXT("No id ticks whatever is open"), Runner.CompleteStep(NAME_None));
	TestEqual(TEXT("The last sub-step hands over to the next step"), Runner.GetCurrentEntry(), 4);

	TestTrue(TEXT("The last step finishes"), Runner.CompleteStep(TEXT("C")));
	TestTrue(TEXT("The run reached its end"), Runner.ReachedEnd());
	TestEqual(TEXT("Finished once"), Finished, 1);
	TestEqual(TEXT("Every entry opened, in order"), Opened, TArray<int32>({ 0, 1, 2, 3, 4 }));
	TestEqual(TEXT("Every entry closed, in order"), Closed, TArray<int32>({ 0, 1, 2, 3, 4 }));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_StepRunnerSubStepJudgingTest,
	"FlexXR.Training.Runner.SubStepsAreJudged",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_StepRunnerSubStepJudgingTest::RunTest(const FString& Parameters)
{
	using namespace FXR_StepRunnerTests;

	// One step in three parts; the second is critical.
	TArray<FFXR_CompiledStep> Entries;
	Entries.Add(Entry(TEXT("Prepare"), 0, TEXT("Lift"), 0, { TEXT("Extinguisher") }));
	Entries.Add(Entry(TEXT("Prepare"), 0, TEXT("Pin"), 1, { TEXT("SafetyPin") }, true));
	Entries.Add(Entry(TEXT("Prepare"), 0, TEXT("Aim"), 2, { TEXT("Nozzle") }));

	FFXR_StepRunner Runner;
	Runner.Start(MoveTemp(Entries), EFXR_DeliveryMode::Exam);

	Runner.HandleEvent(Grab(TEXT("Extinguisher")));
	TestEqual(TEXT("The open sub-step's own object is not a mistake"), Runner.GetMistakes().Num(), 0);

	Runner.HandleEvent(Grab(TEXT("SafetyPin")));
	TestEqual(TEXT("A later sub-step's object is a mistake"), Runner.GetMistakes().Num(), 1);
	if (Runner.GetMistakes().Num() == 1)
	{
		const FFXR_Mistake& Mistake = Runner.GetMistakes()[0];
		TestEqual(TEXT("... out of order"), Mistake.Kind, EFXR_MistakeKind::OutOfOrder);
		TestEqual(TEXT("... on the step"), Mistake.StepId, FName(TEXT("Prepare")));
		TestEqual(TEXT("... and the sub-step that was open"), Mistake.SubStepId, FName(TEXT("Lift")));
	}
	TestTrue(TEXT("A non-critical sub-step does not fail on it"), Runner.IsRunning());

	Runner.CompleteStep(TEXT("Lift"));
	Runner.HandleEvent(Grab(TEXT("Extinguisher")));
	TestEqual(TEXT("A finished sub-step's object is ignored"), Runner.GetMistakes().Num(), 1);

	Runner.HandleEvent(Grab(TEXT("Decoy")));
	TestTrue(TEXT("A mistake on a critical sub-step fails the run"), Runner.Failed());
	TestEqual(TEXT("... as a critical mistake"), Runner.GetFailReason(), EFXR_FailReason::CriticalMistake);
	TestEqual(TEXT("... on that sub-step"), Runner.GetFailedEntry(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_TrainingGMCompileSubStepsTest,
	"FlexXR.Training.GM.CompilesSubSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_TrainingGMCompileSubStepsTest::RunTest(const FString& Parameters)
{
	UFXR_TrainingSteps* Steps = NewObject<UFXR_TrainingSteps>();

	auto AddStep = [Steps](FName StepId, TArray<FName> SubIds)
	{
		FFXR_StepInfo& Step = Steps->Steps.AddDefaulted_GetRef();
		Step.StepId = StepId;
		for (const FName SubId : SubIds)
		{
			Step.SubSteps.AddDefaulted_GetRef().SubStepId = SubId;
		}
	};

	AddStep(TEXT("A"), {});
	AddStep(TEXT("B"), { TEXT("B1"), TEXT("B2") });
	AddStep(TEXT("C"), { TEXT("A") });                 // its only sub-step reuses an id
	AddStep(TEXT("D"), {});

	TestEqual(TEXT("The dropdown offers what can be completed, once each"), Steps->GetCompletableIdStrings(),
		TArray<FString>({ TEXT("A"), TEXT("B1"), TEXT("B2"), TEXT("D") }));

	UFXR_TrainingGM* GM = NewObject<UFXR_TrainingGM>();
	GM->Steps = Steps;

	auto AddSetup = [GM](FName Id) -> FFXR_GMStep&
	{
		FFXR_GMStep& Setup = GM->StepSetup.AddDefaulted_GetRef();
		Setup.StepId = Id;
		return Setup;
	};

	AddSetup(TEXT("A"));
	AddSetup(TEXT("B1")).StepObjects.Add(TEXT("Extinguisher"));
	AddSetup(TEXT("B2")).bCritical = true;
	AddSetup(TEXT("B"));                               // set up as a whole, though it has sub-steps
	AddSetup(TEXT("Nowhere"));
	AddSetup(TEXT("D"));

	TArray<FFXR_CompiledStep> Entries;
	TArray<TArray<FFXR_StepAction>> Events;
	TArray<FString> Errors;
	TestTrue(TEXT("Compiles"), GM->Compile(Entries, Events, Errors));

	if (!TestEqual(TEXT("A, B1, B2 and D - C has nothing left to complete"), Entries.Num(), 4))
	{
		return false;
	}

	TestEqual(TEXT("Events stay parallel to the entries"), Events.Num(), Entries.Num());

	TestEqual(TEXT("A is a whole step"), Entries[0].GetCompletionId(), FName(TEXT("A")));
	TestFalse(TEXT("... not a sub-step"), Entries[0].IsSubStep());

	TestEqual(TEXT("B1 completes by its own id"), Entries[1].GetCompletionId(), FName(TEXT("B1")));
	TestEqual(TEXT("... and belongs to B"), Entries[1].StepId, FName(TEXT("B")));
	TestEqual(TEXT("B1 and B2 share B's index"), Entries[1].StepIndex, Entries[2].StepIndex);
	TestEqual(TEXT("B2 is B's second sub-step"), Entries[2].SubStepIndex, 1);
	TestEqual(TEXT("B1 carries its objects"), Entries[1].Objects, TArray<FName>({ TEXT("Extinguisher") }));
	TestTrue(TEXT("B2 carries its Critical flag"), Entries[2].bCritical);
	TestEqual(TEXT("D keeps its index in the Steps asset"), Entries[3].StepIndex, 3);

	auto Reported = [&Errors](const TCHAR* Fragment)
	{
		return Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment); });
	};

	TestTrue(TEXT("Reports the reused id"), Reported(TEXT("is used twice")));
	TestTrue(TEXT("Reports the step left empty"), Reported(TEXT("no usable sub-steps")));
	TestTrue(TEXT("Reports setup on a step that has sub-steps"), Reported(TEXT("which has sub-steps")));
	TestTrue(TEXT("Reports setup for an unknown id"), Reported(TEXT("not a step or sub-step")));

	return true;
}

#endif
