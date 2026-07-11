// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IFXR_AnchorProvider — the MR-readiness seam for spatial anchors (architecture §10).
 *
 * The abstraction behind which OpenXR spatial-anchor extensions will sit in the MR phase,
 * so anchored SOP scenes can pin to real-world locations. Native (non-UObject) interface;
 * no runtime implementation yet — declared now so the seam is stable.
 */
class FXR_CORE_API IFXR_AnchorProvider
{
public:
	virtual ~IFXR_AnchorProvider() = default;

	/** Whether the active runtime supports persistent spatial anchors. */
	virtual bool AreAnchorsSupported() const = 0;

	/** Create a world-space anchor; returns an opaque handle (0 on failure). Stub until the MR phase. */
	virtual uint64 CreateAnchor(const FTransform& WorldTransform) = 0;

	/** Destroy a previously created anchor. */
	virtual void DestroyAnchor(uint64 AnchorHandle) = 0;
};
