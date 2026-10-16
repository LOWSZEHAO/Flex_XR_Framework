// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Data/FXR_TrainingSteps.h"

const FFXR_StepInfo* UFXR_TrainingSteps::FindStep(FName StepId) const
{
	return Steps.FindByPredicate([StepId](const FFXR_StepInfo& Step) { return Step.StepId == StepId; });
}

TArray<FString> UFXR_TrainingSteps::GetCompletableIdStrings() const
{
	TArray<FString> Out;
	Out.Reserve(Steps.Num());
	for (const FFXR_StepInfo& Step : Steps)
	{
		if (Step.SubSteps.IsEmpty())
		{
			if (!Step.StepId.IsNone())
			{
				Out.AddUnique(Step.StepId.ToString());
			}
			continue;
		}

		for (const FFXR_SubStepInfo& Sub : Step.SubSteps)
		{
			if (!Sub.SubStepId.IsNone())
			{
				Out.AddUnique(Sub.SubStepId.ToString());
			}
		}
	}
	return Out;
}
