// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "FXR_GuidanceArrow.generated.h"

class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/**
 * UFXR_GuidanceArrow — "the thing you need is not here; it is that way."
 *
 * The one guidance problem highlighting cannot solve. A highlight only reaches what is already on
 * screen, so an extinguisher behind the player is invisible to it no matter how brightly it glows.
 *
 * Drop it on the pawn beside FXR_Interaction Driver and point it at something. Diegetic on purpose:
 * a world-space arrow floating below the eye line, not a HUD element welded to the view — a screen-
 * locked marker would break presence, and in a training sim it makes an assessment read as a
 * tutorial.
 *
 * **It hides itself as soon as it is redundant** — once the target is inside Hide Within Angle of
 * where the player is already looking, or closer than Arrive Radius. An arrow that stays up after
 * you have found the thing is just clutter, and the same reasoning keeps the far-ray pointer off
 * until it has something to point at.
 *
 * Lives in FXR_UI, so a game that never loads FXR_Training can use it for objectives. Training
 * consumes this; it does not own it.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_UI_API UFXR_GuidanceArrow : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_GuidanceArrow();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Point at a fixed place in the world. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void PointToLocation(FVector WorldLocation);

	/** Point at something that moves, and keep following it. Null clears. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void PointToComponent(USceneComponent* Target);

	/** Stop pointing. The arrow fades rather than vanishing. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void ClearGuidance();

	UFUNCTION(BlueprintPure, Category = "FlexXR|Guidance")
	bool HasGuidance() const { return bHasTarget; }

protected:
	/** How far in front of the player the arrow floats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "10.0", Units = "cm"))
	float Distance = 70.f;

	/** How far below eye level it sits — negative is down, out of the way of what the player is reading. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (Units = "cm"))
	float HeightOffset = -30.f;

	/** Arrow length in centimetres, taken from the mesh's own bounds rather than an assumed authoring scale. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.5", Units = "cm"))
	float ArrowSize = 8.f;

	/**
	 * Hide once the target is within this angle of where the player is already looking. They have
	 * found it; the arrow has nothing left to say.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float HideWithinAngle = 30.f;

	/** Hide once the player is this close. Arrival is its own answer. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.0", Units = "cm"))
	float ArriveRadius = 120.f;

	/** How long the arrow takes to fade in or out, matching the highlight so nothing pops. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.0", ClampMax = "1.0", Units = "s"))
	float FadeTime = 0.2f;

	//~ Ship with the plugin so a bare component draws. Soft: a hard default roots the asset through
	//~ this CDO, which makes it unrebuildable from tooling.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	TSoftObjectPtr<UStaticMesh> ArrowMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	TSoftObjectPtr<UMaterialInterface> ArrowMaterial;

private:
	/** Where the arrow should be pointing this frame, or false if nothing. */
	bool ResolveTarget(FVector& OutLocation) const;

	/** The pawn's camera, cached — the arrow is placed relative to the head, not the actor. */
	UCameraComponent* GetCamera() const;

	void UpdateArrow(float DeltaTime);

	UPROPERTY(Transient)
	mutable TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Arrow;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArrowMID;

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> TargetComponent;

	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;
	float Alpha = 0.f;
};
