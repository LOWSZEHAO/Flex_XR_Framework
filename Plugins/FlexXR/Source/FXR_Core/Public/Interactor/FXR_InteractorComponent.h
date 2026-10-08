// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Interactor/FXR_Interactor.h"
#include "Types/FXR_CoreTypes.h"
#include "FXR_InteractorComponent.generated.h"

/**
 * UFXR_InteractorComponent — shared base for the concrete FlexXR interactors
 * (controller, tracked hand, desktop-sim). A SceneComponent that implements
 * IFXR_Interactor and derives grip/aim/palm poses and the framework query shapes
 * from a single tracked transform, so concrete types override only pose sourcing
 * (GetTrackedTransform) and input (type / select / use).
 *
 * Abstract: it never resolves an interactor type; drop a concrete subclass instead.
 */
UCLASS(Abstract, ClassGroup = (FlexXR))
class FXR_CORE_API UFXR_InteractorComponent : public USceneComponent, public IFXR_Interactor
{
	GENERATED_BODY()

public:
	UFXR_InteractorComponent();

	//~ Begin IFXR_Interactor (poses + query shapes; concrete types add type/select/use)
	virtual EFXR_HandSide GetHandSide() const override { return HandSide; }
	virtual bool IsInteractorActive() const override { return bInteractorActive; }
	virtual FTransform GetGripTransform() const override;
	virtual FTransform GetAimTransform() const override;
	virtual FTransform GetPalmTransform() const override;
	virtual void GetGrabSphere(FVector& OutCenter, float& OutRadius) const override;
	virtual void GetPokeTip(FVector& OutLocation, float& OutRadius) const override;
	virtual void GetFarRay(FVector& OutOrigin, FVector& OutDirection) const override;
	virtual void SendHapticFeedback(float Amplitude, float Duration) override {}
	// Inert defaults so the base stays concrete — a UObject CDO must be constructible, so no
	// unimplemented pure virtuals. The base is Abstract (never placed); concrete types override these.
	virtual EFXR_InteractorType GetInteractorType() const override { return EFXR_InteractorType::DesktopSim; }
	virtual float GetSelectValue() const override { return 0.f; }
	virtual float GetUseValue() const override { return 0.f; }
	//~ End IFXR_Interactor

	/** Set whether this interactor is the active source for its hand (driven by UFXR_XRSubsystem hot-swap). */
	void SetInteractorActive(bool bActive) { bInteractorActive = bActive; }

	/** Set which hand this interactor drives (configured by the owning rig at construction). */
	void SetHandSide(EFXR_HandSide Side) { HandSide = Side; }

	/** Set the component that positions and aims this hand's far ray (wired by the owning rig). */
	void SetAimSource(USceneComponent* Source) { AimSource = Source; }

	/** Whether the query-shape gizmo (grab sphere + poke tip) is drawn — an authoring aid for offset tuning. */
	bool IsEditorGizmoEnabled() const { return bShowEditorGizmo; }

protected:
	/** Transform the poses derive from — this component's transform by default; hand tracking overrides it. */
	virtual FTransform GetTrackedTransform() const;

	/** Which hand this interactor drives. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	EFXR_HandSide HandSide = EFXR_HandSide::Right;

	/** Radius (cm) of the near grab-sphere query shape. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor", meta = (ClampMin = "1.0"))
	float GrabSphereRadius = 10.f;

	/** Grab-sphere centre offset from the tracked transform, in tracked-local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	FVector GrabSphereLocalOffset = FVector::ZeroVector;

	/** Grip anchor offset from the tracked transform, in tracked-local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	FVector GripLocalOffset = FVector::ZeroVector;

	/** Palm-contact offset from the tracked transform, in tracked-local space. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	FVector PalmLocalOffset = FVector::ZeroVector;

	/**
	 * Where this hand's far ray leaves it and which way it points, authored by dragging the component
	 * rather than by typing offsets. AFXR_Pawn wires its Left Ray / Right Ray here; leave it unset
	 * and the ray fires straight out of the tracked pose.
	 *
	 * Read as an offset from the tracked pose, never as a world transform — see UFXR_RayOrigin.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	TObjectPtr<USceneComponent> AimSource;

	/** Index-fingertip (poke) offset from the tracked transform, in tracked-local space — align to the hand mesh's extended index tip. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor")
	FVector PokeLocalOffset = FVector::ZeroVector;

	/** Radius (cm) of the poke-tip probe sphere (a fingertip pad). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor", meta = (ClampMin = "0.1"))
	float PokeRadius = 1.f;

	/**
	 * Draw this interactor's query shapes — grab sphere and poke tip — in the editor viewport while
	 * the component is selected, for tuning the offsets against the hand mesh.
	 *
	 * Editor-only, and named for it. The shapes drawn *during play* come from Draw Debug on
	 * AFXR_Pawn instead. Two settings that both read as "debug", only one of which does anything in
	 * play, is a trap worth not setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Interactor|Debug")
	bool bShowEditorGizmo = false;

	bool bInteractorActive = true;
};
