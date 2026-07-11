# ADR-003 — No separate `IFXR_Interactable` interface

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 2

## Context
A common Unreal pattern is to have the interaction manager depend on an interface (`IFXR_Interactable`) rather
than a concrete base class, keeping the manager decoupled from implementations and allowing arbitrary classes
to opt in.

## Decision
Do not ship the interface. `UFXR_InteractableBase`'s virtual lifecycle —
`CanBegin / OnBegin / OnUpdate / OnEnd(EFXR_EndReason)` — **is** the extension contract. Custom interactables
subclass the base and inherit registration, highlighting, enable/disable semantics, event-bus emission, and the
detail-panel contract for free.

## Consequences
- **Positive:** one documented extension path; extenders get the full framework contract rather than five
  orphan functions; no UE interface boilerplate; no speculative abstraction with a single implementer.
- **Negative:** a class that *cannot* inherit `UFXR_InteractableBase` (single-inheritance conflict with a
  third-party SDK) has no entry point. No such user exists for this project.
- Test mocks subclass the base inside the test module; an interface is not required for mocking.

## Reversal path
The detection subsystem is the only would-be consumer. Extracting a native-only `IFXR_Interactable` from the
base's existing virtuals — with a single `End(EFXR_EndReason)` exit rather than a separate `Cancel` — is a
mechanical refactor of call sites. Deferring costs nothing; adding it now would cost a little forever.
