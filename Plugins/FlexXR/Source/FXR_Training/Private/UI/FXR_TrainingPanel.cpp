// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "UI/FXR_TrainingPanel.h"

#include "Components/FXR_TrainingSession.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Types/FXR_LogChannels.h"

UFXR_TrainingPanel::UFXR_TrainingPanel(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultMistakeMessage = NSLOCTEXT("FlexXR", "PanelDefaultMistake", "Not that one.");
}

void UFXR_TrainingPanel::NativeConstruct()
{
	Super::NativeConstruct();

	if (!Session.IsValid())
	{
		BindToSession(FindSessionInWorld());
	}
	else
	{
		// Already bound, so BindToSession will not run and the widgets have never been written.
		Refresh();
	}
}

void UFXR_TrainingPanel::NativeDestruct()
{
	// Unbinds and kills the dwell timer. A widget torn down mid-complaint must not come back on a
	// timer that outlived it.
	BindToSession(nullptr);

	Super::NativeDestruct();
}

void UFXR_TrainingPanel::BindToSession(UFXR_TrainingSession* InSession)
{
	if (UFXR_TrainingSession* Previous = Session.Get())
	{
		Previous->OnStepActivated.RemoveAll(this);
		Previous->OnMistake.RemoveAll(this);
		Previous->OnSessionFinished.RemoveAll(this);
	}

	Session = InSession;

	ClearMistake();
	Instruction = FText::GetEmpty();
	StepId = NAME_None;
	Report = FFXR_SessionReport();
	bFinished = false;

	if (!InSession)
	{
		Refresh();
		return;
	}

	InSession->OnStepActivated.AddDynamic(this, &UFXR_TrainingPanel::HandleStepActivated);
	InSession->OnMistake.AddDynamic(this, &UFXR_TrainingPanel::HandleMistake);
	InSession->OnSessionFinished.AddDynamic(this, &UFXR_TrainingPanel::HandleFinished);

	// A panel built after the run started would otherwise sit blank until the next step opened,
	// which on a long step is most of the time a trainee spends looking at it.
	if (InSession->IsRunning())
	{
		Instruction = InSession->GetActiveInstruction();

		const TArray<FName> Active = InSession->GetActiveStepIds();
		if (!Active.IsEmpty())
		{
			StepId = Active[0];
		}

		OnStepChanged(StepId, Instruction, InSession->GetStepsCompleted() + 1, InSession->GetStepTotal());
	}

	Refresh();
}

FText UFXR_TrainingPanel::GetProgressText() const
{
	const UFXR_TrainingSession* Current = Session.Get();
	if (!Current || Current->GetStepTotal() <= 0)
	{
		return FText::GetEmpty();
	}

	return FText::Format(NSLOCTEXT("FlexXR", "PanelProgress", "Step {0} / {1}"),
		FText::AsNumber(Current->GetStepsCompleted()),
		FText::AsNumber(Current->GetStepTotal()));
}

float UFXR_TrainingPanel::GetProgressFraction() const
{
	const UFXR_TrainingSession* Current = Session.Get();
	const float Total = Current ? static_cast<float>(Current->GetStepTotal()) : 0.f;
	if (Total <= 0.f)
	{
		return 0.f;
	}

	return static_cast<float>(Current->GetStepsCompleted()) / Total;
}

void UFXR_TrainingPanel::HandleStepActivated(FName InStepId, FText InInstruction)
{
	StepId = InStepId;
	Instruction = InInstruction;

	// A new step is its own answer to the last complaint.
	ClearMistake();

	const UFXR_TrainingSession* Current = Session.Get();
	const int32 Total = Current ? Current->GetStepTotal() : 0;

	// Completed plus one, not an index into the graph: a procedure that allows steps in any order
	// has no single current step, and what a trainee reads here is how far through they are.
	const int32 Number = Current ? Current->GetStepsCompleted() + 1 : 0;

	Refresh();
	OnStepChanged(StepId, Instruction, Number, Total);
}

void UFXR_TrainingPanel::HandleMistake(FFXR_Mistake Mistake)
{
	MistakeText = Mistake.Message.IsEmpty() ? DefaultMistakeMessage : Mistake.Message;

	Refresh();
	OnMistakeShown(Mistake, MistakeText);

	UWorld* World = GetWorld();
	if (MistakeDwellSeconds <= 0.f || !World)
	{
		return;
	}

	// Re-armed rather than stacked: a second slip extends the dwell instead of taking the message
	// down while the trainee is still making it.
	World->GetTimerManager().SetTimer(MistakeTimer, this, &UFXR_TrainingPanel::ClearMistake, MistakeDwellSeconds, false);
}

void UFXR_TrainingPanel::HandleFinished(FFXR_SessionReport InReport)
{
	Report = InReport;
	bFinished = true;

	Instruction = FText::GetEmpty();
	StepId = NAME_None;
	ClearMistake();

	Refresh();
	OnSessionComplete(Report);
}

void UFXR_TrainingPanel::ClearMistake()
{
	if (const UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(MistakeTimer);
	}

	// Silent when there was nothing up, so a bind or a step change does not fire a hide at a panel
	// that is already hiding it.
	if (MistakeText.IsEmpty())
	{
		return;
	}

	MistakeText = FText::GetEmpty();

	Refresh();
	OnMistakeCleared();
}

void UFXR_TrainingPanel::Refresh()
{
	// Every widget is optional, so each one is its own question. A designer's layout that keeps only
	// the instruction is a valid layout.
	if (InstructionLabel)
	{
		InstructionLabel->SetText(Instruction);
	}

	if (ProgressLabel)
	{
		ProgressLabel->SetText(GetProgressText());
	}

	if (ProgressFill)
	{
		ProgressFill->SetPercent(GetProgressFraction());
	}

	if (MistakeLabel)
	{
		MistakeLabel->SetText(MistakeText);
	}

	// Collapsed rather than Hidden: a hidden widget still takes its space, which leaves a hole where
	// the complaint was and moves everything below it.
	if (MistakeCard)
	{
		MistakeCard->SetVisibility(HasMistake() ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}

	if (ScoreLabel)
	{
		ScoreLabel->SetText(FText::AsNumber(FMath::RoundToInt(Report.Score)));
	}

	if (SummaryLabel)
	{
		SummaryLabel->SetText(BuildSummaryText());
	}

	if (CompletionCard)
	{
		CompletionCard->SetVisibility(bFinished ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

FText UFXR_TrainingPanel::BuildSummaryText() const
{
	const int32 Minutes = FMath::FloorToInt(Report.DurationSeconds / 60.f);
	const int32 Seconds = FMath::FloorToInt(Report.DurationSeconds) % 60;

	FNumberFormattingOptions Padded;
	Padded.MinimumIntegralDigits = 2;

	return FText::Format(
		NSLOCTEXT("FlexXR", "PanelSummary", "{0} of {1} steps     {2} mistakes     {3}:{4}"),
		FText::AsNumber(Report.StepsCompleted),
		FText::AsNumber(Report.StepsTotal),
		FText::AsNumber(Report.Mistakes.Num()),
		FText::AsNumber(Minutes),
		FText::AsNumber(Seconds, &Padded));
}

UFXR_TrainingSession* UFXR_TrainingPanel::FindSessionInWorld() const
{
	UWorld* World = GetWorld();
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

	// One session is the ordinary case and wiring a panel by hand is a step someone forgets. Two is
	// a decision the panel is not entitled to make silently.
	if (Found > 1)
	{
		UE_LOG(LogFXR, Warning,
			TEXT("%s found %d training sessions and bound the first. Call Bind To Session to choose one."),
			*GetName(), Found);
	}

	return First;
}
