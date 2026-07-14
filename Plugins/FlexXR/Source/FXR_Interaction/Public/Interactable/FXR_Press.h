// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Interactable/FXR_InteractableBase.h"
#include "FXR_Press.generated.h"

class UPrimitiveComponent;
class IFXR_Interactor;

/** Broadcast on the activation edges of an FXR_Press (crossing / leaving the click point). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FFXR_PressDelegate);

/**
 * UFXR_Press — poke interactions: buttons, keypads, touchscreens (§4).
 *
 * Fingertip-depth driven: the interaction driver feeds each hand's poke tip (the tracked index
 * tip, or a tuned controller offset) to presses in range; the cap follows the fingertip along
 * the press axis, clamped to the travel, and springs back when the finger leaves. OnPressed
 * fires crossing the activation depth (with hysteresis on release) plus a haptic tick.
 *
 * The component's own transform is the button's rest face: its +Z is the face normal (points
 * out of the button, toward the approaching finger); pressing travels along -Z. Attach it
 * under the cap mesh it moves — rest transforms are cached at BeginPlay (circular-parenting
 * trap pre-solved, same as FXR_Latch). Not grabbable: poke-only.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_Press : public UFXR_InteractableBase
{
	GENERATED_BODY()

public:
	UFXR_Press();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Poke-driven, never grab-claimed. */
	virtual bool IsGrabTarget() const override { return false; }

	/**
	 * Offer a fingertip to this press (called by the interaction driver each tick, both hands).
	 * The deepest overlapping tip this frame drives the cap.
	 */
	void NotifyPoke(const FVector& TipLocation, float TipRadius, IFXR_Interactor* Interactor);

	/** Normalized press depth 0..1 across the travel. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Press")
	float GetPressValue() const;

	/** True while the press is past its activation depth (hysteresis-latched). */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Press")
	bool IsPressed() const { return bPressed; }

	/** Depth crossed the activation point — the click. */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Press")
	FFXR_PressDelegate OnPressed;

	/** Depth fell back below the release point (or the finger left). */
	UPROPERTY(BlueprintAssignable, Category = "FlexXR|Press")
	FFXR_PressDelegate OnReleased;

	//~ Gizmo accessors.
	float GetTravel() const { return Travel; }
	float GetFaceRadius() const { return FaceRadius; }

protected:
	/** Full press travel (cm) along the component's -Z. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.1"))
	float Travel = 1.f;

	/** Radius (cm) of the pressable face around the component's Z axis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.1"))
	float FaceRadius = 2.5f;

	/** Fraction of the travel at which OnPressed fires (the click point). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.05", ClampMax = "1.0"))
	float ActivationFraction = 0.7f;

	/** Fraction below which OnReleased fires — keep under Activation Fraction for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseFraction = 0.4f;

	/** How fast the cap springs back to rest when the finger leaves (interp speed; higher = snappier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.1"))
	float ReturnSpeed = 20.f;

	/** Haptic tick amplitude on the click edge (0 disables). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Press", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float HapticAmplitude = 0.5f;

private:
	void ApplyDepth();

	// Cached at BeginPlay (world space; assumes the button actor itself does not move at runtime).
	FTransform FaceRestWorld = FTransform::Identity;
	FTransform DrivenRestWorld = FTransform::Identity;
	TWeakObjectPtr<UPrimitiveComponent> Driven;

	float Depth = 0.f;            // cm, 0 = rest .. Travel = fully pressed
	float PendingPokeDepth = 0.f; // deepest tip offered since the last tick
	bool bPokedThisFrame = false;
	bool bPressed = false;
	IFXR_Interactor* PressingInteractor = nullptr; // haptics target; valid only while poked
};
