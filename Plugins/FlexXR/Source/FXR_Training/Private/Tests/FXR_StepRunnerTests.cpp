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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_StepRunnerStepTimeoutTest,
	"FlexXR.Training.Runner.StepTimeoutSpansSubSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_StepRunnerStepTimeoutTest::RunTest(const FString& Parameters)
{
	using namespace FXR_StepRunnerTests;

	// A step given ten seconds as a whole, in two parts with no limit of their own.
	TArray<FFXR_CompiledStep> Entries;
	Entries.Add(Entry(TEXT("Prepare"), 0, TEXT("Lift"), 0));
	Entries.Add(Entry(TEXT("Prepare"), 0, TEXT("Pin"), 1));
	Entries.Add(Entry(TEXT("Spray"), 1));
	Entries[0].StepTimeoutSeconds = 10.f;
	Entries[1].StepTimeoutSeconds = 10.f;

	FFXR_StepRunner Runner;
	Runner.Start(MoveTemp(Entries), EFXR_DeliveryMode::Exam);

	Runner.Tick(6.f);
	Runner.CompleteStep(TEXT("Lift"));
	Runner.Tick(3.f);
	TestTrue(TEXT("Nine seconds into a ten-second step is still in time"), Runner.IsRunning());

	Runner.Tick(2.f);
	TestTrue(TEXT("The step's clock does not restart with its second part"), Runner.Failed());
	TestEqual(TEXT("... it fails on time"), Runner.GetFailReason(), EFXR_FailReason::Timeout);
	TestEqual(TEXT("... on the part that was open"), Runner.GetFailedEntry(), 1);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFXR_StepActionTimingTest,
	"FlexXR.Training.GM.EventTiming",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FFXR_StepActionTimingTest::RunTest(const FString& Parameters)
{
	auto Make = [](bool bAutoStart, bool bExamOnly)
	{
		FFXR_StepAction Action;
		Action.bAutoStartInExam = bAutoStart;
		Action.bExamOnly = bExamOnly;
		return Action;
	};

	constexpr EFXR_DeliveryMode Training = EFXR_DeliveryMode::Training;
	constexpr EFXR_DeliveryMode Exam = EFXR_DeliveryMode::Exam;

	const FFXR_StepAction Plain = Make(false, false);
	TestTrue(TEXT("Plain: Training fires it as its step arrives"), Plain.FiresOnArrival(Training));
	TestTrue(TEXT("Plain: so does Exam"), Plain.FiresOnArrival(Exam));
	TestFalse(TEXT("Plain: not at the start of an Exam"), Plain.FiresAtExamStart());

	const FFXR_StepAction AutoStart = Make(true, false);
	TestTrue(TEXT("Auto Start: Training fires it as its step arrives"), AutoStart.FiresOnArrival(Training));
	TestFalse(TEXT("Auto Start: Exam does not fire it again at its step"), AutoStart.FiresOnArrival(Exam));
	TestTrue(TEXT("Auto Start: Exam fires it at the start"), AutoStart.FiresAtExamStart());

	const FFXR_StepAction ExamOnly = Make(false, true);
	TestFalse(TEXT("Exam Only: Training never fires it"), ExamOnly.FiresOnArrival(Training));
	TestTrue(TEXT("Exam Only: Exam fires it as its step arrives"), ExamOnly.FiresOnArrival(Exam));
	TestFalse(TEXT("Exam Only: not at the start"), ExamOnly.FiresAtExamStart());

	const FFXR_StepAction Decoy = Make(true, true);
	TestFalse(TEXT("Both: Training never fires it"), Decoy.FiresOnArrival(Training));
	TestFalse(TEXT("Both: Exam does not fire it at its step"), Decoy.FiresOnArrival(Exam));
	TestTrue(TEXT("Both: Exam fires it at the start"), Decoy.FiresAtExamStart());

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

	TestEqual(TEXT("The step dropdown offers steps only"), Steps->GetStepIdStrings(),
		TArray<FString>({ TEXT("A"), TEXT("B"), TEXT("C"), TEXT("D") }));
	TestEqual(TEXT("A step's sub-step dropdown offers its own sub-steps"), Steps->GetSubStepIdStrings(TEXT("B")),
		TArray<FString>({ TEXT("B1"), TEXT("B2") }));

	UFXR_TrainingGM* GM = NewObject<UFXR_TrainingGM>();
	GM->Steps = Steps;

	auto Action = [](FName EventName)
	{
		FFXR_StepAction Out;
		Out.EventName = EventName;
		return Out;
	};

	GM->StepSetup.AddDefaulted_GetRef().StepId = TEXT("A");
	GM->StepSetup.Last().StepObjects.Add(TEXT("Gun"));

	// B is set up as a whole and part by part.
	FFXR_GMStep& B = GM->StepSetup.AddDefaulted_GetRef();
	B.StepId = TEXT("B");
	B.StepObjects.Add(TEXT("Extinguisher"));
	B.TimeoutSeconds = 30.f;
	B.Events.Add(Action(TEXT("OpenCabinet")));

	FFXR_GMSubStep& B1 = B.SubSteps.AddDefaulted_GetRef();
	B1.SubStepId = TEXT("B1");
	B1.StepObjects.Add(TEXT("Bracket"));
	B1.Events.Add(Action(TEXT("EnableBracket")));

	FFXR_GMSubStep& B2 = B.SubSteps.AddDefaulted_GetRef();
	B2.SubStepId = TEXT("B2");
	B2.StepObjects.Add(TEXT("Pin"));
	B2.bCritical = true;
	B2.Events.Add(Action(TEXT("EnablePin")));

	B.SubSteps.AddDefaulted_GetRef().SubStepId = TEXT("D");   // a step's id, under B

	GM->StepSetup.AddDefaulted_GetRef().StepId = TEXT("Nowhere");

	TArray<FFXR_CompiledStep> Entries;
	TArray<TArray<FFXR_StepAction>> Events;
	TArray<FString> Errors;
	TestTrue(TEXT("Compiles"), GM->Compile(Entries, Events, Errors));

	if (!TestEqual(TEXT("A, B1, B2 and D - C has nothing left to complete"), Entries.Num(), 4)
		|| !TestEqual(TEXT("Events stay parallel to the entries"), Events.Num(), Entries.Num()))
	{
		return false;
	}

	TestEqual(TEXT("A is a whole step"), Entries[0].GetCompletionId(), FName(TEXT("A")));
	TestFalse(TEXT("... not a sub-step"), Entries[0].IsSubStep());

	TestEqual(TEXT("B1 completes by its own id"), Entries[1].GetCompletionId(), FName(TEXT("B1")));
	TestEqual(TEXT("... and belongs to B"), Entries[1].StepId, FName(TEXT("B")));
	TestEqual(TEXT("B1 and B2 share B's index"), Entries[1].StepIndex, Entries[2].StepIndex);
	TestEqual(TEXT("B2 is B's second sub-step"), Entries[2].SubStepIndex, 1);
	TestEqual(TEXT("D keeps its index in the Steps asset"), Entries[3].StepIndex, 3);

	TestEqual(TEXT("B1 holds B's objects and its own"), Entries[1].Objects,
		TArray<FName>({ TEXT("Extinguisher"), TEXT("Bracket") }));
	TestEqual(TEXT("B2 holds B's objects and its own"), Entries[2].Objects,
		TArray<FName>({ TEXT("Extinguisher"), TEXT("Pin") }));
	TestFalse(TEXT("B1 is not critical"), Entries[1].bCritical);
	TestTrue(TEXT("B2 is"), Entries[2].bCritical);
	TestEqual(TEXT("B's timeout covers its parts"), Entries[2].StepTimeoutSeconds, 30.f);
	TestEqual(TEXT("... and is not a part's own"), Entries[2].TimeoutSeconds, 0.f);

	auto Names = [](const TArray<FFXR_StepAction>& Actions)
	{
		TArray<FName> Out;
		for (const FFXR_StepAction& Action : Actions)
		{
			Out.Add(Action.EventName);
		}
		return Out;
	};

	TestEqual(TEXT("B's events fire with its first part, ahead of the part's own"), Names(Events[1]),
		TArray<FName>({ TEXT("OpenCabinet"), TEXT("EnableBracket") }));
	TestEqual(TEXT("... and only once"), Names(Events[2]), TArray<FName>({ TEXT("EnablePin") }));

	auto Reported = [&Errors](const TCHAR* Fragment)
	{
		return Errors.ContainsByPredicate([Fragment](const FString& Error) { return Error.Contains(Fragment); });
	};

	TestTrue(TEXT("Reports the reused id"), Reported(TEXT("is used twice")));
	TestTrue(TEXT("Reports the step left empty"), Reported(TEXT("no usable sub-steps")));
	TestTrue(TEXT("Reports a sub-step set up under the wrong step"), Reported(TEXT("not one of its sub-steps")));
	TestTrue(TEXT("Reports setup for an unknown step"), Reported(TEXT("which is not a step in")));
	TestTrue(TEXT("Reports a step with nothing set up"), Reported(TEXT("'D' has nothing set up")));

	return true;
}

#endif
