// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "FXR_Panel.generated.h"

class APlayerCameraManager;

/** How a panel turns to face the player. */
UENUM(BlueprintType)
enum class EFXR_PanelFacing : uint8
{
	/** Doesn't turn. For anything mounted: machine readouts, wall signs. */
	Fixed      UMETA(DisplayName = "Fixed"),

	/** Turns to face the player but stays upright, so text never tilts. */
	YawOnly    UMETA(DisplayName = "Face Player (upright)"),

	/** Turns fully, including pitch. For panels well above or below eye level. */
	Full       UMETA(DisplayName = "Face Player (fully)")
};

/**
 * UFXR_Panel - a world-space UMG widget you can point at with a ray or press with a finger.
 *
 * It doesn't reimplement buttons or sliders; UMG already has those and they work as-is inside the
 * panel. This component handles the VR side: sizing in centimetres, facing the player, fading,
 * and being hittable by the hand ray and fingertip without any collision setup. The rig's
 * UFXR_WidgetPointer finds it automatically.
 *
 * The front face is the component's +X. The pointer uses the same axis, so you can't press a
 * panel from behind.
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

	/** Show or hide the panel with a fade (uses the shared fade duration from Motion settings). */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Panel")
	void SetPanelVisible(bool bNewVisible);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPanelVisible() const { return bWantVisible; }

	/** Fade progress, 0 to 1. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	float GetPanelAlpha() const { return Alpha; }

	/** Whether presses are accepted right now. Not while fading in or out, so you can't hit a half-gone button. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPointable() const;

	/**
	 * Where a fingertip touches this panel, if it's close to the front and inside the widget area.
	 * OutDepth is cm along the normal: positive = in front, negative = through. The caller decides
	 * what counts as a press since it knows which side the finger came from.
	 */
	bool TracePoke(const FVector& TipWorld, float Range, FHitResult& OutHit, float& OutDepth);

	/** Every registered panel in this world. Resets OutPanels. */
	static void GetPanelsInWorld(const UWorld* World, TArray<UFXR_Panel*>& OutPanels);

protected:
	/**
	 * Panel width in the world, in cm, applied to the component scale. Draw Size is the widget's
	 * resolution, so at scale 1 a 600 px widget is 6 m wide; this saves hand-tuning the scale.
	 * Set to 0 to manage the scale yourself. Relative, so a scaled parent still scales the panel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", Units = "cm"))
	float PanelWidth = 40.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	EFXR_PanelFacing Facing = EFXR_PanelFacing::Fixed;

	/** Visible when play begins. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	bool bStartVisible = true;

	/**
	 * Mark the actor as a ray target so the pointer beam shows while aiming at the panel (the beam
	 * otherwise only shows over interactable things). Turn off for read-only panels or actors that
	 * are already ray targets.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	bool bPointerBeam = true;

private:
	/** Apply PanelWidth to the component scale, unless it's 0 or unchanged. */
	void ApplyPanelWidth();

	void UpdateFacing();
	void UpdateFade(float DeltaTime);

	/** The player's view point (the HMD in VR, not the pawn). */
	APlayerCameraManager* GetCameraManager() const;

	UPROPERTY(Transient)
	mutable TWeakObjectPtr<APlayerCameraManager> CameraManager;

	bool bWantVisible = true;
	float Alpha = 1.f;
};
