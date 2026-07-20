// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FXR_TeleportBlocker.generated.h"

/**
 * UFXR_TeleportBlocker — a no-teleport region (a world component).
 *
 * Any teleport target falling inside the box is rejected regardless of the validation mode — a
 * blocker overrides NavMesh, Surface Angle, Anchors, everything. Use it to fence off hazards,
 * pits, or off-limits areas. Self-registers into the teleport registry.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportBlocker : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_TeleportBlocker();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True if the world-space Location is inside this blocker's box (oriented by the component, unscaled). */
	bool IsInside(const FVector& Location) const;

protected:
	/** Half-size (cm) of the no-teleport box, along the component's own axes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportBlocker", meta = (ClampMin = "1.0"))
	FVector BoxExtent = FVector(100.f, 100.f, 100.f);

	/** Draw the blocked box at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportBlocker|Debug")
	bool bDrawDebug = false;
};
