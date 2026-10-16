// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FXR_TrainingSteps.generated.h"

/** One step as the trainee reads it. */
USTRUCT(BlueprintType)
struct FFXR_StepInfo
{
	GENERATED_BODY()

	/** Unique within the procedure. The GM and the Complete Step node refer to the step by this. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FName StepId;

	/** The short label - the heading on the panel while this step is open. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	FText DisplayName;

	/** What the trainee is asked to do, in their language - the step's row in the panel's list. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training", meta = (MultiLine = "true"))
	FText Instruction;
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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Training")
	TArray<FFXR_StepInfo> Steps;

	/** The step with this id, or null. */
	const FFXR_StepInfo* FindStep(FName StepId) const;

	/** Step ids in order, for the GM's step dropdown. */
	TArray<FString> GetStepIdStrings() const;
};
