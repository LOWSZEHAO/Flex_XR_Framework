// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Detection/FXR_ScoringPolicy.h"

float UFXR_ScoringPolicy::ScoreCandidate(const UFXR_InteractableBase& Candidate, const FVector& GrabCenter,
	EFXR_HandSide HandSide, float ReachDistanceSq) const
{
	// Nearest wins. The reach test already measured to the closest point on the accepting grip point
	// rather than to the object's origin, so there is nothing further to compute here.
	return ReachDistanceSq;
}
