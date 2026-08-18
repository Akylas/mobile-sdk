---
title: Lighting, sky & fog
description: One directional sun, cascaded shadow maps, the shader sky, and fog shared by ground and sky.
sidebar_position: 8
---

# Sun, shadows, sky, fog

Scope: everything about how the scene is lit and what fills the frame beyond the ground.

## One resolution point

`all/native/components/StyleEnvironment.h` is where the app's options and the style's opinions are
merged, and **every consumer must go through it** or the ground and the sky end up lit differently:

- `resolveLighting(LightOptions, StyleEnvironment) -> ResolvedLighting` — sun direction, colour,
  intensity, ambient, and the shadow parameters (strength, bias, softness, distance, map size,
  cascade count, caster margin).
- `resolveFog(FogOptions, StyleEnvironment, ResolvedLighting, cameraDistance) -> ResolvedFog` —
  colour, atmosphere colours, range and horizon blend, with the colour **lit by the same sun** (dark
  at night, warm at a low sun). It also turns the camera-relative range into internal units, which
  is the only place that conversion happens.

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

**The range is a multiple of the camera-to-focus distance, never a number of metres.** mapbox's model
verbatim (`3d-style/render/shadow_renderer.ts`: `cascadeSplitDist = cameraToCenterDistance * 1.5`,
`shadowCutoutDist = cascadeSplitDist * 3.0`), which is also the unit `FogOptions` already uses for its
range. `SHADOW_CUTOUT_DISTANCE_FACTOR = 4.5` in `GLTileRenderer.cpp`; `LightOptions.ShadowDistance`
overrides it, `0` takes the default.

The camera-to-focus distance follows the **zoom alone** (`ViewState::_zoom0Distance / 2^zoom`), so one
factor holds from a city to a massif. That is the whole reason for the unit.

Dead ends this replaced, both of them metric:

- **A texel budget** (`TARGET_SHADOW_TEXEL_METERS x mapSize`, 10 m x the page = ~10 km at 1024). It
  bounded how far shadows reach by how well they can be drawn, which is the right *idea* and the
  wrong *quantity*: ~10 km at every camera means a mountain's shadow ends one screen away at z12,
  and no amount of panning brings it back. That is what this replaces.
- **A slant clamp on the frustum rays** (`t1 = maxDistance / length`, applied *before* the slab
  intersection). From a high oblique camera the eye is further from the ground than the cutout is
  long, so every sampled ray failed `t1 < t0`, the ground range came out empty, and the fit fell
  back to the **whole tile cover** - one enormous box per cascade, which on screen is long shadows
  everywhere and square. Panning a little put one ray back across the slab and the normal wedge
  returned: a flip-flop between "pixelated everything" and "cut too near". The cutout now applies to
  the resulting ground *range*, and the fallback box is bounded by the cutout radius around the
  camera instead of by the cover.

The earlier texel-budget measurements (Crosscall, z14, per cascade + caster tiles: tilt 45
5.4/14.3/28.7 m 242 tiles → 3.9/9.0/19.1 m 176 tiles; tilt 30 3.3/13.1/52.6 m 205 tiles →
1.3/2.7/10.7 m 121 tiles) are kept for the shape of the effect, not as current numbers - the
camera-relative range is longer at a low zoom and shorter at a high one. **Not re-measured.**

Shadows are **present at every tilt** from 90 down to 5 (the demo clamps at 30; `--es freeRoam look`
opens the range): `shadows ACTIVE`, boxes fitted, no dropouts.

Known gap: the caster count still grows with the range at a low zoom, where 4.5 x the camera distance
is tens of kilometres. It is bounded by the visible tile cover (the box only *culls* casters, it does
not create them), but the per-cascade cost at z11-z12 has not been measured against the old cap.

Shadows are **present at every tilt** from 90 down to 5 (the demo clamps at 30; `--es freeRoam look`
opens the range): `shadows ACTIVE`, boxes fitted, no dropouts.

### The map is the depth buffer

Where a depth texture can be sampled — ES3 core, or `GL_OES_depth_texture` / `GL_ANGLE_depth_texture`
— `TerrainShadowMap` attaches a **`DEPTH_COMPONENT24` texture as the depth attachment and has no
colour attachment at all**. The caster pass then writes depth alone; before, it wrote depth to a
renderbuffer *and* a packed-RGB copy of `gl_FragCoord.z` to an RGBA8 target, and the receiver
unpacked it with a `dot`. The atlas goes from RGBA8 + D16 to D24, the caster fragment shader's
packing is masked off (`glColorMask(FALSE)`), and the receiver's `shadowDepth()` is a plain `.r`
read under `SHADOW_DEPTH_TEXTURE`.

24 bits, not 16: the packed path spread `gl_FragCoord.z` over three bytes, so a D16 texture would
have *lost* precision and bought acne back. ES2 + `OES_depth_texture` has only the unsized form and
takes `UNSIGNED_SHORT`.

Two things worth knowing:

- **A depth-only framebuffer is complete by the ES3 spec**, and is on the Metal-backed emulator
  (`OpenGL ES 3.0 (4.1 Metal - 90.5)`). It is not guaranteed on ES2 drivers, so an incomplete
  status falls back to the packed-colour map rather than to no shadows.
- **The packed path stays.** iOS builds against MetalANGLE (`libs-external/angle-metal`), whose
  README records the build being patched down to ES2 for 32-bit devices, and `MapView` still has an
  ES2 fallback on both platforms.

### Hardware PCF, and the ESSL 3.00 programs

`GL_EXT_shadow_samplers` is reported **absent on both the emulator and the Crosscall** (Adreno,
`OpenGL ES 3.2 V@0502.0`) — for the reason that makes it good news: it is an *ES2* extension, and a
driver does not advertise it on an ES3 context because `sampler2DShadow` is **core in GLSL ES 3.00**.
The hardware has it; only the shading language could not reach it.

So the shadow-receiving programs are compiled as **GLSL ES 3.00** (`ESSL3` / `SHADOW_HW` flags), from
the same shader sources as everything else. The version difference is a prelude in
`createShaderProgram`: `attribute`→`in`, `varying`→`in`/`out`, `texture2D`→`texture`, and a declared
`out vec4 glFragColor`. mapbox does exactly this — their fragment shaders write `glFragColor`, which
is one of those macros.

The one source-level cost: **a fragment shader writes `glFragColor`, never `gl_FragColor`.** A name
beginning with `gl_` cannot be `#define`d, so that one had to be a real rename (24 sites); the 1.00
path defines `glFragColor` back to `gl_FragColor`.

`shadowTap()` then becomes one `texture(sampler2DShadow, vec3(uv, ref))` — four depth compares and
their bilinear average, in the texture unit — where the 1.00 path does a fetch, an unpack and a
compare. The texture is bound with `TEXTURE_COMPARE_MODE = COMPARE_REF_TO_TEXTURE` and `LINEAR`
filtering, which is only meaningful *because* the comparison happens before the filter.

Measured on the emulator, Grenoble z12.53 tilt 26 sun 17.783 UTC: **123,401 px of 2,592,000 differ**
from the manual-tap path — softer shadow edges, as 2x2 filtered comparisons should be.

**It buys quality, not speed, and that was measured.** Interleaved A/B on the Crosscall (Adreno 610,
`-PprofileRender`, Grenoble z13 tilt 30, `base composite`, `shadow 0.6`, panning, medians over 42
one-second windows):

| Configuration | fps | GPU drape |
|---|---|---|
| `shadow 0` | 23.9 | 1.2 ms |
| `shadow 0.6`, hardware PCF, 4 taps | 14.6 | 6.0 ms |
| `shadow 0.6`, manual taps, 4 taps | 14.5 | 6.0 ms |
| `shadow 0.6`, hardware PCF, 1 tap | 14.6 | 6.2 ms |
| `shadow 0.6`, hardware PCF, 1 cascade | 17.0 | 4.8 ms |

Hardware PCF is **within noise of the manual path**, and so is dropping from four taps to one. The
tap count was never the cost: four hardware taps are the same four texture fetches, each now doing
four compares instead of one, so the change is 16 effective samples for the price of 4. What the
shadow feature actually costs at this camera is ~4.9 ms of drape, and **1.4 ms of it is the cascade
count** — one shadow matrix per vertex and one `highp vec3` varying per cascade, which is what
§"Where the shadow cost actually is" already concluded from a different angle.

So the next perf step is cascades, not sampling: mapbox's `computeRequiredCascades` (a cascade
nothing lands in is never drawn) and fewer, larger pages.

An ESSL 3.00 program that fails to build falls back to its 1.00 form rather than taking the map with
it (`hasShaderVersionFallback()`).

### An unrelated compile failure this uncovered

Adding the version prelude shifted shader line numbers, which surfaced a **pre-existing** failure:
`tilecolormap` built with `TERRAIN_LIGHT` + `TERRAIN` calls `terrainNormal()` and reads `uSunDir` /
`uSunColor` / `uLightParams`, none of which `colormapFsh` declares — they live in `backgroundFsh`,
a different string, or come from the application's lighting shader when one is installed. Without
that shader the program does not compile. It is caught by `TileRenderer::prepareFrame` and logged, so
it is invisible unless you grep for it: **6 occurrences per run on the committed build**, before any
of this work. Not fixed here.

Compile errors now quote the source around the reported line and list the program's defines — the
line number is into the concatenated source, which nothing on disk matches, and without the quote
every shader error costs a round of guessing.

### Why building shadows are softer than mapbox's, and what to set

At z16 a building's cast shadow is quantised into visible steps. Measured on the Crosscall,
Grenoble z16 tilt 45, same camera forced by broadcast, identical crops:

| Configuration | fps | GPU drape | Look |
|---|---|---|---|
| 1024 x 3 (the default) | 14.1 | 5.8 ms | washed courtyard shadows |
| 2048 x 2 (mapbox's) | 13.6 | 5.8 ms | tight, defined edges |

**The cause is the texel, not the filter and not the mask.** With the range at 4.5 x the
camera-to-focus distance (~7 km at z16) split three ways, the near cascade covers ~1.5 km through a
1024 page — about **1.5 m of ground per texel**, which is metres-wide steps across a street. mapbox
puts the same near-cascade distance through a 2048 page and spends nothing on a third cascade.

Set `shadowMapSize 2048` + `shadowCascades 2` for the sharp look. It is nearly free: drape is
unchanged and the 0.5 fps is device drift, because the per-cascade cost is matrices and varyings
rather than sampling (see the table above — one cascade is *faster* than three). The reason it is
not the default is memory: 2048 x 2 at D24 is ~33 MB of atlas against ~12.6 MB for 1024 x 3. mapbox
pays ~16 MB for theirs by using D16; at a 0.75 m texel the precision argument for D24 is weaker than
it was, so D16 at 2048 is worth trying — **not tested**.

**Dead end: the screen-space mask is not the limiter.** `SHADOW_MASK_DIVISOR = 1` (full resolution)
was tried on the theory that a quarter-resolution mask was blurring building shadow edges. It made
them look *worse*: the full-res mask exposed the shadow-map staircase that the quarter-res blur had
been smoothing over. The mask hides quantisation, it does not cause it. Leave it at 4 — it is one of
the biggest wins in this file (§"The screen-space shadow mask") and costs no sharpness that the map
itself can resolve.

### Normal offset

`LightOptions.ShadowNormalOffset` (default 3, mapbox's) pushes a receiving surface **along its own
normal** before it looks itself up, by that many shadow-map texels, scaled by
`min(1 - N.L, 1) * 0.5 + 0.5` — mapbox's curve
(`3d-style/shaders/_prelude_shadow.vertex.glsl`). It clears acne by moving the sample *sideways*
rather than lifting its depth, which is what lets the depth bias stay small enough for the shadow to
stay attached to the foot of the wall casting it.

It applies to **3D extrusions only**. The terrain surface takes its normal per fragment from the DEM
(`terrainNdl`), and a vertex-stage offset cannot reach that.

Two things that are ours, not mapbox's:

- **The per-cascade offset is CLAMPED to the near cascade's world size.** The offset moves the
  sample across the shadow map, so on a far cascade — whose texel is metres of ground — three of
  them walk a roof out of the mountain shadow it stands in, and raising the value takes more of the
  roof with it. mapbox never sees this: two cascades over a shorter range, so their worst texel is
  small; we have up to four over 4.5 x the camera distance. Verified on the emulator at Grenoble
  z17 tilt 40, sun 17.6 UTC, strength 0.9: offset 0 vs **8** (the demo slider's maximum) differs by
  28,351 px of 2,592,000 with the roof shadows intact.
- **The texel size is read back off the light matrix** (`2 / (len(row 0) * mapSize)`, the ortho
  scale, since the light view is a pure rotation) rather than threaded through `MapRenderer`. The
  offset and the box it belongs to then cannot drift apart.

Known gap: no camera has yet been found where the offset *earns* its artifact — at
`shadowBias 0` the same views are already acne-free with the offset at 0. It is on by default
because it is mapbox's default; the case for it is a lower depth bias, which has not been retuned.

## Buildings

`TileRenderer::LIGHTING_SHADER_3D` lights extrusions, and it is installed **per vertex**
(`LightingShader(true, ...)`). That has a consequence worth knowing before touching it: any function
of height in there only reaches the screen through the values at the base ring and at the roof - the
wall carries the linear interpolation between them, whatever curve the formula draws. A falloff
"over the first metre" is therefore a full-height ramp on screen, and the only thing that changes the
look is the endpoint value.

The ambient term at the foot of a wall (`1 - 0.65/(1 + h*h)`) is the cue that makes an extrusion
stand on the terrain rather than float over it: that corner is occluded by the ground and by the
building's own footprint whatever the sun does, and the shadow map cannot resolve it - its texels are
metres wide. Measured luminance down a wall on the device: 206 at the roof, 100 at the foot (124 with
the previous 0.5, which read as too light).

Contact darkening on the GROUND around a footprint is the other half and is not implemented: the
ground does not know where the buildings are. It would need either screen-space AO over the scene
depth (too expensive on an Adreno 610, where the whole 3D pass is ~9 ms) or a halo drawn by the
style, which is a styling decision rather than an engine one.

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

Fog lives on its own `FogOptions` (it used to be three fields on `TerrainOptions`), on the Mapbox
`fog` model: colour, `high-color`, `space-color`, `horizon-blend`, `star-intensity`, and a **range in
multiples of the camera-to-focus distance** rather than in metres.

Camera-relative is the load-bearing choice. `ViewState::calculateCameraDistance()` is tangram's
`m_pos.z` — a function of the zoom alone, so it does not move with tilt or with the terrain under the
camera. A metric range had to be retuned for every zoom, and a range tuned for a city view painted a
mountain view solid.

**Resolved once per frame, before anything draws.** `MapRenderer::collectStyleEnvironment` merges
every tile layer's Map-block opinion and `_frameFog` is resolved from it; the sky, the background
plane and the terrain surface are all handed that value. They used to each call `resolveFog` with an
**empty** `StyleEnvironment`, which only `TileRenderer` filled in — so a fog declared by a style
reached the tile content and nothing else: hazy ground under a clear sky, and a `fog-enabled: 0` the
sky ignored. The collection has to happen before the sky draws, which is long before `drawLayers`
would have gathered it.

Four consumers, all through the same resolved value, so the horizon matches:

| Consumer | How it fogs |
|---|---|
| vt (`setFog` + `applyFog` in `commonFsh`) | per-fragment, `1/gl_FragCoord.w` scaled into range units |
| `BackgroundRenderer` | the same expression in its plane shader |
| `SkyRenderer` | angular, `fogAmount(rayDir)` — the sky has no distance |
| `TerrainRenderer::renderSurface` | the relief surface's `u_fogRange`, which is in **metres** (`v_dist = pos.w * u_metersPerUnit`), so it converts |

**Fog does not depend on the terrain.** `BackgroundRenderer::setupFogUniforms` used to gate on
`terrainOptions->isEnabled()` while `TileRenderer` did not, so a plain 2D map fogged its tile content
and left the background plane — everything past the loaded tiles, which is exactly the far distance
you want fogged — untouched. The gate is gone.

**The drape bake must never fog** (`fogFlag()` returns 0 while `_drapeMVPOverride` is set). The bake
is flat content baked into a texture that is then painted on the terrain surface and fogged there,
once; anything fogged in the bake is *burnt into a cached texture* and survives the fog being turned
off. This used to fall out of the arithmetic — an orthographic pass has `gl_FragCoord.w = 1`, a whole
world in internal units (2²⁰), which no metric range ever reached — and a camera-relative range does
reach it at high zoom, where the camera distance approaches 1. The symptom was a fog wash left over
the tiles after switching fog off, while the sky above the horizon updated correctly.

**`Enabled` is a real switch, not a value driven to zero.** `resolveFog` returns a default
`ResolvedFog` (which is not `active()`) when it is off, so every consumer stops together and nothing
has to round-trip a colour or a range through 0.

`BackgroundRenderer` draws the flat z=0 plane that fills the view past the terrain and past
`TerrainOptions::ViewDistanceFactor`. It uses `Options::getBackgroundBitmap()` — **not** the CartoCSS
`Map { background-color }`, which is why changing the style background does not tint it.

`ViewDistanceFactor` ends the ground; pair it with fog or it ends on a hard edge.

### The custom fog shader

`FogOptions::setShaderSource` supplies `vec4 applyFog(vec4 color, float amount, float dist)` and it is
compiled into all three fog paths, so one function covers map and sky in 2D and 3D. Mechanics worth
knowing before touching it:

- **One uniform naming, everywhere.** vt's names (`uFogColor`, `uFogHighColor`, `uFogSpaceColor`,
  `uFogParams`) are the contract. `BackgroundRenderer` was renamed to them; `SkyRenderer` keeps its
  own `u_*` sky-shader contract and adds `#define` aliases rather than a second set of uniforms.
- **`uFogParams` is in range units** (`start`, `1/(end-start)`, internal→range scale, `end`). Every
  path converts `gl_FragCoord.w` first, at the cost of one multiply, so the numbers a custom shader
  sees are the numbers the API and the style are written in.
- **vt substitutes at `$FOG_BLEND$`** inside `commonFsh` — it has to sit after the uniform
  declarations it reads and before the `applyFog(color)` that calls it. `setFogShaderSource` deletes
  every program and clears both caches; it is called from `TileRenderer::onDrawFrame`, i.e. the GL
  thread, which is what makes that safe.
- **The built-in sky no longer mixes the fog itself.** `skyColor` returns the unfogged colour (plus
  the coverage the haze supplies below the horizon) and `main()` applies `applyFog` once, so a custom
  fog shader reaches the sky instead of being overridden by it.

## Known gaps

- Zoom **blinking with fog on** is reported and not diagnosed: a terrain tile from the zoom being
  left behind stays drawn and is fogged (or lit) differently. The mismatch is a depth/stand-in
  problem that fog only makes visible — it also shows with daylight and no fog. Worth re-checking
  now that the bake no longer burns fog into the cached drape.
- **`high-color`, `space-color` and stars are only visible well up the sky.** `atmosphereColor`
  ramps on the elevation angle (`smoothstep(0, 0.6, elevation/90°)` for the high colour), and the
  demo clamps tilt at 30, where the sky band in frame is a few degrees above the horizon and the
  ramp is still near zero. They need `--es freeRoam look` and a negative tilt to see at all, and
  neither has been checked on a device.
- **What is checked, on the emulator:** fog in 2D with terrain off; no burnt-in wash at z17 with the
  drape on; a style-declared fog reaching the sky and the ground with no seam; enable/disable
  toggled from adb over repeated cycles, direct and style source. The last one is what turned up the
  double-buffer rule in [01-frame.md](01-frame.md) — the fog was only the messenger.
