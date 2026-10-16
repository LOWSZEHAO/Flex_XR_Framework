// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_TrainingTypes.generated.h"

/**
 * How a procedure is run. The same two assets serve both: the steps the trainee reads, and the GM
 * that says what happens at each one.
 *
 * Two modes rather than three. Guided and Practice differed only in when help appeared, and both
 * let the trainee change the world ahead of the procedure - attach the part early, close the valve
 * early - after which the steps and the world disagree. Teaching should not allow that, and
 * assessment should record it. So one mode does each.
 */
UENUM(BlueprintType)
enum class EFXR_DeliveryMode : uint8
{
	/**
	 * Teaching. The GM's events open each step's objects as the step arrives, so the trainee can
	 * only do what the procedure is on. The highlight and every instruction are shown, timeouts do
	 * nothing, and there is no score - a run that cannot go wrong has nothing to grade.
	 */
	Training,

	/**
	 * Assessment. Events marked Auto Start In Exam fire when the run begins, so the objects are
	 * open from the start and mistakes are possible. No hints, no instructions on the panel. A
	 * critical step, a timeout, or reaching Max Mistakes fails the run.
	 */
	Exam
};

/** What kind of wrong this was. The taxonomy is the point: "they failed" is not a finding. */
UENUM(BlueprintType)
enum class EFXR_MistakeKind : uint8
{
	/** Touched an object that is not this step's - or one the step named, with its own message. */
	WrongAction,

	/** Did something that belongs to a step that has not opened yet. */
	OutOfOrder,

	/** Ran out of time on an open step. */
	Timeout
};

/**
 * How a run ended.
 *
 * This replaces a bool, which could not tell a failure from an abandonment - and that difference is
 * the whole of what Exam mode means. A trainee who ran out of time and an instructor who closed the
 * level are not the same finding, and a report that calls both "did not reach the end" is useless
 * for the one question a training product is bought to answer.
 */
UENUM(BlueprintType)
enum class EFXR_SessionOutcome : uint8
{
	/** Still running, or never started. A mid-session report is allowed to say so. */
	InProgress,

	/** Ran to an end: a transition with nowhere left to go fired. */
	Completed,

	/** Ended against the trainee. Today that means an Exam step ran past its timeout. */
	Failed,

	/** Stopped from outside - StopSession, end of play, a level change. Not a verdict. */
	Stopped
};

/** Why a run failed. Only meaningful when the outcome is Failed. */
UENUM(BlueprintType)
enum class EFXR_FailReason : uint8
{
	None,

	/** An Exam step stayed open past its Timeout Seconds. */
	Timeout,

	/** An Exam run reached the session's Max Mistakes. */
	TooManyMistakes,

	/** A mistake was made while a step marked Critical was open. */
	CriticalMistake
};

/** Where a step sits in the run. */
UENUM(BlueprintType)
enum class EFXR_StepStatus : uint8
{
	/** Not reachable yet. */
	Pending,

	/** Open and accepting its completing event. */
	Active,

	/** Done. */
	Complete,

	/** Opened, then bypassed because a branch went elsewhere. */
	Skipped
};

/** One recorded error, with enough context to be readable in a report months later. */
USTRUCT(BlueprintType)
struct FFXR_Mistake
{
	GENERATED_BODY()

	/** The step that was open when this happened. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** Its sub-step that was open, or None when the step has no sub-steps. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/** What the trainee actually did. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FName InteractionId;

	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	EFXR_MistakeKind Kind = EFXR_MistakeKind::WrongAction;

	/** Seconds from the start of the session, so a report can be read as a timeline. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	float TimeSeconds = 0.f;

	/** Author's message for this mistake, shown when the mode allows it. */
	UPROPERTY(BlueprintReadOnly, Category = "FlexXR|Training")
	FText Message;
};
