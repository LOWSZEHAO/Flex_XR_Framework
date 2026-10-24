// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_TrainingTypes.generated.h"

/**
 * How a procedure is run. Both modes use the same Steps asset and GM.
 *
 * There used to be three (Guided / Practice / Exam). Guided and Practice let the trainee do things
 * ahead of the procedure, after which the steps and the world no longer matched, so I cut it down
 * to one mode that teaches and one that tests.
 */
UENUM(BlueprintType)
enum class EFXR_DeliveryMode : uint8
{
	/**
	 * Teaching. The GM's events open each step's objects as the step arrives, so only the current
	 * step can be done. Highlight and instructions on, no timeouts, no score.
	 */
	Training,

	/**
	 * Assessment. Events marked Auto Start In Exam fire at the start, so objects are open early and
	 * mistakes are possible. No hints. A critical step, a timeout or Max Mistakes fails the run.
	 */
	Exam
};

/** What kind of mistake it was. */
UENUM(BlueprintType)
enum class EFXR_MistakeKind : uint8
{
	/** Touched an exposed object that no step uses. */
	WrongAction,

	/** Touched an object that belongs to a step (or sub-step) that hasn't opened yet. */
	OutOfOrder,

	/** Ran out of time on an open step. */
	Timeout
};

/**
 * How a run ended. An enum rather than a bool so a failed exam and a session that was simply
 * stopped don't look the same in the report.
 */
UENUM(BlueprintType)
enum class EFXR_SessionOutcome : uint8
{
	/** Still running, or never started. */
	InProgress,

	/** The last step was completed. */
	Completed,

	/** Exam only: failed on a critical mistake, a timeout or Max Mistakes. */
	Failed,

	/** Stopped from outside (StopSession, end of play, level change). Not a result. */
	Stopped
};

/** Why a run failed. None unless the outcome is Failed. */
UENUM(BlueprintType)
enum class EFXR_FailReason : uint8
{
	None,

	/** A step stayed open past its Timeout Seconds. */
	Timeout,

	/** The run reached the session's Max Mistakes. */
	TooManyMistakes,

	/** A mistake was made while a Critical step was open. */
	CriticalMistake
};

/** Where a step is in the run. */
UENUM(BlueprintType)
enum class EFXR_StepStatus : uint8
{
	/** Not reached yet. */
	Pending,

	/** Open, waiting for Complete Step. */
	Active,

	/** Done. */
	Complete,

	/** Left over from the old branching graph. Nothing sets it now. */
	Skipped
};

/** One recorded mistake. */
USTRUCT(BlueprintType)
struct FFXR_Mistake
{
	GENERATED_BODY()

	/** The step that was open when it happened. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** The sub-step that was open, or None if the step has none. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/** The object the trainee touched. None for a timeout. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName InteractionId;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_MistakeKind Kind = EFXR_MistakeKind::WrongAction;

	/** Seconds since the session started. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float TimeSeconds = 0.f;

	/** Optional text for the panel. The runner currently leaves it empty. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText Message;
};
