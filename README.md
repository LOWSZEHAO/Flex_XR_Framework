# FlexXR Framework

**A high-fidelity XR interaction framework for Unreal Engine 5.8 — built for both SOP-driven industrial training and AAA-grade VR gameplay.**

C++ core · OpenXR · hand tracking + controller parity · PCVR-first, Quest-capable · MR-ready

> 🚧 **Status:** Phase 2 — Interaction core **complete**. Next: Phase 2.5 — FXR_Locomotion. See [Roadmap](#roadmap).

---

## Overview

FlexXR is a modular Unreal Engine plugin providing a single, premium interaction layer that serves two markets normally handled by separate codebases:

- **Industrial training** — step-based SOP procedures with validation, mistake tracking, scoring and replay (e.g. fire safety certification).
- **VR games** — Alyx-grade hand interaction feel: physics hands, constrained mechanisms, deterministic behaviour at 90 fps.

The architectural bet: **one interaction layer, two products.** Training modules sit *on top of* the interaction layer and only observe it — games simply never load them.

---

## Features

| | |
|---|---|
| 🖐️ **Hand + controller parity** | Every interaction works with tracked hands, motion controllers, or a desktop mouse simulator. No interaction ever assumes an input device. |
| 🤲 **Drop-in interactables** | `FXR_Grab` (one- and two-handed, built-in use events, snap-to-pose grip points), `FXR_Latch` (doors, levers, valves, drawers — limits, states, 0–1 value events), `FXR_Press` (fingertip-depth buttons with haptic click). Drop the component on a mesh — it works. |
| 🚪 **Deterministic constraint solver** | Kinematic-while-held, physics-on-release. Doors, valves, drawers and levers with authored weight and detents — no jitter, no constraint explosions, identical on PCVR and Quest. Geometry + determinism pinned by automation tests. |
| ✋ **Skeleton-agnostic hand poses** | Poses stored as curl/splay values, retargeted per skeleton + fingertip IK — one pose library survives hand-mesh swaps and any player hand size. |
| 🎯 **Zero-setup detection** | Registry-based broad phase with activation radii; no per-object collision configuration, ever. |
| 📋 **SOP step graph** | A judge, not a controller — the world stays interactive, the graph watches and validates. Branching, parallel steps, Guided/Practice/Exam modes, session reports. |
| 🚶 **Comfort-first locomotion** | One component, every mode — teleport (room-scale origin so your head lands on target), smooth move, snap/smooth turn, comfort vignette; hand-tracking gesture parity with automatic teleport fallback. |
| ✨ **Premium authoring UX** | Presets, viewport gizmos, ghost-hand previews, in-VR pose recorder, author-time validation panel. |

---

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│  FXR_Training     (optional — SOP layer)                    │  knows about everything below
├──────────────────────────────┬──────────────────────────────┤
│  FXR_UI                      │  FXR_Locomotion              │  siblings
│  (spatial UI kit, motion)    │  (teleport, smooth, turn)    │
├──────────────────────────────┴──────────────────────────────┤
│  FXR_Interaction  (components + solvers + detection)        │  knows nothing about Training
├─────────────────────────────────────────────────────────────┤
│  FXR_Core         (platform / OpenXR layer)                 │
└─────────────────────────────────────────────────────────────┘
```

**Full design document:** [`Docs/FlexXR_Architecture.md`](Docs/FlexXR_Architecture.md)
**Decision records:** [`Docs/adr/`](Docs/adr/)
**Coding standards:** [`CODING_STANDARDS.md`](CODING_STANDARDS.md)

---

## Demos

<!-- GIFs land here as phases complete -->
| | |
|---|---|
| _In-VR pose recorder_ | _coming with the authoring tools_ |
| _Palm-push vs handle-pull_ | _coming with contact drive_ |
| _Guided vs Exam mode_ | _coming in Phase 4_ |
| _PCVR vs Quest side-by-side_ | _coming in Phase 5_ |

---

## Requirements

- Unreal Engine **5.8**
- Visual Studio 2022 (Windows) with *Game development with C++* workload
- An OpenXR runtime (Meta Quest Link, SteamVR, or Varjo)
- Git LFS

## Build

```bash
git clone https://github.com/LOWSZEHAO/Flex_XR_Framework.git
cd Flex_XR_Framework
git lfs pull
```
Right-click `FlexXR.uproject` → **Generate Visual Studio project files** → open the `.sln` → build **Development Editor**.

---

## Roadmap

- [x] **Phase 1 — FXR_Core** · pawn/rig, `IFXR_Interactor`, input mapping, capability detection, event bus, MR flags
- [x] **Phase 2 — Interaction core** · registry detection, deterministic constraint solver (+ automation tests), Grab (two-hand, use events) / Latch (states, value events) / Press (fingertip probes), grip points + hand poses, editor gizmos
- [ ] **Phase 2.5 — FXR_Locomotion** · teleport (arc + validation, room-scale origin), smooth move, snap/smooth turn, comfort vignette, anchors + blockers
- [ ] **Phase 3 — FXR_UI** · spatial UI kit, ray targeting + focus manager, sockets, highlight system
- [ ] **Phase 4 — FXR_Training** · SOP step graph, modes, reporting + fire safety demo
- [ ] **Phase 5 — Optimization** · Quest standalone build, Unreal Insights performance case study
- [ ] **Phase 6 — MR + game demo** · passthrough, planes, anchors + action game demo

---

## License

All rights reserved — see [LICENSE](LICENSE). This repository is public for
portfolio review; no usage license is granted.
