// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "FXR_TrainingLibrary.h"

#include "Components/FXR_TrainingSession.h"

bool UFXR_TrainingLibrary::CompleteStep(const UObject* WorldContextObject, FName StepId)
{
	UFXR_TrainingSession* Session = UFXR_TrainingSession::FindInWorld(WorldContextObject);
	return Session && Session->CompleteStep(StepId);
}
