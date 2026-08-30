// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
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
 * For a visual, add a Static Mesh Component under it in the Blueprint and drive that from On Aim /
 * On Exit. The anchor stays a bare scene component deliberately: inheriting a primitive would add
 * twenty-one collision, physics and input events to the Events list for the sake of one mesh slot.
 * Give any such child mesh no collision — the teleport arc has to reach the floor underneath it.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_LOCOMOTION_API UFXR_TeleportAnchor : public USceneComponent
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
	FFXR_TeleportAnchorEvent OnAim;

	/** The aim left this anchor, or aiming stopped. Always paired with an earlier On Aim. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportAnchor")
	FFXR_TeleportAnchorEvent OnExit;

	/** The player committed a teleport onto this anchor. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|TeleportAnchor")
	FFXR_TeleportAnchorEvent OnTeleported;

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
