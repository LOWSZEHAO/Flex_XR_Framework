// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Runtime/FXR_StepRunner.h"
#include "FXR_TrainingGM.generated.h"

class AActor;
class UFXR_TrainingSteps;

/** One custom event the GM calls on a Blueprint placed in the level. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_StepAction
{
	GENERATED_BODY()

	/** The Blueprint class to call. Instances are looked up in the level when the event fires. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TSoftClassPtr<AActor> Blueprint;

	/** Only needed if the Blueprint is placed more than once: limits the call to actors with this tag. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName Tag;

	/** Custom event to call, picked from the Blueprint's own events. Must have no inputs. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (DisplayName = "Event"))
	FName EventName;

	/**
	 * Exam: fire when the run starts instead of when the step arrives. That's how Exam gets objects
	 * open early enough to be used out of order. Training still fires it on arrival unless Exam
	 * Only is also ticked.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bAutoStartInExam = false;

	/**
	 * Never fire in Training. For things only an exam should have, like a decoy. With Auto Start
	 * In Exam it fires when the exam starts, otherwise when its step arrives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bExamOnly = false;

	/** Whether this fires when its step arrives, in this mode. */
	bool FiresOnArrival(EFXR_DeliveryMode Mode) const
	{
		// Exam already fired the Auto Start ones at the start; Training skips the Exam Only ones.
		return Mode == EFXR_DeliveryMode::Exam ? !bAutoStartInExam : !bExamOnly;
	}

	/** Whether this fires when an Exam run starts. */
	bool FiresAtExamStart() const { return bAutoStartInExam; }
};

/** GM setup for one sub-step. Added on top of its step's setup. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_GMSubStep
{
	GENERATED_BODY()

	/** Picked from this step's sub-steps in the Steps asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/** Fired when this sub-step arrives (or at the start of an Exam, for Auto Start ones). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "EventName"))
	TArray<FFXR_StepAction> Events;

	/** Interaction Ids of this sub-step's objects, added to the step's own. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> StepObjects;

	/** Exam: fail if this sub-step is still open after this long. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/** Exam: any mistake while this sub-step is open fails the run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bCritical = false;
};

/** GM setup for one step, with its sub-steps nested under it. */
USTRUCT(BlueprintType)
struct FXR_TRAINING_API FFXR_GMStep
{
	GENERATED_BODY()

	/** Picked from the Steps asset. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (GetOptions = "GetStepIdOptions"))
	FName StepId;

	/** Fired when the step arrives. With sub-steps, these fire just before the first sub-step's events. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "EventName"))
	TArray<FFXR_StepAction> Events;

	/**
	 * Interaction Ids of the objects this step uses (not step ids). Highlighted in Training. In Exam
	 * they decide what counts as a mistake: using them while the step is open is fine, before it is
	 * out of order, after it is ignored. With sub-steps these apply to every sub-step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FName> StepObjects;

	/** Exam: fail if the step (all of its sub-steps together) is still open after this long. 0 = no limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (ClampMin = "0.0", Units = "s"))
	float TimeoutSeconds = 0.f;

	/** Exam: any mistake while this step is open, in any sub-step, fails the run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	bool bCritical = false;

	/**
	 * Setup for each sub-step, if the step has any. Give each one its own Enable events so the parts
	 * open one at a time. A sub-step without a row just uses the step's setup.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "SubStepId"))
	TArray<FFXR_GMSubStep> SubSteps;

	/** The setup for this sub-step, or null. */
	const FFXR_GMSubStep* FindSubSetup(FName SubStepId) const;
};

/**
 * UFXR_TrainingGM - what happens at each step: events to fire, the step's objects, and the Exam
 * rules. Points at a UFXR_TrainingSteps asset for the steps themselves.
 *
 * It doesn't say when a step is done. The project calls Complete Step from whatever event means
 * done (a grab, a latch closing, a button), so the framework doesn't need to know.
 *
 * Kept as data instead of a Blueprint graph so it's readable top to bottom and can be validated
 * before the run starts (missing events, steps that don't match).
 */
UCLASS(BlueprintType)
class FXR_TRAINING_API UFXR_TrainingGM : public UDataAsset
{
	GENERATED_BODY()

public:
	/** The procedure's steps. Their order is the run order. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TObjectPtr<UFXR_TrainingSteps> Steps;

	/** One entry per step, with its sub-steps under it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "StepId"))
	TArray<FFXR_GMStep> StepSetup;

	/**
	 * Builds the runner's entry list (one per sub-step, or per step without sub-steps) and each
	 * entry's events, in Steps order. Anything that doesn't line up between the two assets goes
	 * into OutErrors. Returns false if there's nothing to run.
	 */
	bool Compile(TArray<FFXR_CompiledStep>& OutEntries, TArray<TArray<FFXR_StepAction>>& OutEvents,
		TArray<FString>& OutErrors) const;

	/** The setup for this step, or null. */
	const FFXR_GMStep* FindSetup(FName StepId) const;

	/** Step ids from the Steps asset, for the Step Id dropdown. */
	UFUNCTION()
	TArray<FString> GetStepIdOptions() const;
};
