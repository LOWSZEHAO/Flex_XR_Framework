# ADR-002 — Registry-based detection over physics overlap events

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 2

## Context
Hands must discover nearby interactables every frame. The conventional Unreal approach is overlap events
(`OnComponentBeginOverlap`) on a sphere attached to the hand, against per-object collision volumes.

## Decision
Interactables auto-register with `UFXR_InteractionSubsystem` (world subsystem + spatial hash). Each frame the
interactor's grab sphere queries the registry against **activation radii** declared by FXR_GripPoint and other
interactables. A scored narrow phase (distance, approach angle, hand side, priority, enabled state) selects the
winner, with hysteresis and claim locking. A single custom trace channel (`FXR_Interaction`) is reserved for
genuinely mesh-accurate work: ray targeting, fingertip probes, procedural grip.

## Consequences
- **Positive:** the detection primitive is the activation radius, not the render mesh — a tiny pin can be
  forgiving, a huge door isn't grabbable at its far corner; **zero per-object collision setup**, which deletes
  the single most common VR bug class ("wrong collision preset, can't grab"); deterministic evaluation order
  (overlap events fire unpredictably); cheap on Quest (sphere-vs-hash, no physics broadphase churn).
- **Negative:** a second spatial structure is maintained alongside the physics scene; interactables must
  register/unregister correctly (lifecycle bugs surface as "object not grabbable").
- **Mitigation:** registration lives in `UFXR_InteractableBase`, not user code; the editor validation panel
  reports unregistered or misconfigured interactables at author time.
