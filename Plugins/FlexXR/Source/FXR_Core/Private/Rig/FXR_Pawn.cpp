// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#include "Rig/FXR_Pawn.h"
#include "Interactor/FXR_InteractorComponent.h"
#include "Interactor/FXR_ControllerInteractor.h"
#include "Interactor/FXR_HandInteractor.h"
#include "Interactor/FXR_DesktopSimInteractor.h"
#include "Input/FXR_InputConfig.h"
#include "System/FXR_XRSubsystem.h"
#include "Types/FXR_DeviceCapabilities.h"
#include "Types/FXR_LogChannels.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "HeadMountedDisplayFunctionLibrary.h"
#include "HeadMountedDisplayTypes.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

AFXR_Pawn::AFXR_Pawn()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Player0;
	BaseEyeHeight = 0.f;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);

	LeftHandRoot = CreateDefaultSubobject<USceneComponent>(TEXT("LeftHandRoot"));
	LeftHandRoot->SetupAttachment(VROrigin);

	RightHandRoot = CreateDefaultSubobject<USceneComponent>(TEXT("RightHandRoot"));
	RightHandRoot->SetupAttachment(VROrigin);

	// The pawn owns the motion controllers (never nest a default subobject inside another
	// component — Blueprint subclassing then mismatches the attach-parent). The controller
	// interactors parent under them and inherit the tracked pose.
	LeftMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftMotionController"));
	LeftMotionController->SetupAttachment(LeftHandRoot);
	LeftMotionController->SetTrackingMotionSource(FName("Left"));

	RightMotionController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightMotionController"));
	RightMotionController->SetupAttachment(RightHandRoot);
	RightMotionController->SetTrackingMotionSource(FName("Right"));

	LeftController = CreateDefaultSubobject<UFXR_ControllerInteractor>(TEXT("LeftController"));
	LeftController->SetupAttachment(LeftMotionController);
	LeftController->SetHandSide(EFXR_HandSide::Left);

	RightController = CreateDefaultSubobject<UFXR_ControllerInteractor>(TEXT("RightController"));
	RightController->SetupAttachment(RightMotionController);
	RightController->SetHandSide(EFXR_HandSide::Right);

	LeftHand = CreateDefaultSubobject<UFXR_HandInteractor>(TEXT("LeftHand"));
	LeftHand->SetupAttachment(LeftHandRoot);
	LeftHand->SetHandSide(EFXR_HandSide::Left);

	RightHand = CreateDefaultSubobject<UFXR_HandInteractor>(TEXT("RightHand"));
	RightHand->SetupAttachment(RightHandRoot);
	RightHand->SetHandSide(EFXR_HandSide::Right);

	DesktopSim = CreateDefaultSubobject<UFXR_DesktopSimInteractor>(TEXT("DesktopSim"));
	DesktopSim->SetupAttachment(Camera);
}

void AFXR_Pawn::BeginPlay()
{
	Super::BeginPlay();

	UHeadMountedDisplayFunctionLibrary::SetTrackingOrigin(EHMDTrackingOrigin::LocalFloor);

	if (UFXR_XRSubsystem* XR = GetXRSubsystem())
	{
		XR->OnActiveInteractorsChanged.AddUObject(this, &AFXR_Pawn::UpdateActiveInteractors);
		XR->RefreshCapabilities();
		XR->SetMode(StartupMode);
	}

	ApplyInputMapping();
	UpdateActiveInteractors();
}

void AFXR_Pawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bDrawDebug)
	{
		DrawDebugInteractors();
	}
}

void AFXR_Pawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	if (UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		if (LeftController)
		{
			LeftController->BindInput(EnhancedInput, InputConfig);
		}
		if (RightController)
		{
			RightController->BindInput(EnhancedInput, InputConfig);
		}
	}
}

IFXR_Interactor* AFXR_Pawn::GetActiveInteractor(EFXR_HandSide Side) const
{
	UFXR_InteractorComponent* Ordered[3] = { nullptr, nullptr, DesktopSim };
	if (Side == EFXR_HandSide::Left)
	{
		Ordered[0] = LeftController;
		Ordered[1] = LeftHand;
	}
	else
	{
		Ordered[0] = RightController;
		Ordered[1] = RightHand;
	}

	for (UFXR_InteractorComponent* Interactor : Ordered)
	{
		if (Interactor && Interactor->IsInteractorActive())
		{
			return Interactor; // implicit upcast to IFXR_Interactor*
		}
	}
	return nullptr;
}

float AFXR_Pawn::GetHandGripAlpha(EFXR_HandSide Side) const
{
	if (IFXR_Interactor* Interactor = GetActiveInteractor(Side))
	{
		return Interactor->GetSelectValue();
	}
	return 0.f;
}

UFXR_XRSubsystem* AFXR_Pawn::GetXRSubsystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GameInstance = World->GetGameInstance())
		{
			return GameInstance->GetSubsystem<UFXR_XRSubsystem>();
		}
	}
	return nullptr;
}

void AFXR_Pawn::UpdateActiveInteractors()
{
	FFXR_DeviceCapabilities Caps;
	if (const UFXR_XRSubsystem* XR = GetXRSubsystem())
	{
		Caps = XR->GetCapabilities();
	}

	bool bControllers = Caps.bHasMotionControllers;
	bool bHands = !bControllers && Caps.bHasHandTracking;
	bool bDesktop = !bControllers && !bHands;

	switch (InteractorPreference)
	{
	case EFXR_InteractorPreference::ForceControllers: bControllers = true;  bHands = false; bDesktop = false; break;
	case EFXR_InteractorPreference::ForceHands:       bControllers = false; bHands = true;  bDesktop = false; break;
	case EFXR_InteractorPreference::ForceDesktop:     bControllers = false; bHands = false; bDesktop = true;  break;
	default: break;
	}

	if (LeftController)  { LeftController->SetInteractorActive(bControllers); }
	if (RightController) { RightController->SetInteractorActive(bControllers); }
	if (LeftHand)        { LeftHand->SetInteractorActive(bHands); }
	if (RightHand)       { RightHand->SetInteractorActive(bHands); }
	if (DesktopSim)      { DesktopSim->SetInteractorActive(bDesktop); }

	UE_LOG(LogFXR, Log, TEXT("AFXR_Pawn active interactors: controllers=%d hands=%d desktop=%d"),
		bControllers, bHands, bDesktop);
}

void AFXR_Pawn::ApplyInputMapping()
{
	if (!InputConfig || !InputConfig->MappingContext)
	{
		return;
	}

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (ULocalPlayer* LocalPlayer = PC->GetLocalPlayer())
		{
			if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
			{
				Subsystem->AddMappingContext(InputConfig->MappingContext, InputConfig->MappingPriority);
			}
		}
	}
}

void AFXR_Pawn::DrawDebugInteractors()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	auto DrawInteractor = [World](IFXR_Interactor* Interactor, const FColor& Color)
	{
		if (!Interactor || !Interactor->IsInteractorActive())
		{
			return;
		}

		FVector Center;
		float Radius = 0.f;
		Interactor->GetGrabSphere(Center, Radius);
		DrawDebugSphere(World, Center, Radius, 12, Color, false, -1.f, 0, 0.5f);

		FVector Origin, Direction;
		Interactor->GetFarRay(Origin, Direction);
		DrawDebugLine(World, Origin, Origin + Direction * 200.f, Color, false, -1.f, 0, 0.3f);
	};

	auto TypeName = [](IFXR_Interactor* Interactor) -> const TCHAR*
	{
		if (!Interactor)
		{
			return TEXT("none");
		}
		switch (Interactor->GetInteractorType())
		{
		case EFXR_InteractorType::MotionController: return TEXT("Controller");
		case EFXR_InteractorType::TrackedHand:      return TEXT("Hand");
		case EFXR_InteractorType::DesktopSim:       return TEXT("Desktop");
		default:                                    return TEXT("?");
		}
	};

	IFXR_Interactor* Left = GetActiveInteractor(EFXR_HandSide::Left);
	IFXR_Interactor* Right = GetActiveInteractor(EFXR_HandSide::Right);
	DrawInteractor(Left, FColor::Cyan);
	DrawInteractor(Right, FColor::Yellow);

	if (GEngine)
	{
		if (Right)
		{
			GEngine->AddOnScreenDebugMessage(1, 0.f, FColor::Yellow,
				FString::Printf(TEXT("R  [%s]  select=%.2f  use=%.2f"), TypeName(Right), Right->GetSelectValue(), Right->GetUseValue()));
		}
		if (Left)
		{
			GEngine->AddOnScreenDebugMessage(2, 0.f, FColor::Cyan,
				FString::Printf(TEXT("L  [%s]  select=%.2f  use=%.2f"), TypeName(Left), Left->GetSelectValue(), Left->GetUseValue()));
		}
	}
}
