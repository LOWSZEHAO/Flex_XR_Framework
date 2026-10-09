// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "FXR_Panel.generated.h"

class APlayerCameraManager;

/** Which way a panel turns to be read. */
UENUM(BlueprintType)
enum class EFXR_PanelFacing : uint8
{
	/** Stays where it was placed — a machine readout, a wall sign, anything mounted on something. */
	Fixed      UMETA(DisplayName = "Fixed"),

	/** Turns to the player but stays upright. Text never tilts, which is most of what makes it readable. */
	YawOnly    UMETA(DisplayName = "Face Player (upright)"),

	/** Turns fully, pitch included. For a panel well above or below eye level. */
	Full       UMETA(DisplayName = "Face Player (fully)")
};

/**
 * UFXR_Panel — a UMG widget you can point at and touch.
 *
 * **The kit deliberately does not reimplement buttons, sliders and keypads.** Slate already has
 * them, and a 3D framework that rewrote them would ship worse ones that no designer recognises.
 * What UMG has no answer for is everything around the widget: a surface in the world a laser can
 * hit and a finger can push, sized in centimetres instead of pixels, that turns to face the player
 * and fades instead of appearing. That is this component. What goes inside it is ordinary UMG, and
 * a designer's existing widgets work unchanged.
 *
 * Drop it on an actor, set Widget Class, and it is pointable — no collision to configure and no
 * input to route. The rig's UFXR_WidgetPointer finds it.
 *
 * The widget's visible face is the component's **+X** axis, which is also where a poke has to come
 * from. Both this component and the pointer read that from one place, so a panel cannot be pressed
 * through its own back.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_UI_API UFXR_Panel : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UFXR_Panel();

	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** Show or hide the panel. It fades, at the framework's one fade duration. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Panel")
	void SetPanelVisible(bool bNewVisible);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPanelVisible() const { return bWantVisible; }

	/** How far through its fade the panel is, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	float GetPanelAlpha() const { return Alpha; }

	/**
	 * Whether a press should reach the widget right now. A panel still fading in, or already on its
	 * way out, refuses one: a button that answers while it is half gone is how a trainee presses the
	 * thing that replaced it.
	 */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPointable() const;

	/**
	 * Where a fingertip lands on this panel, if it is close enough to the front and inside the
	 * widget's rectangle.
	 *
	 * OutDepth is centimetres along the panel's normal: positive is still in front of the glass,
	 * negative is through it. The caller decides what counts as a press, because only the caller
	 * knows whether the finger arrived from the front.
	 */
	bool TracePoke(const FVector& TipWorld, float Range, FHitResult& OutHit, float& OutDepth);

	/** Every registered panel in this world. Resets OutPanels. */
	static void GetPanelsInWorld(const UWorld* World, TArray<UFXR_Panel*>& OutPanels);

protected:
	/**
	 * How wide the panel is in the world, in centimetres, driven into the component's scale. This is
	 * the measurement that matters in a headset and the one UMG cannot express: Draw Size is the
	 * widget's *resolution*, and at scale 1 a 600-pixel widget is six metres across, which is why
	 * every VR project ends up with a hand-tuned scale of 0.06-something.
	 *
	 * Set it to 0 to manage the scale yourself. It is relative, so a scaled parent scales the panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", Units = "cm"))
	float PanelWidth = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	EFXR_PanelFacing Facing = EFXR_PanelFacing::Fixed;

	/** Whether the panel is up when play begins. Starting hidden costs no fade. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	bool bStartVisible = true;

	/**
	 * Light the rig's pointer beam while this panel is aimed at, by marking the actor as a ray
	 * target. Without it the beam stays dark over UI — it only shows for things that will answer —
	 * and a laser you cannot see is one you cannot aim.
	 *
	 * Clear it for a panel that is only ever read, or one on an actor that is already a ray target.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	bool bPointerBeam = true;

private:
	/** Drive PanelWidth into the component scale, unless it is zero or nothing has changed. */
	void ApplyPanelWidth();

	void UpdateFacing();
	void UpdateFade(float DeltaTime);

	/** The player's eye. In VR this is the HMD, not the pawn. */
	APlayerCameraManager* GetCameraManager() const;

	UPROPERTY(Transient)
	mutable TWeakObjectPtr<APlayerCameraManager> CameraManager;

	bool bWantVisible = true;
	float Alpha = 1.f;
};
