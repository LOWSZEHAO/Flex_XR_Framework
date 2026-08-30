// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "FXR_TeleportAnchor.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_TeleportAnchorEvent);

/**
 * UFXR_TeleportAnchor — a marked teleport landing spot (a world component, not a pawn mode).
 *
 * The component's transform is the exact spot the player lands. When teleport Validation is
 * Anchors Only, aiming within Snap Radius of an anchor is the only way a target validates, and it
 * snaps to the anchor; optionally it also forces the landing facing. Self-registers into the
 * teleport registry (ADR-002 pattern), so the locomotion component never scans the level per aim.
 *
 * It is a static mesh component so the marker is the anchor rather than a child of it: assign a
 * Static Mesh for the visual and swap Materials from On Aim Enter / Exit to highlight it. Leaving
 * the mesh unset renders nothing and costs nothing. Collision is off — the teleport arc has to
 * reach the floor underneath, not stop on the marker.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportAnchor : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UFXR_TeleportAnchor();

	virtual void OnRegister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	float GetSnapRadius() const { return SnapRadius; }
	bool ShouldOverrideFacing() const { return bOverrideFacing; }

	/** The teleport aim settled on this anchor. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportAnchor")
	FFXR_TeleportAnchorEvent OnAimEnter;

	/** The aim left this anchor, or aiming stopped without committing. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportAnchor")
	FFXR_TeleportAnchorEvent OnAimExit;

	/** The player committed a teleport onto this anchor. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportAnchor")
	FFXR_TeleportAnchorEvent OnTeleportedTo;

protected:
	/** Aim within this radius (cm) of the anchor snaps the teleport target to it. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor", meta = (ClampMin = "1.0"))
	float SnapRadius = 60.f;

	/** Land facing this anchor's yaw instead of keeping the arc facing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor")
	bool bOverrideFacing = false;

	/** Draw the snap radius (and facing arrow) — in the level viewport as well as in play. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|TeleportAnchor|Debug")
	bool bDrawDebug = false;

private:
	/** Tick exists only for the debug draw, so it follows bDrawDebug rather than running always. */
	void RefreshTickState();
};
