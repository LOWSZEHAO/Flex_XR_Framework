// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/FXR_TrainingTypes.h"
#include "FXR_SessionReport.generated.h"

/** What happened to one step during a run. */
USTRUCT(BlueprintType)
struct FFXR_StepRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	/**
	 * What the trainee was asked to do. Carried into the report because a finding needs the
	 * question as well as the answer: "four of twelve reached for the handle first" only means
	 * something next to the instruction they were given.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText Instruction;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_StepStatus Status = EFXR_StepStatus::Pending;

	/** Seconds from the start of the session. Negative means the step never opened. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float OpenedAtSeconds = -1.f;

	/** Negative means it never closed, which is the normal state of a step still open at the end. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float ClosedAtSeconds = -1.f;

	/** Time the step was open. Zero when it never opened or never closed. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float DurationSeconds = 0.f;


	/** Mistakes made while this step was open. The per-step number is the interesting one. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 MistakeCount = 0;
};

/**
 * How a run turns into a number. Weights rather than a hardcoded formula, because what counts as
 * serious is the customer's judgement: skipping a lockout is not the same as fumbling a latch.
 */
USTRUCT(BlueprintType)
struct FFXR_ScoringWeights
{
	GENERATED_BODY()

	/** Everyone starts here and loses from it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float StartingScore = 100.f;

	/** Did something the author named as wrong for the open step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float WrongAction = 10.f;

	/** Did something belonging to a step that had not opened. Usually the more serious finding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float OutOfOrder = 15.f;

	/** Ran out of time on an open step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FlexXR|Training", meta = (ClampMin = "0.0"))
	float Timeout = 5.f;

};

/**
 * The output of a run: what was done, in what order, how long it took, and what went wrong.
 *
 * This is the thing a training product is actually bought for. A pass or fail is not a finding;
 * "four of twelve operators reached for the handle before the pin" is, and it only exists because
 * the world stayed live enough for them to do it.
 */
USTRUCT(BlueprintType)
struct FFXR_SessionReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName GraphName;

	/**
	 * The procedure and course as a person would read them.
	 *
	 * Carried on the report rather than looked up from the graph, so a finished run describes
	 * itself: a report that outlives its session - saved, exported, shown on a panel after the
	 * level changed - still knows what it was a report of.
	 */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText ProcedureName;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText CourseName;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_DeliveryMode Mode = EFXR_DeliveryMode::Training;

	/** What the run amounted to. See EFXR_SessionOutcome for why this is not a bool. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_SessionOutcome Outcome = EFXR_SessionOutcome::InProgress;

	/** The step that ended the run, when Outcome is Failed. None otherwise. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName FailedStepId;

	/** That step's label, so a panel can name it without holding the graph. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText FailedStepName;

	/** What ended a failed run. None for any other outcome. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_FailReason FailReason = EFXR_FailReason::None;

	/** The mistake limit the run was held to. Zero when there was none, which is every non-Exam run. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 MistakeLimit = 0;

	/** Ran to an end rather than being stopped or failed. Kept as the convenience it reads as. */
	bool ReachedEnd() const { return Outcome == EFXR_SessionOutcome::Completed; }

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float DurationSeconds = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 StepsCompleted = 0;

	/** Total authored steps. Completed can be lower than this without failure: a branch not taken. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	int32 StepsTotal = 0;


	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_StepRecord> Steps;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_Mistake> Mistakes;

	/** Clamped to zero at the bottom; a run cannot go negative however badly it went. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float Score = 0.f;
};
