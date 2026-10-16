// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FXR_TrainingLibrary.generated.h"

/**
 * Training nodes that can be called from any Blueprint, without a reference to the session.
 */
UCLASS()
class FXR_TRAINING_API UFXR_TrainingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Finish the open training step and move to the next - or, while a step with sub-steps is open,
	 * tick off its open sub-step. Sub-steps go strictly in order, and the step finishes with its last.
	 *
	 * Wire it to whatever event means a step is done: an object's On Grabbed, a latch's On State
	 * Changed, a button's On Pressed. That is how a step can finish on something the framework could
	 * never have known to watch for - a valve that is closed rather than merely let go of.
	 *
	 * Give it the Step Id - or the Sub Step Id, for a sub-step - and it only advances while that one
	 * is open. Most objects fire their events every time they are used, and this is what stops the
	 * third grab of the gun from skipping step four: at any other time the call does nothing. A step
	 * with sub-steps is finished through them, so its own id does nothing. Leave it empty to finish
	 * whatever is open.
	 *
	 * Returns whether anything was finished. Finds the level's training session by itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training", meta = (WorldContext = "WorldContextObject"))
	static bool CompleteStep(const UObject* WorldContextObject, FName StepId);
};
