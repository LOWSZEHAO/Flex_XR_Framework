// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Templates/SubclassOf.h"
#include "FXR_GuidanceArrow.generated.h"

class AActor;
class UCameraComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

/** Which local axis of the mesh points forward. The engine cone points +Z; most DCC arrows point +X. */
UENUM(BlueprintType)
enum class EFXR_MeshAxis : uint8
{
	X        UMETA(DisplayName = "+X"),
	Y        UMETA(DisplayName = "+Y"),
	Z        UMETA(DisplayName = "+Z"),
	NegX     UMETA(DisplayName = "-X"),
	NegY     UMETA(DisplayName = "-Y"),
	NegZ     UMETA(DisplayName = "-Z")
};

/**
 * UFXR_GuidanceArrow - an arrow that points the player toward something off-screen.
 *
 * Highlighting only helps once the object is in view; this covers the case where it's behind
 * the player. Add it to the pawn and give it a target. It's a world-space arrow floating below
 * eye level, not a HUD element, so it doesn't break presence.
 *
 * It hides itself once the target is within Hide Within Angle of the view direction, or closer
 * than Arrive Radius.
 *
 * Lives in FXR_UI so games can use it for objectives without the training module.
 */
UCLASS(ClassGroup = (FlexXR), meta = (BlueprintSpawnableComponent))
class FXR_UI_API UFXR_GuidanceArrow : public UActorComponent
{
	GENERATED_BODY()

public:
	UFXR_GuidanceArrow();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Point at a fixed place in the world. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void PointToLocation(FVector WorldLocation);

	/** Point at something that moves, and keep following it. Null clears. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void PointToComponent(USceneComponent* Target);

	/** Stop pointing. Fades out. */
	UFUNCTION(BlueprintCallable, Category = "FlexXR|Guidance")
	void ClearGuidance();

	UFUNCTION(BlueprintPure, Category = "FlexXR|Guidance")
	bool HasGuidance() const { return bHasTarget; }

protected:
	/** How far in front of the player the arrow floats. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "10.0", Units = "cm"))
	float Distance = 70.f;

	/** Offset below eye level (negative = down), so it stays out of the way. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (Units = "cm"))
	float HeightOffset = -30.f;

	/** Sideways offset, positive = player's right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (Units = "cm"))
	float LateralOffset = 0.f;

	/** Arrow length in cm, measured from the mesh bounds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.5", Units = "cm"))
	float ArrowSize = 8.f;

	/** Hide once the target is within this angle of where the player is looking. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.0", ClampMax = "90.0", Units = "deg"))
	float HideWithinAngle = 30.f;

	/** Hide once the player is this close. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance", meta = (ClampMin = "0.0", Units = "cm"))
	float ArriveRadius = 120.f;

	//~ Defaults that ship with the plugin. Soft references, since a hard default in the CDO roots
	//~ the asset and then tools can't rebuild it.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	TSoftObjectPtr<UStaticMesh> ArrowMesh;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	TSoftObjectPtr<UMaterialInterface> ArrowMaterial;

	/** Which way the mesh points. Only used with Arrow Mesh. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	EFXR_MeshAxis MeshForwardAxis = EFXR_MeshAxis::Z;

	/**
	 * Optional Blueprint to use instead of Arrow Mesh (particles, an animated marker, etc.). Spawned
	 * on first use and driven by this component; it should face +X and have no collision. Its own
	 * scale is kept and multiplied by the fade, so Arrow Size doesn't apply.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlexXR|Guidance")
	TSubclassOf<AActor> VisualClass;

private:
	/** Where the arrow should point this frame, or false if nowhere. */
	bool ResolveTarget(FVector& OutLocation) const;

	/** Cached pawn camera. The arrow is placed relative to the head. */
	UCameraComponent* GetCamera() const;

	void UpdateArrow(float DeltaTime);

	UPROPERTY(Transient)
	mutable TObjectPtr<UCameraComponent> Camera;

	UPROPERTY(Transient)
	TObjectPtr<UStaticMeshComponent> Arrow;

	UPROPERTY(Transient)
	TObjectPtr<AActor> VisualActor;

	/** The Blueprint's own scale, so the fade multiplies it instead of replacing it. */
	FVector VisualBaseScale = FVector::OneVector;

	/** Only try spawning the Visual Class once, so a failure isn't retried every frame. */
	bool bVisualSpawnAttempted = false;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ArrowMID;

	UPROPERTY(Transient)
	TWeakObjectPtr<USceneComponent> TargetComponent;

	FVector TargetLocation = FVector::ZeroVector;
	bool bHasTarget = false;
	float Alpha = 0.f;
};
