// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "FXR_TeleportBlocker.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_TeleportBlockerEvent);

/**
 * UFXR_TeleportBlocker — a no-teleport region (a world component).
 *
 * Any teleport target falling inside the box is rejected regardless of the validation mode — a
 * blocker overrides NavMesh, Surface Angle, Anchors, everything. Use it to fence off hazards,
 * pits, or off-limits areas. Self-registers into the teleport registry.
 *
 * It is a static mesh component so a warning visual is the blocker itself rather than a child of
 * it: assign a Static Mesh and swap Materials from On Aim Enter / Exit to tell the player *why*
 * they cannot go there. Usually left mesh-less, with only the debug box marking it out. Collision
 * is off — blocking is decided against Box Extent, never by stopping the arc.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportBlocker : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UFXR_TeleportBlocker();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** True if the world-space Location is inside this blocker's box (oriented by the component, unscaled). */
	bool IsInside(const FVector& Location) const;

	/** The teleport aim landed inside this blocker and was rejected. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportBlocker")
	FFXR_TeleportBlockerEvent OnAimEnter;

	/** The aim left this blocker, or aiming stopped. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportBlocker")
	FFXR_TeleportBlockerEvent OnAimExit;

protected:
	/** Half-size (cm) of the no-teleport box, along the component's own axes. Scale does not apply. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportBlocker", meta = (ClampMin = "1.0"))
	FVector BoxExtent = FVector(100.f, 100.f, 100.f);

	/** Draw the blocked box — in the level viewport as well as in play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportBlocker|Debug")
	bool bDrawDebug = false;

private:
	void RefreshTickState();
};
