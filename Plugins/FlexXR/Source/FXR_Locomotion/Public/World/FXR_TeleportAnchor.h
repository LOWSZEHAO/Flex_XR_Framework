// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "FXR_TeleportAnchor.generated.h"

/**
 * UFXR_TeleportAnchor — a marked teleport landing spot (a world component, not a pawn mode).
 *
 * The component's transform is the exact spot the player lands. When teleport Validation is
 * Anchors Only, aiming within Snap Radius of an anchor is the only way a target validates, and it
 * snaps to the anchor; optionally it also forces the landing facing. Self-registers into the
 * teleport registry (ADR-002 pattern), so the locomotion component never scans the level per aim.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportAnchor : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_TeleportAnchor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	float GetSnapRadius() const { return SnapRadius; }
	bool ShouldOverrideFacing() const { return bOverrideFacing; }

protected:
	/** Aim within this radius (cm) of the anchor snaps the teleport target to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor", meta = (ClampMin = "1.0"))
	float SnapRadius = 60.f;

	/** Land facing this anchor's yaw instead of keeping the arc facing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor")
	bool bOverrideFacing = false;

	/** Draw the snap radius (and facing arrow) at runtime. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor|Debug")
	bool bDrawDebug = false;
};
