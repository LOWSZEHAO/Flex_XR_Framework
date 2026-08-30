// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FXR_TeleportBlocker.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_TeleportBlockerEvent);

/**
 * UFXR_TeleportBlocker — a no-teleport region (a world component).
 *
 * Any teleport target falling inside the box is rejected regardless of the validation mode — a
 * blocker overrides NavMesh, Surface Angle, Anchors, everything. Use it to fence off hazards,
 * pits, or off-limits areas. Self-registers into the teleport registry.
 *
 * For a warning visual, add a Static Mesh Component under it in the Blueprint and drive that from
 * On Aim / On Exit to tell the player *why* they cannot go there. Usually there is none, and only
 * the debug box marks it out. Give any such child mesh no collision — blocking is decided against
 * Box Extent, never by stopping the arc.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportBlocker : public USceneComponent
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
	FFXR_TeleportBlockerEvent OnAim;

	/** The aim left this blocker, or aiming stopped. Always paired with an earlier On Aim. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportBlocker")
	FFXR_TeleportBlockerEvent OnExit;

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
