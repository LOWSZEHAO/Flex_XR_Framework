// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXR_TrainingSteps.generated.h"

/** One part of a step, done in its turn. */
USTRUCT(BlueprintType)
struct FFXR_SubStepInfo
{
	GENERATED_BODY()

	/**
	 * Unique within the procedure, step ids included. The GM sets the sub-step up by this, and
	 * Complete Step names it to tick it off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName SubStepId;

	/** Its line on the panel, under the step - "Pull the pin". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;
};

/** One step as the trainee reads it. */
USTRUCT(BlueprintType)
struct FFXR_StepInfo
{
	GENERATED_BODY()

	/** Unique within the procedure. The GM and the Complete Step node refer to the step by this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** What the trainee is asked to do, in their language - the heading of the step's page on the panel. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (MultiLine = "true"))
	FText Instruction;

	/**
	 * Optional. The parts of this step, done strictly in this order: each Complete Step ticks off the
	 * one that is open, and the step is finished when the last is.
	 *
	 * In the GM they sit under their step the same way, each with its own events and objects, so
	 * each part opens what it needs as it arrives.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "SubStepId"))
	TArray<FFXR_SubStepInfo> SubSteps;
};

/**
 * UFXR_TrainingSteps — a procedure as the trainee reads it, and nothing else.
 *
 * The steps run in the order listed. What happens at each one - which objects open, which events
 * fire, what counts as wrong - is the GM's job (UFXR_TrainingGM), which points at this asset. The
 * split is deliberate: this is the content a writer or a translator touches, and the GM is the
 * wiring a designer touches, and neither should have to read past the other's half to do their job.
 *
 * Text only, so the same steps can be wired differently in another level by another GM.
 */
UCLASS(BlueprintType)
class FXR_TRAINING_API UFXR_TrainingSteps : public UDataAsset
{
	GENERATED_BODY()

public:
	/**
	 * What this procedure is called - "Extinguisher drill". The panel's title and the report's.
	 * Falls back to the asset's own name when left empty, so a procedure is never nameless on a wall.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	/** The course this belongs to - "Fire safety". Optional; the panel's first crumb. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText CourseName;

	/** The steps, in the order they run. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (TitleProperty = "StepId"))
	TArray<FFXR_StepInfo> Steps;

	/** The step with this id, or null. */
	const FFXR_StepInfo* FindStep(FName StepId) const;

	/** Step ids in order, for the GM's step dropdown. */
	TArray<FString> GetStepIdStrings() const;

	/** One step's sub-step ids in order, for the dropdown on that step's sub-step rows in the GM. */
	TArray<FString> GetSubStepIdStrings(FName StepId) const;
};
