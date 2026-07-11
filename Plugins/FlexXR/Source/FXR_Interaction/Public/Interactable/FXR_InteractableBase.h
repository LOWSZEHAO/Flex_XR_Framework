// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_InteractableBase.generated.h"

class IFXR_Interactor;
class UPrimitiveComponent;

/**
 * UFXR_InteractableBase — shared base for FlexXR interactables (Grab, Latch, Press, ...).
 *
 * Per ADR-003 there is deliberately no IFXR_Interactable interface: this class's virtual
 * lifecycle IS the extension contract. Subclass it and override CanBegin / OnBegin /
 * OnUpdate / OnEnd, and inherit registry registration, the enable API, driven-component
 * resolution, and InteractionId event emission for free.
 *
 * A SceneComponent so it can be attached to (and sit at) the mesh it drives.
 */
UCLASS(Abstract, ClassGroup = (FlexXR))
class FXR_INTERACTION_API UFXR_InteractableBase : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_InteractableBase();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The everyday gameplay switch (cutscene disable, quest unlock, ...). */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Interaction")
	void SetInteractionEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "FlexXR|Interaction")
	bool IsInteractionEnabled() const { return bInteractionEnabled; }

	UFUNCTION(BlueprintPure, Category = "FlexXR|Interaction")
	bool IsHeld() const { return bHeld; }

	//~ Extension contract (ADR-003) — override these in subclasses.
	virtual bool CanBegin(IFXR_Interactor* Interactor) const;
	virtual void OnBegin(IFXR_Interactor* Interactor);
	virtual void OnUpdate(IFXR_Interactor* Interactor, float DeltaTime);
	virtual void OnEnd(EFXR_EndReason Reason);

	/** Activation radius (cm) used by the detection broad phase. */
	float GetActivationRadius() const { return ActivationRadius; }

	/** World location used for narrow-phase scoring (the driven component, else this component). */
	FVector GetInteractionLocation() const;

protected:
	/** The primitive this interactable moves/affects: the attach-parent primitive, else the actor root primitive. */
	UPrimitiveComponent* ResolveDrivenComponent() const;

	/** Emit this interactable's InteractionId on the FXR event bus, if Expose to Training is set. */
	void BroadcastInteractionEvent(EFXR_InteractionPhase Phase, IFXR_Interactor* Interactor);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction")
	bool bInteractionEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Interaction", meta = (ClampMin = "1.0"))
	float ActivationRadius = 12.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training")
	bool bExposeToTraining = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Training", meta = (EditCondition = "bExposeToTraining"))
	FName InteractionId;

	bool bHeld = false;
};
