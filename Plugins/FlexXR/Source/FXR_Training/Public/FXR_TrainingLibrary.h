// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "FXR_TrainingLibrary.generated.h"

/** Training nodes callable from any Blueprint without a session reference. */
UCLASS()
class FXR_TRAINING_API UFXR_TrainingLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Finish the open training step, or tick off the open sub-step if the step has sub-steps.
	 *
	 * Hook it to whatever means "done": On Grabbed, a latch's state change, a button press. Pass the
	 * Step Id (or Sub Step Id) and it only does something while that one is open, so it's safe on
	 * events that fire every time an object is used. A step with sub-steps ignores its own id. Leave
	 * it empty to finish whatever is open.
	 *
	 * Returns whether anything was finished. Finds the session in the level automatically.
	 */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Training", meta = (WorldContext = "WorldContextObject"))
	static bool CompleteStep(const UObject* WorldContextObject, FName StepId);
};
