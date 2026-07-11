# ADR-001 — Kinematic-while-held, physics-on-release constraint solver

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 2

## Context
FXR_Latch (doors, valves, drawers, levers) and FXR_Use mechanisms need constrained motion driven by the
player's hand. The obvious Unreal approach is a `UPhysicsConstraintComponent` plus a physics handle that
pulls the body toward the hand.

## Decision
Reject full physics constraints. While an interactable is held or touched, the solver:
1. projects the interactor position onto the constraint manifold (one scalar for a hinge),
2. runs that target through a feel model (inertia, damping, resistance curves, detents, state springs),
3. drives the body via `SetKinematicTarget`.

On release, tracked velocity is handed to a damped integrator (doors), a state spring (levers), or full
physics simulation with inherited velocity (free props).

## Consequences
- **Positive:** no jitter at 90 Hz; no constraint explosions; no hands pushing doors through walls; limits
  never overshoot; deterministic (required for SOP validation and replay); cheap enough for the Quest tier;
  perceived weight is authored rather than emergent, so it is *tunable*; detents give free haptic ticks.
- **Negative:** held objects do not physically resist world geometry the way a fully simulated body would;
  interactions between two simultaneously-held constrained bodies are not physically resolved.
- **Accepted trade:** authored feel beats emergent feel for both training fidelity and game polish.

## Reversal path
The solver is plain C++ operating on transforms behind a thin wrapper. A physics-constraint backend could be
added as an alternative implementation without touching component APIs — but determinism would be lost, and
with it SOP replay.
