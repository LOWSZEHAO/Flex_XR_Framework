// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_ScoringPolicy.generated.h"

class UFXR_InteractableBase;

/**
 * UFXR_ScoringPolicy — decides which of several in-reach interactables a hand actually takes.
 *
 * The one seam in the narrow phase (ADR-010). Subclass it, point Project Settings → FlexXR —
 * Interaction → Scoring Policy at the subclass, and the detection subsystem asks it instead. The
 * default ranks by distance, which is the behaviour with no policy set at all.
 *
 * A policy object rather than a virtual on the subsystem: a subsystem cannot be cleanly substituted
 * in UE 5.8, because the collection keys every instance by its concrete class, so a base-class
 * lookup stops finding it the moment a project subclasses it. Working around that means an array
 * copy per lookup, and the detection path is required to be allocation-free.
 *
 * Ranking only, deliberately — no veto. Whether an interactable is eligible at all is already
 * answered by IsGrabTarget / IsInteractionEnabled / CanBegin, and two rejection paths can disagree.
 */
UCLASS(Blueprintable, ClassGroup = (FlexXR))
class FXR_INTERACTION_API UFXR_ScoringPolicy : public UObject
{
	GENERATED_BODY()

public:
	/**
	 * Rank one candidate that has already passed its reach test. **Lower wins.**
	 *
	 * ReachDistanceSq is what the reach test measured: the squared distance to the closest point on
	 * the accepting grip point, not to the object's origin. Weight it, ignore it, or replace it with
	 * gaze alignment or approach angle.
	 */
	virtual float ScoreCandidate(const UFXR_InteractableBase& Candidate, const FVector& GrabCenter,
		EFXR_HandSide HandSide, float ReachDistanceSq) const;
};
