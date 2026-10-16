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
	 * In Exam, fire when the run begins instead of when this step arrives. Training still fires it
	 * when the step arrives, unless Exam Only is ticked too.
	 *
	 * This is how one flow serves both modes. Training opens each object as its step comes up, so
	 * nothing can be done early; Exam opens the ones marked here from the start, so the trainee can
	 * reach for them early - and get it wrong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bAutoStartInExam = false;

	/**
	 * Never fire in Training. For what only an exam should have - a decoy to reach for, a
	 * distraction - which Training, opening one right object at a time, must never offer. With Auto
	 * Start In Exam ticked as well it fires when an Exam begins; without, when its step arrives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bExamOnly = false;

	/** Whether this fires when its step arrives, in this mode. */
	bool FiresOnArrival(EFXR_DeliveryMode Mode) const
	{
		// Exam fired the Auto Start ones when the run began; Training never fires the Exam Only ones.
		return Mode == EFXR_DeliveryMode::Exam ? !bAutoStartInExam : !bExamOnly;
	}

	/** Whether this fires when an Exam run begins. Training has no such moment. */
	bool FiresAtExamStart() const { return bAutoStartInExam; }
};

/** Everything that happens at one sub-step, on top of what its step sets up. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_GMSubStep
{
	GENERATED_BODY()

	/** Which sub-step. Picked from this step's own sub-steps in the Steps asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/**
	 * Fired when this sub-step arrives - which is how each part of a step opens its own objects. In
	 * Exam, those marked Auto Start In Exam fire when the run begins instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "EventName"))
	TArray<FFXR_StepAction> Events;

	/** The Interaction Ids of the objects this sub-step is about, added to its step's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> StepObjects;

	/** Exam: the run fails if this sub-step is still open after this many seconds. Zero is no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/** Exam: any mistake made while this sub-step is open fails the run at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bCritical = false;
};

/** Everything that happens at one step - and, under it, at each of its sub-steps. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_GMStep
{
	GENERATED_BODY()

	/** Which step. Picked from the Steps asset, so the two cannot quietly disagree. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (GetOptions = "GetStepIdOptions"))
	FName StepId;

	/**
	 * Fired when this step arrives - for a step with sub-steps, just before its first sub-step's own.
	 * In Exam, those marked Auto Start In Exam fire when the run begins instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "EventName"))
	TArray<FFXR_StepAction> Events;

	/**
	 * The Interaction Ids of the objects this step is about - the gun, or the stick and the socket it
	 * goes into. Not Step Ids: an Interaction Id names an object in the level, and one object can
	 * belong to several steps. For a step with sub-steps, these belong to every one of them.
	 *
	 * Two jobs. In Training they get the highlight. In Exam they are how the judge tells the right
	 * object from a wrong one: touching one of these while its step is open is never a mistake,
	 * touching one before its step is out of order, and touching one after is ignored, because
	 * procedures reuse things. Leave a step's list empty and grabbing its object counts as wrong.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> StepObjects;

	/**
	 * Exam: the run fails if this step is still open after this many seconds - all of it, sub-steps
	 * included. Zero is no limit. Ignored in Training.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/** Exam: any mistake made while this step is open - in any of its sub-steps - fails the run at once. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bCritical = false;

	/**
	 * What happens at each of this step's sub-steps, when the Steps asset gives it any. Set each one
	 * up to open its own objects as it arrives, so the pin cannot be pulled while the extinguisher is
	 * still on the wall. A sub-step left out runs on the step's setup alone.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "SubStepId"))
	TArray<FFXR_GMSubStep> SubSteps;

	/** The setup for this sub-step, or null. */
	const FFXR_GMSubStep* FindSubSetup(FName SubStepId) const;
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

	/** One entry per step, its sub-steps under it. Each row is titled with its Step Id. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "StepId"))
	TArray<FFXR_GMStep> StepSetup;

	/**
	 * Turn the two assets into what the runner consumes - one entry per sub-step, or per step that
	 * has none, in the Steps asset's order - plus each entry's events. Reports every disagreement
	 * between the two rather than failing quietly.
	 *
	 * Returns false when there is nothing to run. OutErrors is filled either way.
	 */
	bool Compile(TArray<FFXR_CompiledStep>& OutEntries, TArray<TArray<FFXR_StepAction>>& OutEvents,
		TArray<FString>& OutErrors) const;

	/** The setup for this step, or null. */
	const FFXR_GMStep* FindSetup(FName StepId) const;

	/** The Steps asset's step ids, for the Step Id dropdown. */
	UFUNCTION()
	TArray<FString> GetStepIdOptions() const;
};
