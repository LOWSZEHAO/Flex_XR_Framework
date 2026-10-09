// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Panel/FXR_Panel.h"

#include "Camera/PlayerCameraManager.h"
#include "Interactable/FXR_RayTarget.h"
#include "Kismet/GameplayStatics.h"
#include "ProfilingDebugging/CpuProfilerTrace.h"
#include "Settings/FXR_MotionSettings.h"

namespace
{
	/**
	 * Every registered panel, so a fingertip can be tested against them directly.
	 *
	 * Poke range is a few centimetres and a level has a handful of panels, which makes a linear pass
	 * cheaper than a physics sweep and free of the collision tuning a sweep would need. Weak, and
	 * filtered by world on the way out, because PIE runs two worlds at once.
	 */
	TArray<TWeakObjectPtr<UFXR_Panel>> GRegisteredPanels;
}

UFXR_Panel::UFXR_Panel()
{
	PrimaryComponentTick.bCanEverTick = true;

	Space = EWidgetSpace::World;

	// Resolution, not size — PanelWidth turns this into centimetres. 600x400 at the default width
	// is about 2.4 pixels per millimetre, which holds body text without a shimmer.
	DrawSize = FIntPoint(600, 400);

	// A world panel that takes Slate focus takes it from the viewport, and the pawn stops hearing
	// input. Nothing on a panel needs it: a keypad's digits are buttons, not a text field.
	bWindowFocusable = false;

	// Collision is deliberately left at the engine's UI profile. New custom channels inherit that
	// profile's default response, and FXR_Interaction's default is Block — so a panel already
	// answers the rig's ray, on the same terms every interactable gets it (ADR-002).
}

void UFXR_Panel::OnRegister()
{
	Super::OnRegister();

	ApplyPanelWidth();

	// No fade on the way in. A panel that is meant to be up at the start should be up on frame one,
	// not swelling into view while the level finishes loading.
	bWantVisible = bStartVisible;
	Alpha = bStartVisible ? 1.f : 0.f;
	SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, Alpha));
	SetVisibility(bStartVisible);

	GRegisteredPanels.AddUnique(this);
}

void UFXR_Panel::OnUnregister()
{
	GRegisteredPanels.RemoveAllSwap([this](const TWeakObjectPtr<UFXR_Panel>& Panel)
	{
		return !Panel.IsValid() || Panel.Get() == this;
	});

	Super::OnUnregister();
}

void UFXR_Panel::BeginPlay()
{
	Super::BeginPlay();

	AActor* Owner = GetOwner();
	if (!bPointerBeam || !Owner || Owner->FindComponentByClass<UFXR_RayTarget>())
	{
		return;
	}

	// Attached to the panel rather than dropped on the actor, so the base class resolves this panel
	// as the driven component instead of guessing at whichever primitive it finds first.
	UFXR_RayTarget* Target = NewObject<UFXR_RayTarget>(Owner, TEXT("FXR_PanelRayTarget"), RF_Transient);
	Target->SetupAttachment(this);
	Target->RegisterComponent();
}

void UFXR_Panel::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	TRACE_CPUPROFILER_EVENT_SCOPE(FXR_Panel);

	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateFacing();
	UpdateFade(DeltaTime);
}

#if WITH_EDITOR
void UFXR_Panel::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Draw Size and Panel Width are two halves of one measurement, so either one moving re-derives
	// the scale. Cheap enough not to bother filtering on which property changed.
	ApplyPanelWidth();
}
#endif

void UFXR_Panel::SetPanelVisible(bool bNewVisible)
{
	bWantVisible = bNewVisible;

	// Shown immediately so the first frame of the fade is drawn; hiding waits for the fade to finish.
	if (bNewVisible)
	{
		SetVisibility(true);
	}
}

bool UFXR_Panel::IsPointable() const
{
	return IsVisible() && Alpha >= 1.f - KINDA_SMALL_NUMBER && GetUserWidgetObject() != nullptr;
}

bool UFXR_Panel::TracePoke(const FVector& TipWorld, float Range, FHitResult& OutHit, float& OutDepth)
{
	OutDepth = 0.f;

	// A curved panel's surface is not a plane, so flattening a fingertip onto it is a different
	// calculation. Ray pointing still works on one; poking does not, and saying so beats guessing.
	if (GetGeometryMode() != EWidgetGeometryMode::Plane)
	{
		return false;
	}

	const FTransform& ToWorld = GetComponentTransform();
	const FVector Normal = ToWorld.GetUnitAxis(EAxis::X);
	OutDepth = FVector::DotProduct(TipWorld - ToWorld.GetLocation(), Normal);

	if (OutDepth > Range || OutDepth < -Range)
	{
		return false;
	}

	// Flatten onto the surface and let the widget say where that lands, rather than reimplementing
	// its pivot and draw-size arithmetic here and drifting from it.
	const FVector Surface = TipWorld - Normal * OutDepth;
	FVector2D Local = FVector2D::ZeroVector;
	GetLocalHitLocation(Surface, Local);

	const FVector2D Size = GetCurrentDrawSize();
	if (Local.X < 0.f || Local.Y < 0.f || Local.X > Size.X || Local.Y > Size.Y)
	{
		return false;
	}

	OutHit = FHitResult();
	OutHit.bBlockingHit = true;
	OutHit.Component = this;
	OutHit.HitObjectHandle = FActorInstanceHandle(GetOwner());
	OutHit.Location = Surface;
	OutHit.ImpactPoint = Surface;
	OutHit.Normal = Normal;
	OutHit.ImpactNormal = Normal;

	// Slate reads a direction out of these, so they describe a finger arriving from the front even
	// when it has already gone through.
	OutHit.TraceStart = Surface + Normal * Range;
	OutHit.TraceEnd = Surface - Normal * Range;
	return true;
}

void UFXR_Panel::GetPanelsInWorld(const UWorld* World, TArray<UFXR_Panel*>& OutPanels)
{
	OutPanels.Reset();
	if (!World)
	{
		return;
	}

	for (const TWeakObjectPtr<UFXR_Panel>& Weak : GRegisteredPanels)
	{
		UFXR_Panel* Panel = Weak.Get();
		if (Panel && Panel->GetWorld() == World)
		{
			OutPanels.Add(Panel);
		}
	}
}

void UFXR_Panel::ApplyPanelWidth()
{
	if (PanelWidth <= 0.f || DrawSize.X <= 0)
	{
		return;
	}

	// Written only when it differs: OnRegister runs on every editor load, and a scale assigned
	// unconditionally there would dirty the level every time it opened.
	const float Wanted = PanelWidth / static_cast<float>(DrawSize.X);
	if (!GetRelativeScale3D().Equals(FVector(Wanted), 1.e-4f))
	{
		SetRelativeScale3D(FVector(Wanted));
	}
}

void UFXR_Panel::UpdateFacing()
{
	if (Facing == EFXR_PanelFacing::Fixed)
	{
		return;
	}

	const APlayerCameraManager* Camera = GetCameraManager();
	if (!Camera)
	{
		return;
	}

	// Aimed at the eye, not away from it: the widget's visible face is its +X.
	FVector ToEye = Camera->GetCameraLocation() - GetComponentLocation();
	if (Facing == EFXR_PanelFacing::YawOnly)
	{
		ToEye.Z = 0.f;
	}
	if (ToEye.IsNearlyZero())
	{
		return;
	}

	SetWorldRotation(ToEye.GetSafeNormal().ToOrientationQuat());
}

void UFXR_Panel::UpdateFade(float DeltaTime)
{
	const float Target = bWantVisible ? 1.f : 0.f;
	if (!FMath::IsNearlyEqual(Alpha, Target))
	{
		const float Step = FFXR_Motion::FadeStep(DeltaTime, UFXR_MotionSettings::GetFadeDuration());
		Alpha = FMath::FInterpConstantTo(Alpha, Target, 1.f, Step);
		SetTintColorAndOpacity(FLinearColor(1.f, 1.f, 1.f, FFXR_Motion::EaseFade(Alpha)));
	}

	// Stop drawing once it is gone. A faded-out panel still costs a render-target update and a draw
	// call otherwise, which is the kind of thing that is invisible on a desktop and not on a Quest.
	if (Alpha <= KINDA_SMALL_NUMBER && IsVisible())
	{
		SetVisibility(false);
	}
}

APlayerCameraManager* UFXR_Panel::GetCameraManager() const
{
	if (!CameraManager.IsValid())
	{
		CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);
	}
	return CameraManager.Get();
}
