# FlexXR — Project Status & Handoff

**Last updated:** 2026-10-08 · **Last active development:** 2026-10-08
**Current branch:** `phase-3-ui-presentation` · **Architecture doc version:** 0.14

This document is the single place to find *where the project actually is*. The architecture
document says what FlexXR is and why; this says what is built, what is half-built, what was
deliberately not built, and what to do next.

If you are picking this project up cold, read in this order:

| # | Document | What it gives you |
|---|---|---|
| 1 | **This file** | Current state, next actions, known gaps |
| 2 | `Docs/FlexXR_Architecture.md` | The design and the reasoning. Changelog at the bottom is a history of every decision |
| 3 | `Docs/adr/` | Ten decision records. **Do not relitigate these without writing a new ADR** |
| 4 | `CODING_STANDARDS.md` | Binding: Epic style, allocation-free hot paths, module rules, commit format |

---

## 1. The one-paragraph version

FlexXR is a C++ XR interaction framework for Unreal Engine 5.8 (OpenXR), built so that **one
interaction layer serves two markets**: SOP-driven industrial training and high-fidelity VR games.
The architectural bet that makes that possible is **determinism** — kinematic-while-held,
physics-on-release (ADR-001) — because SOP replay and scoring need an interaction that reproduces
exactly, and games need hand feel that does not jitter. Training sits *on top of* interaction and
only observes it; games simply never load it. That separation is enforced mechanically in
`Build.cs`, not by convention.

---

## 2. Phase status

| Phase | State | Tag |
|---|---|---|
| 1 — `FXR_Core` | ✅ Complete | pending `v0.1-core` |
| 2 — Interaction core | ✅ Complete, **fully verified in-headset** | `v0.2-interaction` |
| 2.5 — `FXR_Locomotion` | ✅ Complete, **fully verified in-headset** | `v0.3.1-locomotion` |
| **3 — `FXR_UI` + presentation** | 🔶 **In progress — current work** | — |
| 4 — `FXR_Training` + SOP demo | ⬜ Not started | — |
| 5 — Optimization + Quest standalone | ⬜ Not started | — |
| 6 — MR pass + game demo | ⬜ Not started | — |

As of 2026-10-08 the framework also **runs on Quest standalone** — built, packaged, installed and
confirmed in-headset (§6). That is the Phase 5 target platform reached early, which de-risks it:
Phase 5 is now optimisation on a device that already runs, not a port.

Full roadmap with time estimates: architecture doc §13.

---

## 3. Phase 3 — what is built

All on branch `phase-3-ui-presentation`, unmerged. Builds clean. Verified in-headset as it landed,
except where noted in §4.

**Focus and highlight**
- `UFXR_FocusSubsystem` — hover/selected, precedence Selected > Guidance > Hover.
- `UFXR_HighlightSubsystem` — three styles (Outline, Inner Blink, Sweep) bound to *semantic states*,
  never to appearances. Training says "highlight the pin, Guidance state" and never learns what
  Guidance looks like.
- **Outline has two implementations** behind one API (`Highlight Tier`: Auto / Post Process / Mesh
  Hull). Post Process reads a packed stencil (`State + Level * 4`) in one full-screen pass. Mesh Hull
  is an inverted hull for mobile — the mesh drawn again, pushed along its normals, front faces masked
  by `TwoSidedSign`. `Auto` resolves on **feature level**, so the editor's mobile preview shows the
  Quest path without a Quest.
- **Proximity ramp** — interactables glow as a hand approaches, full strength only at grab range.
  Deliberately *not* always-on: that reads as a tutorial level, and in a training sim it removes the
  competency being tested.
- Everything fades. Nothing pops.

**Far interaction**
- `FXR_RayTarget` — the "you can point at me" marker. The beam itself belongs to the driver.
- Far-ray pointer with per-hand `Left Ray` / `Right Ray` origin components on the pawn — the beam is
  aimed by dragging a component, not by typing offsets.
- `Ray Visibility`: Never / **On Target** (default) / Always. An always-on beam says nothing and
  reads as a menu cursor.
- **Distance grab** — a checkbox on `FXR_Grab`, not a second component.

**Sockets**
- `FXR_Socket` — seat pose is the socket's own transform, actor-tag filtering, nearest accepting
  socket wins, ghost preview with Off / On Approach / Always modes.

**Guidance**
- `UFXR_GuidanceArrow` (`FXR_UI`) — a world-space arrow for "the thing you need is not here, it is
  that way". The one guidance problem highlighting cannot solve, since a highlight only reaches what
  is already on screen. Hides itself once the target is within `Hide Within Angle` of where the
  player is already looking, or nearer than `Arrive Radius`.

**Motion design**
- `UFXR_MotionSettings` in **`FXR_Core`** — one `Fade Duration` governing highlights, the socket
  ghost, the beam and the arrow, plus `FFXR_Motion::EaseFade` for one curve. The spec is settings the
  components read, not a document they are meant to honour. See architecture §5.9.

**Extensibility**
- `UFXR_ScoringPolicy` — the one seam in narrow-phase detection. See ADR-010 for what is open, what
  is closed, and why.

**Tooling**
- `Tools/regen_fxr_materials.py` is the source of truth for every plugin material. **Run it from the
  editor console, never unattended** — see §7.

---

## 4. Phase 3 — what remains

| Item | Notes |
|---|---|
| **Guidance arrow: never seen rendering** | Built, builds clean, material verified correct — but nothing has confirmed it draws. **Highest-value next test.** See §6 for how |
| **Spatial UI kit** | Panels, buttons, sliders, keypads, auto ray-targetable. Explicitly wanted **last** |
| **Validation panel** | An in-world "you did this wrong" surface. Recommend building it in Phase 4, where the step graph defines what it must show, rather than guessing now |
| **Ghost-hand guidance** | **Deliberately dropped** — see §5 |

---

## 5. Deliberate omissions — do not "fix" these

Things that look missing but are decisions. Reopening them needs a reason, and in two cases a new ADR.

**Ghost-hand guidance (dropped 2026-09-01).** A translucent hand demonstrating the grip was considered
and rejected. Grip points already make hand placement automatic — the framework snaps the hand to the
point in the right pose — so a ghost hand would demonstrate something that happens anyway. It also
repeats the always-on-highlight mistake one level deeper: a trainee *shown* the exact grip has not
been assessed on knowing it. Revisit only if the Phase 4 demo surfaces a real case, most likely a
two-handed safety control where both hands must be placed specifically.

**Candidate-selection hysteresis.** Architecture §5.4 describes a sticky-best score bonus. It is
**documented but never implemented**. All hysteresis in the codebase is on input thresholds (grab
value, use value, press depth). Highlight flicker between two adjacent objects is therefore a real
pre-existing gap, not a regression — and the `UFXR_ScoringPolicy` seam is now its natural home.

**`Grab Scope` enum.** Was built, then removed. With a correctly structured object it was identical
to driving the driven mesh; with a badly structured one it made things worse — the object carried
perfectly and then came apart on release, which reads as a physics bug rather than a hierarchy error.

**Far Interaction Policy.** A project-wide toggle for distance grab. Dropped: the per-object checkbox
already says it, and a global setting that silently changes how a specific object behaves between
projects is worse than the one tick that made it so.

**Extensibility, three closed areas.** ADR-010 records what is open and what is closed. Styles and
tiers are closed enums *on purpose* — that is what keeps the state→style map coherent and training's
vocabulary fixed. If a fourth style is ever genuinely needed, migrate styles to **data assets**; do
not simply widen the enum. Locomotion modes are closed by ADR-005 and need a superseding ADR.

---

## 6. Quest smoke test — PASSED 2026-10-08

The standing rule is that a Quest build closes every phase from 2 onward. It had never been run,
because the project carried no Android configuration at all. It has now run end to end on a Quest 3
(Android 14 / SDK 34) and **interaction was confirmed working and smooth in-headset**.

Getting there took two fixes, both invisible on PC and neither producing an error. Both are recorded
as gotchas in §7 and are the clearest argument yet for the device-build-per-phase rule:

1. **Stereo was collapsed** — an identical image in both eyes. Multi-view was fine
   (`bMobileMultiViewEnabled = 1`); the project was on the mobile **deferred** shading path, whose
   lighting pass never sets `RenderTargets.MultiViewCount`. Now `r.Mobile.ShadingPath=0`.
2. **None of FlexXR's own content shipped** — every material and mesh is addressed by a hardcoded
   `FSoftObjectPath` in C++, which leaves the cooker no dependency to follow. All 11 assets were
   absent from the first APK, so the Mesh Hull highlight tier could not have worked on device either.
   Now named in `DirectoriesToAlwaysCook`.

**Known cosmetic difference, not a fault:** the scene reads as sunset on device and midday on PCVR.
That is the template sky stack meeting Android's platform defaults — see §7. It is a property of the
test level's lighting, not of the framework.

### Installing the build — the APK alone is not enough

The package is split: a ~124 MB APK and an ~84 MB OBB holding the cooked content. `adb install` by
itself gives you an app with no content. The full sequence (paths relative to `Binaries/Android`):

```
adb install -r FlexXR-arm64.apk
adb shell rm -r /sdcard/UnrealGame/FlexXR
adb shell rm -r /sdcard/Android/obb/com.LowSzeHao.FlexXR
"C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/DotNET/Android/UnrealAndroidFileTool/win-x64/UnrealAndroidFileTool.exe" ^
    -p com.LowSzeHao.FlexXR -k 951BD0864FB0127E76F8C7B2C85A6672 push main.1.com.LowSzeHao.FlexXR.obb "^mainobb"
```

UE generates `Install_FlexXR-arm64.bat` to do exactly this, but it invokes the file tool as
`.\win-x64\UnrealAndroidFileTool.exe` — **a folder that is not deployed next to the APK** — so the bat
fails at the OBB step. Call the tool by absolute path as above. The `-k` key is per-package and is
printed in the generated bat.

Rebuild and repackage with:

```
RunUAT.bat -ScriptsForProject="G:\Flex_XR_Framework\FlexXR.uproject" BuildCookRun ^
  -project="G:\Flex_XR_Framework\FlexXR.uproject" -nop4 -utf8output -nocompileeditor -skipbuildeditor ^
  -platform=Android -cookflavor=ASTC -clientconfig=Development -build -cook -stage -package -pak -iostore -compressed
```

About 6–7 minutes from clean config. Verify afterwards without a headset: `UnrealPak <utoc> -List`
should list all 11 `/FlexXR/` assets, and extracting `*Engine.ini` from the pak shows the renderer
settings that actually shipped.

Under Git Bash, prefix adb shell commands with `MSYS_NO_PATHCONV=1` or a device path like `/sdcard/...`
is rewritten into a Windows path. `adb pull` needs a **Windows** destination path.

**A headless launch proves nothing about rendering.** With the headset off a head the proximity sensor
suspends the app before the renderer initialises — the log stops around 120 lines, well short of any
stereo or asset-load information. `adb shell am broadcast -a com.oculus.vrpowermanagement.prox_close`
makes the device behave as if worn, if a full runtime log is needed without wearing it.

**Watch for on device:** `Highlight Tier = Auto` resolves to **Mesh Hull** on Quest, not the
post-process outline used on PC. If highlights look wrong on device but right on PC, start there.

---

## 7. Hard-won gotchas

Each of these cost real time. They are not obvious from the code.

- **`r.CustomDepth=3`** (Enabled *with* Stencil) is required or the Post Process outline draws
  nothing. At `1` the stencil goes nowhere readable. Mesh Hull needs neither custom depth nor a
  post-process chain.
- **Materials referenced from C++ must be `TSoftObjectPtr`.** A hard `ConstructorHelpers` reference
  roots the asset, and `MaterialEditingLibrary` asserts `!IsRooted()` — the regen script takes the
  editor down with it.
- **`regen_fxr_materials.py` must run from the editor console, not unattended.** Deleting an existing
  asset needs a confirmation a commandlet cannot give, so `create_asset` then refuses the name and
  every already-existing material is silently skipped — while the commandlet reports success.
- **A component built at runtime defaults to `Static` mobility** and silently discards every transform
  update. Set `Movable` (and usually `SetAbsolute`). This cost a long hunt for an invisible laser.
- **Translucent and additive materials rendered nothing** on plugin meshes where opaque drew
  immediately, and raising emissive to 60 did not recover them. The beam, the arrow and the hull are
  all opaque, and they fade **geometrically** (thinning or growing) rather than by opacity.
- **Unlit emissive above 1.0 clips after tonemapping** — a cyan beam comes out white. Keep intensity
  at or below 1 unless you want the clip.
- **Parked physics:** anything that parks an object kinematic must call
  `UFXR_Grab::NotifyParkedPhysics(bWasSimulating)`. Otherwise the next grab reads the parked body,
  records "never simulated", and the object can never fall again after release. This bit twice.
- **Never `CreateDefaultSubobject` in a *component* constructor** — it breaks the Blueprint SCS and
  hard-crashes the editor on load.
- **A module that directly references another module's `UCLASS` must list it in `Build.cs`.** A
  transitive public dependency gives header access and still fails to link.
- **Grab never writes scale.** A grip point attached beneath a mesh inherits its scale, so
  `GripPose.GetRelativeTransform(Held)` divides the scale by itself and yields a unit-scale offset
  that would overwrite the authored size. Handled centrally in `SetHeldTransform`.
- **Activation radius is absolute cm; rail length scales with the object.** The radius is hand
  ergonomics — a hand is a hand — while a rail is geometry. Different questions, different answers.
- **Mobile deferred shading silently destroys stereo.** `r.Mobile.ShadingPath` must be `0` (forward)
  for Quest. `MobileDeferredShadingPass` builds its own PSO render-target info and never sets
  `RenderTargets.MultiViewCount`, while every other mobile pass does — so with mobile multi-view the
  lighting pass resolves one view and **both eyes receive the same image**. Nothing warns. The engine
  logs `bMobileMultiViewEnabled = 1` and the HMD agrees, so the usual multi-view checks all look
  healthy. Forward also admits MSAA (deferred cannot — `RendererSettings.cpp` quietly forces mobile AA
  back to `None`) and turns on the tonemap subpass, which wants Mobile HDR **on**, multi-view on and
  deferred off. That is why `r.MobileHDR=True` is correct here and must not be "fixed".
- **Plugin content referenced only by `FSoftObjectPath` in C++ is never cooked.** A soft path built
  from a string literal in a constructor leaves no asset-registry dependency edge, so the cooker
  never learns the package exists. The first Quest build shipped with **none** of FlexXR's materials
  or meshes. They cannot be hard references (see the `MaterialEditingLibrary` note above), so the
  folders are named in `DirectoriesToAlwaysCook` in `DefaultGame.ini`. Add any new plugin content
  folder there. Symptom on device: `LogStreaming: Warning: SkipPackage: /FlexXR/... does not exist on
  disk or in the loader`.
- **PC and Quest will never agree on the UE5 default sky, and it is not a bug.** `FlexXR_Development`
  is lit by the template stack — SkyAtmosphere, VolumetricCloud, a real-time-capture SkyLight,
  directional light and height fog. On Android the engine disables volumetric clouds outright
  (`r.VolumetricCloud.Support=0`, `BaseAndroidEngine.ini`) and clamps SkyAtmosphere to the cheap path
  at **every** quality level — `[EffectsQuality@0]` through `@3` in `AndroidScalability.ini` all set
  `FastSkyLUT=1`, a 96×50 LUT, 1–8 samples, aerial perspective at one sample per slice. Undersampled
  scattering underestimates transmittance, so the sky comes out warmer and dimmer. Because the
  SkyLight captures in real time, it then feeds that sky back as ambient on **every surface**, which
  is why the whole scene reads as sunset on device while PCVR looks like midday. Raising scalability
  cannot fix it; the Android values are identical at all four levels.
  The fix is to stop lighting a Quest target from a live desktop sky: capture the SkyLight to a fixed
  cubemap, drop the cloud component (it only ever renders on PC, so it actively misleads), and bake
  the demo level. Do **not** raise the Android sky cvars to chase parity — that spends GPU on a
  90 fps target for a sky the training demo does not care about.
- **Editor file locks:** the editor holds `Content/*.uasset` open. `git checkout` of a content file
  fails with `unable to unlink ... Invalid argument` while it is running, and a branch switch can
  abort half-applied. Close the editor first. **Never `git clean -fd` in this repo.**

---

## 8. Known flaws, unfixed

Small and recorded so they are not rediscovered as mysteries.

- **`UFXR_ScoringPolicy` is marked `Blueprintable` but `ScoreCandidate` is a plain C++ virtual**, not
  a `UFUNCTION`. A Blueprint subclass compiles and its override is never called — worse than not
  being Blueprintable at all. Should either drop `Blueprintable` (matching ADR-010, which says
  extension is C++) or be made properly overridable.
- **The guidance arrow requires manual wiring per step.** Today something must fetch the pawn, get
  the component, and call `Point To Component`. A design exists but was not built: have the arrow
  *follow* `UFXR_HighlightSubsystem::SetGuidance` automatically, so one call drives both the glow and
  the arrow, and nothing touches the pawn. Dependency direction allows it (`FXR_UI` → `FXR_Interaction`).
- **The plugin is not self-contained for packaging.** FlexXR's materials and meshes ship only because
  *this project* names them in `DirectoriesToAlwaysCook`. Anyone who drops the plugin into their own
  project gets a build with no ray, no reticle, no arc, no vignette and no outline hull, and no error
  explaining why. That is a poor first impression for a framework meant to be consumed. The proper fix
  keeps it inside the plugin: subscribe to `UE::Cook::FDelegates::ModifyCook` from a module's
  `StartupModule`, or give the plugin an asset-manager rule, so enabling it is enough. Worth doing
  before any 1.0 claim.

---

## 9. Next actions, in order

1. **Test the guidance arrow** — the last piece of Phase 3 that has never been seen rendering. Its
   material now ships on device (it did not before), but nothing has confirmed it draws on either
   platform. `BP_FXR_Pawn` already carries the component; drive it from PIE with `Point To Component`
   against a test interactable.
2. **Close Phase 3** — PR into `main`, tag `v0.4-ui`.
3. **Phase 4 — `FXR_Training`** — the SOP step graph (ADR-004) and the fire-safety demo. Build the
   validation panel and spatial UI kit *inside* this phase, where a real consumer defines what they
   need. Light that level for the device from the start rather than with the desktop template sky (§7).

Done: the Quest smoke test (§6) — installed, launched and confirmed in-headset on 2026-10-08.

Phase 4 and 5 matter most for the portfolio: a training demo built entirely on the framework, and a
performance case study, are what prove the thesis.

---

## 10. Repo facts worth knowing

- **Repo root is `G:\Flex_XR_Framework\`** (flattened 2026-07-11). An empty leftover `FlexXR\` folder
  from the old nesting may still be pinned as a working directory by tooling — run git and build
  commands against the parent.
- `origin` = `https://github.com/LOWSZEHAO/Flex_XR_Framework.git`. Branch per phase, PR into `main`,
  tag at the end of each phase.
- `.uasset` / `.umap` / media are on **Git LFS**.
- Test content (`BP_Table`, `BP_Interactable`) is scaffolding for trying features in the level, not
  framework content.
