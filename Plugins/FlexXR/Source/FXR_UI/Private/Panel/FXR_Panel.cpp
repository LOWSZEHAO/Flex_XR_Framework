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
	 * All registered panels, so a fingertip can be tested against them directly. Poke range is a few
	 * cm and there are only a handful of panels, so a linear pass is cheaper than a physics sweep and
	 * needs no collision setup. Weak pointers, filtered by world, since PIE can have two worlds.
	 */
	TArray<TWeakObjectPtr<UFXR_Panel>> GRegisteredPanels;
}

UFXR_Panel::UFXR_Panel()
{
	PrimaryComponentTick.bCanEverTick = true;

	Space = EWidgetSpace::World;

	// Resolution, not size (PanelWidth sets the physical size). 600x400 at the default width is
	// about 2.4 px per mm, enough for body text not to shimmer.
	DrawSize = FIntPoint(600, 400);

	/**
	 * Must be false. With a Canvas Panel root the widget doesn't report its designer size:
	 * DesignTimeSize is editor-only (UserWidget.h:1528) and SConstraintCanvas::ComputeDesiredSize
	 * (SConstraintCanvas.cpp:356) returns the largest child offset instead. With draw-at-desired-size
	 * on, that becomes CurrentDrawSize (WidgetComponent.cpp:1432), so the panel renders at roughly its
	 * content size and gets stretched onto the quad, and everything looks blurry. False is already
	 * the engine default; it's set explicitly so nobody flips it.
	 */
	bDrawAtDesiredSize = false;

	// A world panel that takes Slate focus steals it from the viewport and the pawn stops getting
	// input. Nothing on a panel needs keyboard focus.
	bWindowFocusable = false;

	// Collision stays on the engine's UI profile. New custom channels inherit its default response,
	// and FXR_Interaction's default is Block, so the rig's ray already hits the panel (ADR-002).
}

void UFXR_Panel::OnRegister()
{
	Super::OnRegister();

	ApplyPanelWidth();

	// No fade-in at startup; a panel that starts visible should be there on the first frame.
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

	// Attached to the panel (not the actor) so the base class picks this panel as the driven
	// component instead of whatever primitive it finds first.
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

	// Draw Size and Panel Width both affect the scale, so recompute on either. Cheap enough not to
	// check which one changed.
	ApplyPanelWidth();
}
#endif

void UFXR_Panel::SetPanelVisible(bool bNewVisible)
{
	bWantVisible = bNewVisible;

	// Show straight away so the first fade frame draws; hiding waits until the fade is done.
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

	// Poking a curved panel would need different maths. Ray pointing still works on one, poking
	// doesn't, so it's rejected explicitly.
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

	// Project onto the surface and let the widget component do the local-coordinate maths, instead of
	// duplicating its pivot / draw size logic here.
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

	// Slate takes a direction from these, so describe the finger as arriving from the front even
	// if it's already through.
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

	// Only write the scale if it changed. OnRegister runs on every editor load, and an unconditional
	// write would mark the level dirty every time it's opened.
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

	// Face the eye: the widget's visible side is +X.
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

	// Stop drawing when fully faded out. Otherwise it still costs a render target update and a draw
	// call, which matters on Quest.
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
