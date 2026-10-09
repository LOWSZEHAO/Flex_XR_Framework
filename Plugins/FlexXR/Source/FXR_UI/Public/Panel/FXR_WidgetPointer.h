// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetInteractionComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_WidgetPointer.generated.h"

class IFXR_Interactor;
class UFXR_InteractionDriver;
class UFXR_InteractorComponent;
class UFXR_Panel;

/**
 * UFXR_WidgetPointer — one hand's ability to use UMG.
 *
 * Add one per hand to the pawn beside FXR_Interaction Driver and every FXR_Panel in the level
 * becomes usable: hover, click, drag, scroll, all of it, through Slate's own pointer. That is why
 * ordinary widgets work without being told they are in VR.
 *
 * It subclasses the engine's widget interaction component to keep that pointer plumbing and replace
 * only the part that is wrong for XR — where the pointer is. The engine traces along its own
 * transform, which would be a second ray, aimed slightly differently from the one the rig already
 * casts and draws. This reads the rig's cached hit instead, so what the beam lands on and what the
 * press hits are the same answer by construction, at the cost of no extra trace.
 *
 * **A fingertip beats the laser.** Inside Poke Range the finger drives the panel, because a hand
 * that can touch the glass is not pointing at it — the same arbitration the driver makes between
 * near and far interaction, for the same reason.
 *
 * **It yields to interaction.** A hand holding something is not a cursor (ADR-005).
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_UI_API UFXR_WidgetPointer : public UWidgetInteractionComponent
{
	GENERATED_BODY()

public:
	UFXR_WidgetPointer();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** The panel this hand is on, or null. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	UFXR_Panel* GetHoveredPanel() const { return HoveredPanel.Get(); }

	/** True while the finger is driving rather than the ray — for a hand visual that reacts to it. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPoking() const { return bPokeDriving; }

protected:
	/** Which hand this pointer belongs to. One component per hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	EFXR_HandSide Hand = EFXR_HandSide::Right;

	/**
	 * How near the fingertip has to be before it takes the panel off the laser. Zero turns touch off
	 * and leaves the hand pointing, which is the right setting for a panel mounted out of reach.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", Units = "cm"))
	float PokeRange = 8.f;

	/** Select value at or above which a pointed-at widget is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressThreshold = 0.5f;

	/** Select value below which it is released. Kept under Press Threshold for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseThreshold = 0.35f;

private:
	/** Decide what this hand is on this frame and hand it to Slate. */
	void UpdateSource();

	/** Send or withdraw the press, after Slate has been told where the pointer is. */
	void UpdatePress();

	/** Nearest panel the fingertip is inside. Sets the arm flag while the finger is still outside. */
	bool ResolvePoke(IFXR_Interactor* Interactor, FHitResult& OutHit, UFXR_Panel*& OutPanel);

	/** The rig's far hit, if it landed on a panel. */
	bool ResolveRay(FHitResult& OutHit, UFXR_Panel*& OutPanel) const;

	IFXR_Interactor* GetInteractor() const;

	void SetPressed(bool bNewPressed);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_InteractorComponent>> CachedInteractors;

	UPROPERTY(Transient)
	TObjectPtr<UFXR_InteractionDriver> CachedDriver;

	TWeakObjectPtr<UFXR_Panel> HoveredPanel;

	/** Refilled every frame and emptied again; kept as a member so the scan never allocates. */
	TArray<UFXR_Panel*> PanelScratch;

	/** Centimetres along the panel's normal: positive is short of the glass, negative is through it. */
	float PokeDepth = 0.f;

	/**
	 * Set once the finger has been seen in front of a panel, and the only way a press is allowed.
	 * Without it a hand reaching past the back of a floating panel would press whatever is on it.
	 */
	bool bPokeArmed = false;

	bool bPokeDriving = false;
	bool bPressed = false;
};
