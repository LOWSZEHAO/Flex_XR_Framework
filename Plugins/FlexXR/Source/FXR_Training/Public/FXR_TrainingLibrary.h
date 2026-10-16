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
	 * Finish the open training step and move to the next.
	 *
	 * Wire it to whatever event means a step is done: an object's On Grabbed, a latch's On State
	 * Changed, a button's On Pressed. That is how a step can finish on something the framework could
	 * never have known to watch for - a valve that is closed rather than merely let go of.
	 *
	 * Give it the Step Id and it only advances while that step is the one open. Most objects fire
	 * their events every time they are used, and this is what stops the third grab of the gun from
	 * skipping step four: at any other step the call does nothing. Leave Step Id empty to finish
	 * whatever step is open.
	 *
	 * Returns whether a step was finished. Finds the level's training session by itself.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training", meta = (WorldContext = "WorldContextObject"))
	static bool CompleteStep(const UObject* WorldContextObject, FName StepId);
};
