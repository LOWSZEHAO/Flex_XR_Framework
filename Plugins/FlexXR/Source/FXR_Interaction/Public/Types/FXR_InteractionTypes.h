// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"
#include "FXR_InteractionTypes.generated.h"

/** Why an interaction ended — passed to UFXR_InteractableBase::OnEnd. */
UENUM(BlueprintType)
enum class EFXR_EndReason : uint8
{
	Released      UMETA(DisplayName = "Released"),
	ForceReleased UMETA(DisplayName = "Force Released"),
	Disabled      UMETA(DisplayName = "Disabled"),
	Broken        UMETA(DisplayName = "Broken")
};
