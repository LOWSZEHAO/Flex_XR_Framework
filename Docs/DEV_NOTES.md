# Dev notes

Things that cost me real time on this project and aren't obvious from the code. Mostly Unreal 5.8
behaviour, a few Quest-specific.

## Rendering and Quest

**Mobile deferred shading breaks stereo on Quest.** With mobile multi-view on, the deferred lighting
pass (`MobileDeferredShadingPass`) builds its own PSO render-target info and never sets
`RenderTargets.MultiViewCount`, while every other mobile pass does. The result is that both eyes get
the same image, and nothing warns: the log says `bMobileMultiViewEnabled = 1` and the HMD agrees.
`r.Mobile.ShadingPath` has to be `0` (forward). Forward also allows MSAA (deferred quietly forces
mobile AA back to None) and the tonemap subpass, which is why `r.MobileHDR=True` is correct here.

**Plugin content referenced only by a soft path in C++ never gets cooked.** A soft path built from
a string in a constructor leaves no dependency edge for the cooker. My first Quest build shipped
with none of the plugin's materials or meshes. They can't be hard references (next note), so the
plugin folders are listed in `DirectoriesToAlwaysCook` in `DefaultGame.ini`. On device the symptom
is `LogStreaming: Warning: SkipPackage: /FlexXR/... does not exist`.

**Hard-referencing a material from C++ roots it**, and `MaterialEditingLibrary` asserts
`!IsRooted()`, so the material regen script crashed the editor. Materials referenced from C++ are
`TSoftObjectPtr`.

**`r.CustomDepth=3`** (enabled with stencil) is needed for the post-process outline. At `1` the
stencil isn't readable and nothing draws. The mesh-hull outline (used on Quest) needs neither.

**Translucent and additive materials drew nothing** on the plugin's meshes, while opaque worked
straight away. The ray beam, arrow and outline hull are all opaque, and they fade by scaling
(thinning or growing) instead of by opacity.

**Unlit emissive above 1.0 clips after tonemapping**, so a cyan beam comes out white.

**PC and Quest won't match on the default UE5 sky.** Android disables volumetric clouds and clamps
SkyAtmosphere to the fast LUT at every scalability level, and a real-time SkyLight feeds that dimmer,
warmer sky back as ambient light. On device the level looks like sunset, on PC like midday. The fix
is a captured SkyLight and baked lighting for the demo level, not raising the Android sky settings.

## UMG and world-space panels

**`bDrawAtDesiredSize` has to be off** on a widget component whose widget has a Canvas Panel root.
`DesignTimeSize` is editor-only (`UserWidget.h:1528`) and `SConstraintCanvas::ComputeDesiredSize`
returns the largest child offset, not the canvas size, so a 900 x 870 widget rendered into a
~500 x 500 target and got stretched. Everything looked soft.

**`UImage::SetDesiredSizeOverride` stores nothing.** It forwards to the live Slate widget, so on an
image built in code (not constructed yet) it does nothing and the image falls back to 32 x 32. All
my 3 px dividers came out 32 px tall. Set the size in the brush (`ImageSize`) instead.

**UMG font sizes are points.** Slate renders at 96 DPI, so "24" from a CSS design draws at 32 px.
Line height is different too: CSS multiplies the font size, UMG's Line Height Percentage multiplies
the font's natural line height (1.219 em for Montserrat). Both are converted in one place,
`FXR_PanelStyle`.

**Rebuilding a Widget Blueprint tree in code:** `RemoveWidget` leaves the old widget outered to the
tree under its name, so creating a new widget with that name and a different class is a fatal
"Cannot replace existing object of a different class". Rename the old widgets into the transient
package first. Also empty `WidgetVariableNameToGuidMap`, otherwise the compiler hits an ensure for
every widget (125 callstacks, an 84 ms build turned into 14 s).

**Compiling a Widget Blueprint blanks its live instance in the editor world** and doesn't rebuild
it. Save and reload the level. Widget components are also one-sided, so from behind a panel is
simply invisible.

**`UWidget` has a member called `Slot`**, so a local named `Slot` in a widget method is C4458, which
is an error with warnings-as-errors.

## Engine and C++

**A component created at runtime defaults to Static mobility** and ignores every transform update.
Set Movable (and usually absolute). This was a long hunt for an invisible laser.

**Never `CreateDefaultSubobject` inside a component's constructor.** It breaks the Blueprint SCS and
crashes the editor on load.

**A module that uses another module's types directly needs it in `Build.cs`.** A transitive public
dependency gives you the headers and then fails to link.

**Parked physics:** anything that switches a grabbable object to kinematic (a socket, for example)
has to call `UFXR_Grab::NotifyParkedPhysics`. Otherwise the next grab reads the parked body, thinks
it never simulated, and the object never falls again after release. This bit me twice.

**Physics only welds to a mesh parent.** `WeldTo` looks for the root body by walking up primitive
parents only, so attaching to a plain scene component (a socket) attaches but silently never welds.
Attach to the nearest mesh above it instead.

**A line trace that starts inside a shape hits it at the start**, with `bStartPenetrating` set and
the normal pointing straight up, so it looks exactly like a floor. `bFindInitialOverlaps = false`
does not help: it only applies to sweeps. Check `bStartPenetrating` and retrace with that component
ignored.

**Don't move a welded component directly.** Setting its transform rebuilds the weld (unweld and weld
again) on every call. A welded child moved by its parent is free. So weld after anything stops driving
it, and detach before anything starts.

**A skeletal mesh only animates in the editor world with Update Animation In Editor on**, and
`SetUpdateAnimationInEditor` silently does nothing before the component is registered. The Anim BP
also evaluates once as it registers, so set whatever it reads before `RegisterComponent`.

**A material used on a skeletal mesh needs Used With Skeletal Mesh set.** Otherwise the editor sets
it the first time the material lands on one and leaves the asset unsaved, which shows up later as a
mystery save prompt.

**Grab never writes scale.** A grip point under a scaled mesh inherits its scale, and the relative
transform maths would produce a unit-scale offset that overwrites the object's size.

**Activation radius is absolute cm, rail length scales with the object.** A hand is a hand, but a
rail is part of the object's geometry.

**Run `Tools/regen_fxr_materials.py` from the editor console, not as a commandlet.** Deleting an
existing asset needs a confirmation a commandlet can't give, so existing materials get skipped while
the commandlet still reports success.

**Close the editor before switching branches.** It keeps `Content/*.uasset` open, so `git checkout`
fails with `unable to unlink` and can leave the branch switch half applied.

---

# Known issues

- **Exam mode hasn't been played in the headset yet.** It's covered by automation tests and was
  checked in PIE, but failing on a timeout or a critical mistake, the report's buttons and poking the
  panel with a finger haven't been tried with real hands.
- **`UFXR_ScoringPolicy` is `Blueprintable`, but `ScoreCandidate` is a plain C++ virtual**, so a
  Blueprint subclass compiles and its override is never called. Either drop `Blueprintable`
  (ADR-010 says extension is C++) or expose it properly.
- **The plugin isn't self-contained for packaging.** Its materials and meshes are only cooked because
  this project lists them in `DirectoriesToAlwaysCook`. Another project using the plugin would get no
  ray, arc, vignette or outline hull. Fix: add them from the plugin itself (a `ModifyCook` delegate or
  an asset manager rule).
- **The guidance arrow is wired by hand per step.** The plan is for it to follow
  `UFXR_HighlightSubsystem::SetGuidance` automatically, so one call drives both the glow and the
  arrow.
- **Curved panels can't be poked** (`TracePoke` refuses cylinder geometry). Ray pointing still works.
- **Small black tearing at the outer edge of the right eye on Quest**, only across high-contrast
  edges (like a horizon). Not foveation (confirmed off in the device log). Next things to test are
  image-based VRS (`r.Vulkan.VRSFormat=0`) and the MSAA resolve. Settings can be A/B tested on device
  without repackaging by pushing a `UECommandLine.txt` with `-ForceDPCVars=...`.
- **PC and Quest brightness differ.** It comes from the mobile renderer and the Quest device profile
  lowering post-process quality, not from the level. If the device needs to be brighter, tune it in
  `Config/Android/AndroidEngine.ini` rather than chasing exact parity.
