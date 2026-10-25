// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Engine/EngineTypes.h"
#include "Types/FXR_CoreTypes.h"
#include "Types/FXR_InteractionTypes.h"
#include "FXR_GripPoint.generated.h"

class UFXR_HandPose;
class UFXR_HandVisual;
class UFXR_InteractableBase;

/**
 * UFXR_GripPoint - an authored place and pose for a hand on an object.
 *
 * Its transform is the grip pose. Add one or more to a grabbable actor; the owning interactable
 * picks one by hand side, priority and distance, then aligns the held object (Grab) or the hand
 * (Latch) to it, so the hold is consistent instead of wherever the hand happened to close.
 *
 * If an interactable has any grip points, they become its only grab surface (ADR-007); with
 * none, the mesh itself is the grab surface. All grip settings live here, not on the interactable.
 *
 * The owner is found automatically: the nearest interactable up the hierarchy, otherwise the
 * actor's only interactable. With several interactables on one actor, set Owners explicitly (a
 * grip point can be shared, but only one owner can be enabled at a time).
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_INTERACTION_API UFXR_GripPoint : public USceneComponent
{
	GENERATED_BODY()

public:
	UFXR_GripPoint();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void OnRegister() override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

#if WITH_EDITOR
	virtual void CheckForErrors() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	/** True if this grip point accepts the given hand. */
	bool AcceptsHand(EFXR_HandSide Side) const;

	/**
	 * The interactables this grip point belongs to. Explicit Owners win; otherwise the nearest
	 * ancestor interactable in the hierarchy, else the actor's single interactable. Ambiguity
	 * (several interactables, no explicit owner) resolves to none - fail loudly at author time,
	 * never guess at runtime (ADR-007).
	 */
	void ResolveOwners(TArray<UFXR_InteractableBase*>& OutOwners) const;

	/** True if the given interactable is one of this grip point's resolved owners. */
	bool IsOwnedBy(const UFXR_InteractableBase* Interactable) const;

	/** True when this is a rail (a grabbable extent) rather than a single point. */
	bool IsRail() const { return RailLength > KINDA_SMALL_NUMBER; }

	/** Rail length (cm) along the component's local X, centred on the component. 0 = point grip. */
	/**
	 * Rail length in world centimetres - the authored length times this component's world scale.
	 *
	 * Scaled, unlike the activation radius, because the two answer different questions. The radius is
	 * hand ergonomics: a hand is a hand, so 8 cm of grab tolerance stays 8 cm however large the object
	 * is, and shrinking a part must not make it harder to grab. The rail is geometry - how far along
	 * this object a hand may slide - so scaling the object has to scale it too, or the hand slides off
	 * the end of the mesh it is supposedly holding.
	 *
	 * Read live rather than cached, so rescaling the mesh takes effect without re-adding anything.
	 */
	float GetRailLength() const { return RailLength * FMath::Abs(GetComponentScale().X); }

	/**
	 * Where on this grip a hand at WorldLocation lands: the nearest point on the rail, or the
	 * component location for a point grip. Reach, snapping and hand attachment all use this, which is
	 * how a hand can grab a handrail anywhere along it.
	 */
	FVector GetClosestPointTo(const FVector& WorldLocation) const;

	/** The authored grip pose for a hand at WorldLocation - component rotation, rail-slid location. */
	FTransform GetGripTransformFor(const FVector& WorldLocation) const;

	int32 GetPriority() const { return Priority; }
	float GetActivationRadius() const { return ActivationRadius; }
	EFXR_GripSnapMode GetSnapMode() const { return SnapMode; }
	float GetSnapInterpSpeed() const { return SnapInterpSpeed; }
	bool IsDrawDebugEnabled() const { return bDrawDebug; }

	/** The hand pose formed when gripping here, or null. */
	UFXR_HandPose* GetHandPose() const;

protected:
	/**
	 * Owning interactables, set explicitly. Leave empty to auto-resolve (nearest ancestor
	 * interactable, else the actor's single interactable). Required when several interactables
	 * share this point (e.g. a door handle owned by Grab + Latch) - at most one owner may be
	 * enabled at a time.
	 */
	UPROPERTY(EditAnywhere, Category = "FlexXR|GripPoint", meta = (UseComponentPicker, AllowedClasses = "/Script/FXR_Interaction.FXR_InteractableBase"))
	TArray<FComponentReference> Owners;

	/** Which hand may use this grip point. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	EFXR_GripHandedness Handedness = EFXR_GripHandedness::Both;

	/** Higher priority wins when several grip points are in range for the same hand. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	int32 Priority = 0;

	/** Radius (cm) from the hand's grip within which this point is a candidate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint", meta = (ClampMin = "0.5"))
	float ActivationRadius = 8.f;

	/**
	 * Rail length (cm) along local X, centred on the component. Hands can grab anywhere along it (a
	 * handrail, pipe or rifle handguard) instead of snapping to one spot. 0 = single point. Activation
	 * Radius is measured from the rail, so it acts like the rail's thickness.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint", meta = (ClampMin = "0.0"))
	float RailLength = 0.f;

	/** How the object arrives at this grip pose when grabbed here. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	EFXR_GripSnapMode SnapMode = EFXR_GripSnapMode::Smooth;

	/** Smooth mode: how fast the object eases to the grip pose (higher = faster; ~10 is roughly 100 ms). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint", meta = (ClampMin = "0.1", EditCondition = "SnapMode == EFXR_GripSnapMode::Smooth"))
	float SnapInterpSpeed = 10.f;

	/** Optional hand pose (finger shape) formed when gripping here - applied by the hand Anim BP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint")
	TObjectPtr<UFXR_HandPose> HandPose;

	/** Draw this grip point's axes + activation radius at runtime (authoring aid). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|GripPoint|Debug")
	bool bDrawDebug = false;

	virtual void OnUpdateTransform(EUpdateTransformFlags UpdateTransformFlags, ETeleportType Teleport = ETeleportType::None) override;

#if WITH_EDITORONLY_DATA
	/**
	 * Show a hand here in the editor, placed and posed the way the real hand will be when it holds
	 * this point. A copy of the pawn's own hand (see Hand Preview Pawn in the FlexXR project
	 * settings), so it matches the mesh and grip offset you play with. A point either hand can use
	 * shows the right hand. Editor only; nothing is created in game.
	 */
	UPROPERTY(EditAnywhere, Category = "FlexXR|GripPoint|Debug")
	bool bPreviewHand = true;
#endif

private:
	/** Owners this point registered with at BeginPlay, kept for symmetric unregistration. */
	TArray<TWeakObjectPtr<UFXR_InteractableBase>> RegisteredOwners;

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	TObjectPtr<UFXR_HandVisual> PreviewHand;
#endif

#if WITH_EDITOR
	/** Create, update or remove the preview hand to match the current settings. */
	void RefreshPreviewHand();

	/** Put the preview hand where the real one goes: the pawn's grip offset from this point. */
	void PlacePreviewHand() const;
#endif
};
