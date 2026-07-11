# ADR-004 — SOP step graph: custom runtime + DataAsset authoring (not StateTree, not a node editor yet)

**Status:** Accepted · **Date:** 2026-07 · **Phase:** 4

## Context
The FXR_Training step graph (Docs/FlexXR_Architecture.md §7) needs: active-step sets, event matching against
`InteractionId`s, branching, parallel steps, timeouts with hint escalation, a mistake taxonomy, scoring, three
delivery modes (Guided / Practice / Exam), session reporting, and deterministic replay.

Three candidate implementations were considered:
1. **UE StateTree** — Epic's hierarchical state machine, free editor, maintained upstream.
2. **Custom graph asset + Slate/EdGraph node editor** — full control, strong tooling showcase.
3. **Custom lightweight runtime + DataAsset authoring** — no bespoke editor.

## Decision
Option 3. Ship a small, pure-C++ step runtime (`FFXR_StepRunner`) consuming a **compiled step array**, with
authoring via `UFXR_StepGraph` DataAssets.

Critically: **the authoring format is separated from the runtime.** The runtime never consumes the authoring
asset directly — it consumes a compiled array of step structs. Authoring front-ends are therefore swappable:

```
Authoring (swappable)               Runtime (stable)
UFXR_StepGraph DataAsset   ──┐
CSV / JSON import          ──┼──►  compiled step array ──► FFXR_StepRunner
Visual graph editor (later) ──┘                            (pure C++, deterministic)
```

## Rationale
- **StateTree is a general-purpose state machine for AI/gameplay.** Our domain semantics (mistake taxonomy,
  hint escalation, scoring weights, exam mode, per-step reporting) do not map onto its model; they would live in
  bespoke tasks and evaluators regardless — i.e. writing our runtime *and* bending a framework.
- **Enterprise procedures arrive as spreadsheets, not engine assets.** A data-driven format supports CSV/JSON
  import for client SOPs. StateTree assets do not, meaningfully.
- **The runtime is genuinely small** — active-step set, event dispatch, branch resolution, timers — and pure
  C++, so it is deterministic (required for replay) and unit-testable without a world.
- **A node editor is the highest-cost, lowest-certainty item on the roadmap** (realistically 3–6 weeks of
  Slate/EdGraph work) and would consume Phase 4. The tooling-engineer portfolio claim is already carried by the
  pose editor, in-VR pose recorder, latch gizmos, and validation panel.

## Consequences
- **Positive:** Phase 4 stays on schedule; runtime is deterministic and testable; client SOP import is possible;
  a visual editor can be added later as a *new front-end onto the same runtime*, with zero runtime churn.
- **Negative:** authoring a large branching procedure in a details panel is less pleasant than a node graph;
  no upstream maintenance benefit from Epic.

## Revisit trigger
If a production step graph exceeds ~30 steps with heavy nesting and DataAsset authoring becomes unusable, build
the visual graph editor at that point — informed by real authoring pain rather than speculation. The runtime does
not change when that happens.
