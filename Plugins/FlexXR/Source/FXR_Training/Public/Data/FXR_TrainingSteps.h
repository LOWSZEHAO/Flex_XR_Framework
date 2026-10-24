// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXR_TrainingSteps.generated.h"

/** One part of a step. */
USTRUCT(BlueprintType)
struct FFXR_SubStepInfo
{
	GENERATED_BODY()

	/** Unique across the whole procedure (step ids too). Used by the GM and by Complete Step. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/** The row text on the panel, e.g. "Pull the pin". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;
};

/** One step of the procedure. */
USTRUCT(BlueprintType)
struct FFXR_StepInfo
{
	GENERATED_BODY()

	/** Unique within the procedure. The GM and Complete Step use it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** What the trainee has to do. Shown as the heading of the step's page on the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (MultiLine = "true"))
	FText Instruction;

	/**
	 * Optional. Done strictly in order: each Complete Step ticks off the current one, and the step
	 * finishes with the last. Set up in the GM under the same step.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "SubStepId"))
	TArray<FFXR_SubStepInfo> SubSteps;
};

/**
 * UFXR_TrainingSteps - the text side of a procedure: ids, instructions and sub-steps, in order.
 *
 * What happens at each step (which objects open, which events fire) lives in the GM
 * (UFXR_TrainingGM), which points at this asset. Keeping the text separate means it can be edited
 * or translated without touching the wiring, and the same steps can be reused with another GM.
 */
UCLASS(BlueprintType)
class FXR_TRAINING_API UFXR_TrainingSteps : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Procedure name, e.g. "Extinguisher drill". Falls back to the asset name if empty. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	/** Optional course name, e.g. "Fire safety". First part of the panel's breadcrumb. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText CourseName;

	/** The steps, in the order they run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "StepId"))
	TArray<FFXR_StepInfo> Steps;

	/** The step with this id, or null. */
	const FFXR_StepInfo* FindStep(FName StepId) const;

	/** Step ids in order, for the GM's step dropdown. */
	TArray<FString> GetStepIdStrings() const;

	/** One step's sub-step ids in order, for the sub-step dropdown in the GM. */
	TArray<FString> GetSubStepIdStrings(FName StepId) const;
};
