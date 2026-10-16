// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Runtime/FXR_StepRunner.h"
#include "FXR_TrainingGM.generated.h"

class AActor;
class UFXR_TrainingSteps;

/** One custom event the GM calls on a Blueprint in the level. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_StepAction
{
	GENERATED_BODY()

	/** The Blueprint to call into. It is found in the level when the event fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TSoftClassPtr<AActor> Blueprint;

	/**
	 * Only needed when that Blueprint is placed more than once: the call then reaches just the
	 * copies carrying this Actor Tag. Left empty, every copy in the level is called.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName Tag;

	/** The custom event to call - picked from that Blueprint's own events. It must take no inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (DisplayName = "Event"))
	FName EventName;

	/**
	 * Exam only: fire when the run begins, instead of when this step arrives.
	 *
	 * This is how one flow serves both modes. Training opens each object as its step comes up, so
	 * nothing can be done early; Exam opens the ones marked here from the start, so the trainee can
	 * reach for them early - and get it wrong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bAutoStartInExam = false;
};

/** A message of its own for one wrong object. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_WrongObjectMessage
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName InteractionId;

	/** Shown in Training. Exam shows only "Mistake 1 of 3" - a message saying what went wrong is a hint. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText Message;
};

/** Everything that happens at one step. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_GMStep
{
	GENERATED_BODY()

	/** Which step. Picked from the Steps asset, so the two cannot quietly disagree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (GetOptions = "GetStepIdOptions"))
	FName StepId;

	/** Fired when this step arrives. In Exam, those marked Auto Start In Exam fire when the run begins instead. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "EventName"))
	TArray<FFXR_StepAction> Events;

	/**
	 * The Interaction Ids this step is about - the gun, or the stick and the socket it goes into.
	 *
	 * Never judged wrong while this step is open. Highlighted in Training. And what Exam judges
	 * against: touching one before its step is out of order, and touching one after is ignored,
	 * because procedures reuse things.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> Objects;

	/**
	 * Optional messages for specific wrong objects while this step is open. Every exposed object
	 * that is not this step's is already wrong without being listed - this only gives one a line
	 * of its own instead of "Wrong object."
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "InteractionId"))
	TArray<FFXR_WrongObjectMessage> WrongObjectMessages;

	/** Exam: the run fails if this step is still open after this many seconds. Zero is no limit. Ignored in Training. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/** Exam: any mistake made while this step is open fails the run at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bCritical = false;
};

/**
 * UFXR_TrainingGM — what happens at each step of a procedure.
 *
 * The other half of UFXR_TrainingSteps, which it points at. The steps say what the trainee reads;
 * this says which custom events fire as each step arrives, which objects the step is about, and the
 * Exam rules. Finishing a step is not described here at all: the project calls Complete Step from
 * whatever event means "done" - a grab, a latch reaching closed, a button - which is how a step can
 * finish on something the framework could never have known to watch for.
 *
 * Kept as data rather than a Blueprint graph so a procedure reads top to bottom, step by step, and
 * so the framework can check it - an event name that does not exist, a step one asset has and the
 * other does not - before the run starts rather than halfway through it.
 */
UCLASS(BlueprintType)
class FXR_TRAINING_API UFXR_TrainingGM : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The procedure's steps, as the trainee reads them. Their order is the order the run takes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TObjectPtr<UFXR_TrainingSteps> Steps;

	/** One entry per step. Each row is titled with its Step Id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "StepId"))
	TArray<FFXR_GMStep> StepSetup;

	/**
	 * Turn the two assets into what the runner consumes, in the Steps asset's order, plus each
	 * step's events. Reports every disagreement between the two rather than failing quietly.
	 *
	 * Returns false when there is nothing to run. OutErrors is filled either way.
	 */
	bool Compile(TArray<FFXR_CompiledStep>& OutSteps, TArray<TArray<FFXR_StepAction>>& OutEvents,
		TArray<FString>& OutErrors) const;

	/** The setup for this step, or null. */
	const FFXR_GMStep* FindSetup(FName StepId) const;

	/** The Steps asset's ids, for the Step Id dropdown. */
	UFUNCTION()
	TArray<FString> GetStepIdOptions() const;
};
