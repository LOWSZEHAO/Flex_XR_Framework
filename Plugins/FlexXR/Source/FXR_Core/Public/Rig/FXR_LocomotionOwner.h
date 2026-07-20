// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "FXR_LocomotionOwner.generated.h"

class USceneComponent;

UINTERFACE(MinimalAPI)
class UFXR_LocomotionOwner : public UInterface
{
	GENERATED_BODY()
};

/**
 * IFXR_LocomotionOwner — the rig contract FXR_Locomotion needs (ADR-006). The pawn exposes its
 * tracking-space origin (the component room-scale teleport relocates) and its HMD component
 * (whose horizontal position must land on the teleport target). FXR_Locomotion queries this and
 * never casts to a concrete pawn class, so it works with any project's pawn — including a game
 * team's own ACharacter subclass.
 *
 * Plain C++ virtuals (not BlueprintNativeEvent): queried on locomotion hot paths, and the pawn
 * that implements it is C++.
 */
class IFXR_LocomotionOwner
{
	GENERATED_BODY()

public:
	/** Play-space / tracking-space origin — the component room-scale teleport relocates. */
	virtual USceneComponent* GetTrackingOriginComponent() const = 0;

	/** HMD / camera component — its horizontal world position is what lands on the target. */
	virtual USceneComponent* GetHMDComponent() const = 0;
};
