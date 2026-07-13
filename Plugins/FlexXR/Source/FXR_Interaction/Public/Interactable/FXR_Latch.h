// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_Latch.generated.h"

class UPrimitiveComponent;

/**
 * UFXR_Latch — constrained motion (doors, levers, valves, drawers).
 *
 * The component's own transform IS the pivot; drag it to the hinge/rail. Kinematic-while-held
 * (ADR-001): the hand is projected onto the constraint via FFXR_ConstraintSolver — an angle
 * about, or a distance along, the chosen local axis — clamped to [MinLimit, MaxLimit], and the
 * driven mesh is driven to match. Rest transforms are cached at BeginPlay so a latch parented
 * under the mesh it moves never orbits its own pivot. The driven mesh should not simulate physics.
 *
 * Rotational motion has a MinLeverArm guard: gripping within that radius of the axis holds the
 * last angle rather than computing atan2 on a near-zero offset (the rotation singularity).
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Latch : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void OnBegin(IFXR_Interactor* Interactor) override;
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime) override;

#if WITH_EDITOR
	virtual void CheckForErrors() override;
#endif

	//~ Accessors for the viewport gizmo / gameplay queries.
	EFXR_LatchMotion GetMotionType() const { return MotionType; }
	/** Lower travel limit — degrees (Rotational) or cm (Linear). May be negative. */
	float GetMinLimit() const { return MinLimit; }
	/** Upper travel limit — degrees (Rotational) or cm (Linear). May be negative. */
	float GetMaxLimit() const { return MaxLimit; }
	/** Unit motion axis in the component's own local space. */
	FVector GetMotionAxisLocalUnit() const { return AxisUnitFor(MotionAxis); }

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Latch")
	EFXR_LatchMotion MotionType = EFXR_LatchMotion::Rotational;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Latch")
	EFXR_LatchAxis MotionAxis = EFXR_LatchAxis::Z;

	/** Lower limit — degrees (Rotational) or cm (Linear). Negative moves opposite the axis; 0 = the authored rest pose, so Min < 0 < Max swings both ways from rest. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Latch")
	float MinLimit = 0.f;

	/** Upper limit — degrees (Rotational) or cm (Linear). May be negative; keep it above Min Limit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Latch")
	float MaxLimit = 110.f;

	/** Rotational only: gripping within this distance (cm) of the axis holds the last angle (avoids the axis singularity). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Latch", meta = (ClampMin = "0.1", EditCondition = "MotionType == EFXR_LatchMotion::Rotational"))
	float MinLeverArm = 5.f;

private:
	FVector AxisUnitFor(EFXR_LatchAxis Axis) const;
	FVector GetAxisWorld() const;
	FVector GetPivotLocation() const;
	void ApplyValue();

	// Cached at BeginPlay (world space; assumes the mechanism actor itself does not move at runtime).
	FTransform PivotRestWorld = FTransform::Identity;
	FTransform DrivenRestWorld = FTransform::Identity;
	TWeakObjectPtr<UPrimitiveComponent> Driven;

	float CurrentValue = 0.f;   // degrees or cm; 0 = authored rest
	float LastValidValue = 0.f;

	// Held reference (captured on grab).
	float ValueAtGrab = 0.f;
	FVector HandRefInPlaneDir = FVector::ForwardVector;
	float HandRefAxisProj = 0.f;
};
