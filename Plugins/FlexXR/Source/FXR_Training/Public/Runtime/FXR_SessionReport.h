// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_SessionReport.generated.h"

/** What happened to one sub-step during a run. */
USTRUCT(BlueprintType)
struct FFXR_SubStepRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_StepStatus Status = EFXR_StepStatus::Pending;

	/** Seconds from the start of the session. Negative means it never opened. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float OpenedAtSeconds = -1.f;

	/** Negative means it never closed. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float ClosedAtSeconds = -1.f;

	/** Time it was open. Zero when it never opened or never closed. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float DurationSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 MistakeCount = 0;
};

/** What happened to one step during a run. */
USTRUCT(BlueprintType)
struct FFXR_StepRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** What the trainee was asked to do, kept so a report makes sense on its own. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText Instruction;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_StepStatus Status = EFXR_StepStatus::Pending;

	/** Seconds from the start of the session. Negative means the step never opened. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float OpenedAtSeconds = -1.f;

	/** Negative means it never closed (normal for a step that was still open at the end). */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float ClosedAtSeconds = -1.f;

	/** Time the step was open. Zero when it never opened or never closed. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float DurationSeconds = 0.f;

	/** Mistakes made while this step was open, including its sub-steps. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 MistakeCount = 0;

	/** Its sub-steps, in order. Empty for a step that has none. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_SubStepRecord> SubSteps;
};

/**
 * Score weights. Configurable because what counts as serious depends on the procedure: skipping a
 * lockout isn't the same as fumbling a latch.
 */
USTRUCT(BlueprintType)
struct FFXR_ScoringWeights
{
	GENERATED_BODY()

	/** Score at the start. Mistakes subtract from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float StartingScore = 100.f;

	/** Touched an exposed object no step uses. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float WrongAction = 10.f;

	/** Touched an object that belongs to a later step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float OutOfOrder = 15.f;

	/** Ran out of time on an open step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float Timeout = 5.f;

};

/**
 * Result of a run: what was done, in what order, how long it took, and the mistakes.
 */
USTRUCT(BlueprintType)
struct FFXR_SessionReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName GraphName;

	/**
	 * Procedure and course name, copied in so the report still makes sense after the session or
	 * level is gone.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText ProcedureName;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText CourseName;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;

	/** How the run ended. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_SessionOutcome Outcome = EFXR_SessionOutcome::InProgress;

	/** The step that ended the run, when Outcome is Failed. None otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName FailedStepId;

	/** That step's instruction, so a panel can name it without holding the assets. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText FailedStepName;

	/** The sub-step the run ended on, when the failed step has sub-steps. None otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName FailedSubStepId;

	/** That sub-step's text. Empty when FailedSubStepId is None. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText FailedSubStepName;

	/** What ended a failed run. None for any other outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_FailReason FailReason = EFXR_FailReason::None;

	/** The mistake limit the run had. 0 = none (always 0 in Training). */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 MistakeLimit = 0;

	/** True if the run was completed (not stopped or failed). */
	bool ReachedEnd() const { return Outcome == EFXR_SessionOutcome::Completed; }

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float DurationSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 StepsCompleted = 0;

	/** Number of steps in the procedure. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 StepsTotal = 0;


	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_StepRecord> Steps;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_Mistake> Mistakes;

	/** Clamped at 0. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float Score = 0.f;
};
