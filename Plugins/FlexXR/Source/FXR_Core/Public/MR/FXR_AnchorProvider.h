// Copyright (c) 2026 Low Sze Hao. All rights reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IFXR_AnchorProvider - placeholder interface for spatial anchors (planned MR phase).
 *
 * OpenXR spatial-anchor support will sit behind this so SOP scenes can be pinned to real-world
 * locations. No implementation yet; it's declared now so the API is in place.
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
