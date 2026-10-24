// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IFXR_PassthroughProvider - placeholder interface for camera passthrough (planned MR phase).
 *
 * An OpenXR passthrough implementation will sit behind this, so EFXR_Mode::MR already has a home
 * in FXR_Core. No implementation yet.
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
