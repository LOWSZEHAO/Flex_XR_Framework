# FlexXR — Coding Standards

Binding for every commit. Deviations require a note in the PR description.

## 1. Style
- Follow the **Epic Coding Standard** (naming, brace placement, prefixes).
- Prefixes: `U` UObject/component · `A` Actor · `F` struct/plain class · `E` enum · `I` interface · `T` template.
- FlexXR types carry the `FXR_` marker: `UFXR_Grab`, `FFXR_ConstraintSolver`, `IFXR_Interactor`, `EFXR_HighlightStyle`, `UFXR_HandPose`.
- One type per header. Headers are the public API — they get comments; `.cpp` files get comments only where the *why* isn't obvious.

## 2. Correctness & Ownership
- `const` by default: const methods, const refs for non-trivial params, `const TArray<T>&` never `TArray<T>` by value.
- No raw `new`/`delete`. UObjects via `NewObject`/`CreateDefaultSubobject`, non-UObjects via `TUniquePtr`/`TSharedPtr`.
- Every UObject pointer held across frames is a `UPROPERTY()` or `TObjectPtr` — no dangling references.
- Prefer `TOptional` / explicit result enums over magic sentinel values.
- Assertions: `check()` for programmer errors, `ensure()` for recoverable-but-wrong, `UE_LOG` for diagnostics. Never silently swallow failure.

## 3. Performance (hot paths)
Hot paths = constraint solver, detection subsystem, hand presentation pipeline, per-tick interactor updates.
- **Zero heap allocation per frame.** Pre-size containers; reuse scratch buffers; no per-tick `TArray` growth, no `TMap` construction, no string formatting.
- No `Cast<>` in tick loops where a cached pointer or interface handle will do.
- No blueprint-exposed virtual dispatch on hot paths (`BlueprintNativeEvent` costs a `Execute_` wrapper).
- Every subsystem tick wrapped in `TRACE_CPUPROFILER_EVENT_SCOPE` — Phase 5's performance case study depends on it.
- Budget mindset: PCVR 11.1 ms/frame @ 90 Hz; Quest tier stricter. If a system can't state its budget, it isn't finished.

## 4. Architecture Rules
- **Module dependencies are one-way** and enforced in `Build.cs`:
  `FXR_Training → FXR_UI → FXR_Interaction → FXR_Core`. Nothing lower may reference anything higher.
- **Framework logic never lives in Blueprint graphs.** Blueprint is the *binding surface* only (events, setters, presets). This keeps the eventual Verse/Scene Graph exposure layer thin and swappable.
- **Solver and scoring logic are plain C++** (POD structs + pure functions) with a thin UObject wrapper — deterministic and unit-testable without a world.
- Editor-only code (pose editor, gizmos, validation panel) lives in `FXR_*Editor` modules. Runtime stays lean.
- Extension point = subclass `UFXR_InteractableBase` and override its virtuals (see `Docs/adr/ADR-003.md`).

## 5. API Surface (the "premium" bar)
Detail panels are a product surface, not a dump of variables:
- Every `UPROPERTY` has a `Category`, sensible defaults, and `meta` where it helps (`ClampMin`, `EditCondition`, `ToolTip`, `DisplayName`).
- Properties that only apply in one mode use `EditCondition` — never show a dead field.
- Public functions are `BlueprintCallable` only when a designer genuinely needs them. A large API is not a good API.
- Every public header has a one-paragraph summary comment: what it is, when to use it, what it costs.

## 6. Testing
- Solver, scoring, and retargeting have **Unreal Automation tests** in `Source/FXR_*/Tests/`.
- Determinism test: identical input sequence → identical transform output, bit-for-bit. This underpins SOP replay.
- Tests run in CI on push (see `.github/workflows/`).

## 7. Commits & PRs
- Imperative mood, module-scoped: `[FXR_Interaction] Add spatial hash broad phase to detection subsystem`.
- One coherent system change per commit. Never `fix`, `update`, `wip`, or end-of-day dumps.
- Branch per phase (`phase-1-core`), PR into `main` with a description of *what changed and why*, even solo.
- Tag a release at the end of each phase (`v0.1-core`, `v0.2-interaction`, …).
