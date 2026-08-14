# Sun, shadows, sky, fog

Scope: everything about how the scene is lit and what fills the frame beyond the ground.

## One resolution point

`all/native/components/StyleEnvironment.h` is where the app's options and the style's opinions are
merged, and **every consumer must go through it** or the ground and the sky end up lit differently:

- `resolveLighting(LightOptions, StyleEnvironment) -> ResolvedLighting` — sun direction, colour,
  intensity, ambient, and the shadow parameters (strength, bias, softness, distance, map size,
  cascade count, caster margin).
- `resolveFog(TerrainOptions, StyleEnvironment, ResolvedLighting) -> ResolvedFog` — colour, start
  distance, distance, with the colour **lit by the same sun** (dark at night, warm at a low sun).

The style wins wherever it has an opinion; the rest stays with the options. The first layer to define
a property wins, values are re-read every frame, so they may depend on the zoom.

Consumers: `TileRenderer` (→ vt uniforms), `BackgroundRenderer`, `SkyRenderer`, and
`MapRenderer::applyTerrainShadows`.

## The sun on the ground

The shared ground is the lit surface of the scene, so one directional light shades the whole map.
The lighting state must be resolved **before** the ground draws (`drawLayers` does this), because
each layer otherwise sets the sun from its own `onDrawFrame`, which runs after the ground — the
ground would light itself with the previous frame's sun.

A terrain paint **covers** the ground it is drawn on, so it carries the ground's sun and shadow as
well, from the geometric normal — not from the hillshade's own exaggerated slope. Lighting only the
surface underneath leaves the shading over it unlit, and since a shadow multiplies the lit colour,
nothing shows at all.

## Cast shadows

`MapRenderer::applyTerrainShadows` + `TerrainShadowMap` (all/native/renderers/utils/).

The caster pass draws **exactly the terrain surfaces that are about to be drawn on screen**, from the
sun, into a packed-depth texture; the surface shader then looks itself up in it. Casters and
receivers share one vertex shader and one elevation fetch, so the shadow geometry cannot disagree
with the rendered geometry.

Design points, each measured:

- **Cascades** (up to `TerrainShadowMap::MAX_CASCADES = 4`, default 3 × 1024). Each cascade fits its
  light box to **its own** piece of ground's relief, not the whole scene's — at a low sun that is
  what sets the box size.
- **The shadow sun is not always the lighting sun.** A shadow is as long as the caster is tall over
  `tan(altitude)`: at 9° a 700 m hill throws 4.4 km, a 2 km massif 13 km. The light box stretches by
  the same factor, so the cascade ladder goes coarse (31/53/62 m texels at z12.3 t60, against 11 m
  bounded) and the shadows need casters far outside the drawn cover — what reaches the screen is a
  grey wash that appears and disappears with the cover. The **shadow pass alone** floors the sun
  altitude at 15°, keeping the azimuth, which caps shadow length at ~3.7× the relief. N·L lighting
  keeps the true sun, so a low sun still reads as a low sun.
- **Caster margin.** Casters are taken from the cover plus a ring of neighbours (`shadowCasterMargin`
  tiles): a mountain just off screen still throws its shadow into the view, and without the margin
  its shadow vanishes as you zoom in and it leaves the visible set.
- **The caster set has to stay a partition of the ground.** The cover is a quadtree partition, but the
  ring is generated at each cover tile's own zoom and the cover mixes zooms (up to
  `TerrainMaxTileZoomCoarsening` levels), so the ring around a coarse tile lands on top of the fine
  tiles beside it. Two casters over the same ground at different DEM levels disagree by tens of
  metres, the shallower one wins the depth test, and the receiver — which uses the fine level — ends
  up in the shadow of *its own ground*. On screen: blocky, roughly axis-aligned dark patches that do
  not follow the sun azimuth, appearing from about tilt 60 (SDK convention, 90 = top down) and growing
  as the view flattens, because a flatter view mixes more zooms. `MapRenderer::applyTerrainShadows`
  therefore **subdivides** an overlapping ring candidate against what is already taken, finest zoom
  first, instead of dropping it — dropping would leave the ground outside the finer tiles with no
  caster. Measured (emulator, Grenoble z13 tilt 30, sun 30°/135°, 3 × 1024): 47.6% of the flat valley
  wrongly darkened before, 4.1% after — the same as with no ring at all (`shadowMargin 0`), which is
  the floor. Ruled out on the way: bias (`shadowBias 10` → 42.5%), map resolution (4096 → 52.1%,
  *worse*), cascade count (1 cascade → 36.5%). None of them is the mechanism.
- A tile with **no elevation yet casts nothing**: drawn flat it is a sea-level plane, which is not the
  terrain it stands for, and a receiver without elevation takes no shadow either.
- The map is **snapped and cached** so a stationary camera does not re-render it. The cache is **per
  page**: each cascade's box is snapped to its own lattice, and the outer page — which holds most of
  the casters — keeps its matrix over far more camera movement than the near one. A page that is not
  refreshed keeps the matrix it was drawn with, so the uniforms are taken from what the pages hold,
  not from this frame's fit. Refreshing only the changed pages needs a scissored clear
  (`TerrainShadowMap::clearCascade`).
- **The per-cascade caster cull is exact.** The sides are snapped *before* the casters are culled
  against them, so the cull uses the final box and a one-texel margin; culling against the unsnapped
  box needed a 20% slop, which on the outer cascade is kilometres of ground. Cost of the pass on the
  emulator, panning at z13 tilt 30: 287 tile draws / 1.3 ms per pass → 120 / 0.6 ms with the exact
  cull → 70 / 0.5 ms with per-page refresh, and passes now redraw one page instead of three.

### Where the shadow cost actually is

Measured on the Crosscall (Adreno 610, `-PprofileRender`, Grenoble z13 tilt 30, 3 × 1024, swipe-panned).
GPU `drape` section, which contains the caster pass and the terrain surface draws:

| Configuration | GPU drape | Note |
|---|---|---|
| `shadow 0` | 14.5–17.0 ms | shadow path not compiled in |
| `shadow 0.6` | 46.7–51.1 ms | ~33 ms for the feature, half the frame |
| `shadow 0.02` | 43.9–51.8 ms | shader runs, shadow invisible — **same cost** |
| `shadowCascades 1` | 42.9–46.3 ms | one page, 76 caster draws instead of 102 |
| one PCF tap instead of nine | 36.5–46.3 ms | |
| `meshResolution 16` (vs 64) | 28.1–30.7 ms on, 7.3–7.9 off | feature cost 21 ms instead of 33 |

So the caster pass is about **4 ms** of the 33 and the rest is the **receiver**: the taps are worth
~6 ms and the remainder is per-vertex and per-fragment overhead that scales with the terrain mesh —
one shadow matrix per vertex and one highp vec3 varying per cascade. Optimising the caster pass
further (fewer tiles, coarser caster mesh, cheaper pages) is therefore not where the frame is.

### The screen-space shadow mask

`TerrainShadowMaskBuffer` (all/native/renderers/utils/) + `SHADOW_MASK_OUT` / `SHADOW_MASK_IN`.

The terrain surface covers the whole screen, and where a paint is drawn on the drape it covers it
twice, so the lookup ran once per covering draw per pixel. It is now resolved **once, at a quarter of the screen
resolution**: the same surface tiles are drawn into a half-size target with a fragment shader that
stops at the shadow value (`renderTerrainShadowMask`, the fill path with `SHADOW_MASK_OUT`), and the
real surface draws sample it by `gl_FragCoord.xy * uShadowMaskScale` — one fetch, no cascade choice,
no matrices, no varyings, no taps. The reduced resolution is invisible in the result: a terrain
shadow edge is a penumbra, and the mask is sampled `GL_LINEAR`. A quarter against a half costs
nothing visible and is worth 14-16 ms -> 8-9 ms of mask pass, 8.5 -> 9.6 fps (with the profiler's
own sections in both, which cost about 3 fps themselves). 3D extrusions and undraped lines keep the analytic
path — they are not the terrain surface, so the mask does not hold their shadow.

**Detach the mask texture from its framebuffer before anything samples it.** This is what makes the
mask pay at all: attached, it is still a render target, sampling it in the same frame is undefined,
and this driver serialises every draw that reads it. Measured on the Crosscall, terrain z13 tilt 30,
GPU drape section: 38–49 ms analytic, 37–45 ms with the mask still attached, **23–27 ms detached**;
frame time 33–54 ms → 25–35 ms. The same detach was added to `TerrainShadowMap`; on its own, with the
mask off, it changes nothing (40–41 ms) — it is the mask that needs it. `MapRenderer` already
documents the same trap for the drape bake.

### How far shadows reach

The shadowed ground is bounded by what the map can **represent**, not only by what the view can see:
the outer cascade's texel is its extent over the resolution, so shadowing 50 km through a 1024 page
gives texels wider than the ridges casting into them - a grey wash. `calculateShadowViewProj` caps
the range at `TARGET_SHADOW_TEXEL_METERS x mapSize` (10 m x the page, so ~10 km at 1024), on top of
the existing relief-and-view heuristic.

This is what makes shadows hold up as the view flattens; measured on the Crosscall at z14, per
cascade, with the caster tile count:

| tilt | before | after |
|---|---|---|
| 90 | 7.2 / 10.7 / 14.3 m, 162 tiles | unchanged - the cap does not bind |
| 45 | 5.4 / 14.3 / 28.7 m, 242 tiles | 3.9 / 9.0 / 19.1 m, 176 tiles |
| 30 | 3.3 / 13.1 / 52.6 m, 205 tiles | 1.3 / 2.7 / 10.7 m, 121 tiles |

Sharper *and* cheaper, because a shorter range is also fewer caster tiles: 9.3 -> 10.6 fps at tilt 30.
A city view at z16 is untouched (the view-based term is already smaller there). Nothing is visibly
lost in the distance - past that range the shadows were texels tens of metres wide, and the last
cascade already fades out over its outer margin.

Shadows are **present at every tilt** from 90 down to 5 (the demo clamps at 30; `--es freeRoam look`
opens the range): `shadows ACTIVE`, boxes fitted, no dropouts.

### What did not work

Kept out on measurement, so the next person does not re-try them:

- **Fragment-side cascade selection** (one `vShadowLocal` varying, one matrix applied per fragment,
  cascade picked from `gl_FragCoord.w` against the split distances). Correct, and *slower*: these
  scenes are fragment-bound, so moving a mat4 out of the vertex stage costs more than the two
  varyings it saves. City 3D pass 10.3–11.1 → 13.1–13.8 ms.
- **Extrusions only in the near cascades**, and a **coarser caster mesh for the outer cascades**.
  Neither moved the frame: with *zero* caster draws the pass still showed up to 60 ms of GPU-section
  time in a city frame, so its cost is not the geometry.
- That last number is also a warning about the tool: a GPU section absorbs the GPU's idle time, and
  in the city the frame is not GPU-bound — the CPU frame time is flat (33–63 ms) across every one of
  these builds. Read `PROF GPU` sections against the CPU frame time before believing a regression.
  The open question in a city view is what makes it CPU-bound, not the shadow pass.

The lookup is also **compiled for the cascade count in use** (`GLTileRenderer::shadowReceiverFlags`,
`SHADOW_CASCADES_2/3/4`); it used to declare four matrices and four varyings whatever the count.
Measured on the same scene: at one cascade 44.3 → 37 ms of drape, i.e. **~2.3 ms per cascade per
frame**, so ~2 ms at the default 3 — real, but inside the run-to-run spread. The remaining ~26 ms is
the single-cascade base cost. Getting that down means selecting the cascade in the fragment stage
from view distance, so the vertex stage applies one matrix and interpolates one varying whatever the
cascade count — not yet done.

**Current state: cast shadows are switched OFF on the shared ground.**
`applyTerrainShadows(..., castShadows = false, ...)` — the light, the boxes and the caster pass are
all wired, but with the pass enabled the map reads as scattered **shadow acne** instead of cast
shadows. Half-working shadows are worse than none. To work on it, flip that argument to true.

Tangram-ng has **no** terrain shadows at all, so there is nothing to copy here — this is one of the
few places the fork is ahead of the reference and therefore on its own.

## Sky

`SkyRenderer` draws a full-screen ray-direction quad before everything else, and reports whether it
drew (if it did, the legacy sky band is skipped).

Apps can replace the body with `SkyOptions::setShaderSource`. The wrapper declares
`u_sunDir`, `u_sunColor`, `u_skyColor`, `u_horizonColor`, `u_groundColor`, `u_fogColor`,
`u_fogBlend`, `u_time`, `u_zoom` and a `fogAmount(rayDir)` helper — **redeclaring any of them is a
compile error and the renderer silently falls back to the built-in sky**, which is the usual reason a
custom sky "does nothing".

Two implementation notes: it uses `glGetUniformLocation` with `>= 0` guards (see
[03-vt-renderer.md](03-vt-renderer.md#shaders)), and it draws from a **client-side array**, so any
renderer that leaves a `GL_ARRAY_BUFFER` bound makes the sky quad fly off screen.

Two more, both about not painting what is covered anyway:

- **The quad starts at the horizon**, not at the bottom of the screen — everything below is ground,
  background plane or terrain, all drawn over the sky, and at a tilt that is half the screen of pure
  overdraw. Tangram's sky mesh spans the top half and is translated onto the horizon the same way
  (`core/src/util/skyManager.cpp`). A generous margin is kept below it because the fog band fades
  downwards from the skyline by an amount that is not a straight function of the horizon. The clip
  applies only when the horizon is what bounds the ground; when the terrain path draws the sky
  although the flat horizon says it is not visible (a peak exposing it), the quad stays full screen.
- **The haze starts fading at an elevation angle, not at zero.** Fading from the mathematical
  horizon is right on a flat map, where the skyline *is* the horizon. In the mountains the skyline
  is a ridge, and a ridge stands well above the horizon once the camera is near it: the fog then
  stops at an angle the sky above is already clear at, and hazy ground meets clean sky along the
  silhouette — the "fog does not reach the sky when zoomed in" report, which appears with zoom
  because the angle to a ridge grows as you approach while the horizon stays at zero. The reference
  angle is the highest terrain the view can hold, seen at the distance the fog saturates at.

## Fog and the background plane

Fog is applied by the vt shaders (ground, content, paint) from `setFog(color, startDistance,
distance)` in internal units, and by the sky through `fogAmount`. Because both come from
`resolveFog`, the horizon matches.

`BackgroundRenderer` draws the flat z=0 plane that fills the view past the terrain and past
`TerrainOptions::ViewDistanceFactor`. It uses `Options::getBackgroundBitmap()` — **not** the CartoCSS
`Map { background-color }`, which is why changing the style background does not tint it.

`ViewDistanceFactor` ends the ground; pair it with fog or it ends on a hard edge.
</content>
