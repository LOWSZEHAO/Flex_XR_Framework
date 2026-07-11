// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IFXR_PassthroughProvider — the MR-readiness seam for camera passthrough (architecture §10).
 *
 * The abstraction behind which an OpenXR passthrough implementation will sit in the MR
 * phase, so EFXR_Mode::MR has a home in FXR_Core today. Native (non-UObject) interface;
 * there is no runtime implementation yet — this exists so higher layers can compile
 * against a stable seam.
 */
class FXR_CORE_API IFXR_PassthroughProvider
{
public:
	virtual ~IFXR_PassthroughProvider() = default;

	/** Whether the active runtime can composite camera passthrough. */
	virtual bool IsPassthroughSupported() const = 0;

	/** Enable or disable passthrough compositing. */
	virtual void SetPassthroughEnabled(bool bEnabled) = 0;

	/** Current passthrough state. */
	virtual bool IsPassthroughEnabled() const = 0;
};
