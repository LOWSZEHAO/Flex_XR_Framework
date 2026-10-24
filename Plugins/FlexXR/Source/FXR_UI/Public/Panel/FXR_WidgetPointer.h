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
 * UFXR_WidgetPointer - lets one hand use UMG panels (hover, click, drag, scroll).
 *
 * Add one per hand to the pawn next to the FXR_InteractionDriver. It goes through Slate's own
 * pointer events, so normal widgets work without changes.
 *
 * Subclasses UWidgetInteractionComponent to reuse its Slate plumbing, but instead of doing its
 * own trace it uses the rig's cached ray hit. That way the beam and the click always agree, and
 * there's no second trace.
 *
 * Within Poke Range the fingertip takes over from the ray. A hand that's holding something
 * doesn't act as a pointer (ADR-005).
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

	/** True while the fingertip is driving instead of the ray. */
	UFUNCTION(BlueprintPure, Category = "FlexXR|Panel")
	bool IsPoking() const { return bPokeDriving; }

protected:
	/** Which hand this pointer is for. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel")
	EFXR_HandSide Hand = EFXR_HandSide::Right;

	/** How close the fingertip has to be to take over from the ray. 0 disables touch (for out-of-reach panels). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", Units = "cm"))
	float PokeRange = 8.f;

	/** Select value at or above which the pointed-at widget is pressed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PressThreshold = 0.5f;

	/** Select value below which it's released. Lower than Press Threshold for hysteresis. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Panel", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ReleaseThreshold = 0.35f;

private:
	/** Work out what this hand is pointing at this frame and pass it to Slate. */
	void UpdateSource();

	/** Send or release the press, after the pointer position has been updated. */
	void UpdatePress();

	/** Nearest panel the fingertip is in. Arms the press while the finger is still in front. */
	bool ResolvePoke(IFXR_Interactor* Interactor, FHitResult& OutHit, UFXR_Panel*& OutPanel);

	/** The rig's far hit, if it's on a panel. */
	bool ResolveRay(FHitResult& OutHit, UFXR_Panel*& OutPanel) const;

	IFXR_Interactor* GetInteractor() const;

	void SetPressed(bool bNewPressed);

	UPROPERTY(Transient)
	TArray<TObjectPtr<UFXR_InteractorComponent>> CachedInteractors;

	UPROPERTY(Transient)
	TObjectPtr<UFXR_InteractionDriver> CachedDriver;

	TWeakObjectPtr<UFXR_Panel> HoveredPanel;

	/** Scratch array reused every frame so the scan doesn't allocate. */
	TArray<UFXR_Panel*> PanelScratch;

	/** Cm along the panel normal: positive = in front, negative = through. */
	float PokeDepth = 0.f;

	/**
	 * Set once the finger has been seen in front of the panel; a press is only allowed after that.
	 * Stops a hand reaching around the back of a floating panel from pressing it.
	 */
	bool bPokeArmed = false;

	bool bPokeDriving = false;
	bool bPressed = false;
};
