// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_TrainingTypes.generated.h"

/**
 * How much help the trainee gets. One graph, three dials: the procedure is authored once and the
 * mode decides what the session shows, never what it accepts. An Exam run and a Guided run score
 * the same actions identically, which is what makes the two comparable.
 */
UENUM(BlueprintType)
enum class EFXR_DeliveryMode : uint8
{
	/** Hints from the moment a step opens. Teaching. */
	Guided,

	/** Silent until something goes wrong, then help. Rehearsal. */
	Practice,

	/** No hints at all. Assessment. */
	Exam
};

/** What kind of wrong this was. The taxonomy is the point: "they failed" is not a finding. */
UENUM(BlueprintType)
enum class EFXR_MistakeKind : uint8
{
	/** Did something the step author explicitly called out as wrong here. */
	WrongAction,

	/** Did something that belongs to a step that has not opened yet. */
	OutOfOrder,

	/** Ran out of time on an open step. */
	Timeout
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
