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

### Undraped 2D content takes the same sun

Content baked into the drape is lit once, by the surface draw that samples the drape texture. What
is **not** baked is drawn live into the 3D scene and was therefore the only thing in the frame that
kept its flat style colour: a `TerrainOptions.NoDrapeLayerFilter` layer (contours by default), and
everything 2D when the drape is off. `GEOMETRY_LIGHT` in `renderTileGeometry` closes that — point,
line and polygon programs multiply by the same normalised Lambert the surface uses, with N·L taken
from the **terrain** normal (`terrainNdl`, the 3×3 DEM stencil), never from the geometry's own flat
normal. Extrusions are excluded: they light by their own model (see [Buildings](#buildings)).

Shadows already worked this way, so the shading and the shadow now share one `ndl` per fragment
(`applyTerrainShading` in `commonFsh`, one helper for all three programs).

**Cost** — Crosscall HLTE556N (Adreno 610), massif at z13.6 / tilt 55, contours over the whole
frame, `--es terrainLight true --es ambient 0.35`, GPU timer, two paired runs per configuration:

| | GPU `layers` | GPU total | fps |
|---|---|---|---|
| shadows on, before | 4.85 ms | 32.4–36.6 ms | 15.1–15.7 |
| shadows on, after | 4.83 ms | 32.2–35.0 ms | 15.3–16.1 |
| shadows off, before | 1.30 ms | 18.8 ms | 20.8–22.0 |
| shadows off, after | 2.51 ms | 20.1 ms | 20.3–20.6 |

With shadows on it is free: the stencil was already being paid for the shadow's slope bias. With
shadows off the lighting is what pays for it, +1.2 ms of `layers` (+6% of the frame) — the price of
a contour that shades like the ground under it. Cheaper normals were not tried: a different normal
than the surface's would make the two disagree exactly where the eye compares them.

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
- **Caster margin, bounded by the shadow THROW.** Casters are the cover plus a ring, because a
  mountain off screen still throws its shadow into the view. The ring's reach is
  `relief / tan(sun altitude)` — capped at about 3.7 x the relief by the 15-degree floor — and NOT a
  tile count: a ring counted in tiles is a distance that shrinks with the zoom, so at z16 (tiles
  ~430 m) the default 3 reached 1.3 km and a mountain 5 km away had no caster at all. Symptom: a
  mountain shadow missing at z16 that appears as soon as you zoom out or pan enough to pull the
  mountain into the cover. Holding the distance means dropping the resolution — 7 km at z16 would be
  a 35x35 ring — so the ring is generated at the **coarsest zoom that spans the throw in
  `shadowCasterMargin` tiles**, and `shadowCasterMargin` now sets the ring's resolution rather than
  its reach.

  **The relief is read from a coarse ancestor (`SHADOW_RELIEF_ZOOM = 10`), not from the cover.** This
  is the part that is easy to get wrong and did not work at first: at z16 top-down over a valley the
  cover is a few tiles of flat ground, so the cover's own relief is metres, the throw is a couple of
  hundred metres, and the ring collapses straight back onto the cover's zoom. The mountain casting
  into that view is *outside* the cover, so its height is never in that range. One elevation query on
  an ancestor spanning the massif fixes it. Measured on the Crosscall at lat 45.193196 lon 5.735717
  z16.04 tilt 90: 21 caster tiles per pass, 1.0 ms per pass, 23.5 fps / 3.5 ms drape — *faster* than
  the cover-relief version (20.5 fps / 4.8 ms, 22 tiles) because the same number of tiles now covers
  the throw coarsely instead of covering the valley floor finely. Against no ring at all: 25.7 fps /
  2.3 ms. The original fine ring drew up to 49 tiles and still missed the mountain.
- **The height slab must span the CASTERS, not the cover.** vt has no per-tile heights for the ring
  — it measures every ring tile at the one range it is handed — so a ridge taller than that range is
  clipped out of the caster pass by the light box's near plane, and its shadow arrives truncated
  along an edge that moves with the camera, because the range follows the cover. Measured at
  Grenoble z16.53 tilt 90: the cover's range was `5.75..17.43` while the ring tiles reached `145.13`,
  eight times taller. `MapRenderer` therefore widens the range with the caster tiles' own min/max,
  which is exact rather than a guess from a coarse ancestor. The per-tile ranges still narrow each
  cascade's *receiver* slab, so the texel size does not pay for it.

  Worth recording how this was nearly missed: a single-frame screenshot A/B said the fix changed
  19,425 px of 1,357,952 — noise — and it was reverted as refuted. The symptom only shows while
  panning, so a static frame did not measure it. Two earlier candidates were ruled out the same way
  and those refutations stand (0 casters skipped for missing elevation; the ring's reach needs 1.76
  tiles of the 3 it has, and `shadowMargin` 3 vs 8 is noise), but a still frame is not a valid
  detector for anything that only appears in motion.

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

### A compile failure this uncovered, and its fix

Adding the version prelude shifted shader line numbers, which surfaced a **pre-existing** failure:
`tilecolormap` built with `TERRAIN_LIGHT` + `TERRAIN` never compiled, in either stage. The fragment
stage calls `terrainNormal()` and reads `uSunDir` / `uSunColor` / `uLightParams`; the vertex stage
never declared or wrote `vElevUV` / `vElevCosh`. Both sets lived only in `backgroundFsh` /
`backgroundVsh` — a different pair of strings — so the program failed on the first symbol the
compiler reached. Caught by `TileRenderer::prepareFrame` and only logged, it was invisible unless
grepped: **6 occurrences per run**, on every committed build since the flag was passed.

So the feature behind it was dead. A raster drawn **outside** the drape at an integer zoom out
(`renderTileBitmap`) never took the sun or the shadow of the surface under it — the flash that path
exists to remove, see [04-terrain.md](04-terrain.md#the-outgoing-generation-at-an-integer-zoom-out).

Fixed by lifting both blocks into `terrainLightVsh` / `terrainLightFsh`, prepended to `background*`
and `colormap*` alike. `commonVsh` / `commonFsh` cannot host them: `terrainPaintVsh`,
`terrainPaintFsh` and `terrainPaintPrelude` declare the same names for themselves and also get
`TERRAIN_LIGHT`, so a shared declaration would collide there.

Verified offline, not on a device. A throwaway harness replicates `buildShaderProgram`'s
concatenation and `createShaderProgram`'s prelude, emits the variants that reach these strings (lit,
shadowed, mask in/out, hardware PCF, plus terrain-paint / normal-map / line / polygon as a
regression check) and runs `glslangValidator` over each: 0 errors after, and the reported error
reproduced exactly on the unfixed header. Note glslang does **not** flag a fragment varying the
vertex stage never declares, so which of "link error" or "reads undefined values" a real driver does
here is untested — either way the vertex writes are what make the lighting correct.

Compile errors now quote the source around the reported line and list the program's defines — the
line number is into the concatenated source, which nothing on disk matches, and without the quote
every shader error costs a round of guessing.

### The light box is a bounding sphere, so tilt does not change the texel

Each cascade's box is the **minimum bounding sphere of its slice of the view frustum** — mapbox's
`createLightMatrix` verbatim, whose own comment says why: *"rotation invariant shadow volume"*.
The radius is a function of the slice distances and the field of view **alone**:

```
k2 = tan(fovX/2)^2 + tan(fovY/2)^2       // tangent to a frustum corner, read off the projection matrix
radius = 0.5 * sqrt((far-near)^2 + 2*(far^2+near^2)*k2 + (far+near)^2*k2^2)
```

No pitch, no bearing, no sun azimuth, no ground. A sphere projects to the same square from every
direction, so **one texel is the same size at every camera** — the entire reason mapbox's shadows
look identical at a low tilt and straight down.

**What this replaced, and why it was wrong.** The box used to be fitted to the *visible-ground
polygon*: a wedge sampled from the frustum edges, clipped to the drawn tiles, hulled, then bounded
in light space. Two things made it tilt-dependent — the wedge stretches towards the horizon as the
view flattens, and its extent *in light space* also swings with the sun azimuth. The texel size
followed both, so building shadows went from sharp at tilt 90 to a grey smear by tilt 45. Measured
at the reported camera (Crosscall, lat 45.188499 lon 5.734500, z16 tilt 45 rotation -15.12,
`bld3d`, sun 16.5 UTC): the old fit gives shadows with no locatable edge, the sphere fit gives hard
edges on individual buildings. About 200 lines of wedge machinery went with it — ray sampling,
annulus sectors, `convexHull2D`, `clipPolygonToRect` — and the empty-footprint fallbacks that
existed only because a wedge can miss the tiles.

This is the case the working agreement warns about: mapbox had a sphere, this branch invented a
polygon fit, and every symptom above followed from that one choice.

**The camera-to-focus distance is passed in, not read from `_viewState`.** That field is filled by
`TileRenderer::onDrawFrame`, which runs *after* the shadow pass, so the fit would read the previous
frame's value or none at all — the first version of this change failed to fit any box for exactly
that reason.

**Resolution is a second-order knob, not the fix.** `shadowMapSize 2048` + `shadowCascades 2`
(mapbox's configuration) is slightly sharper again and nearly free — 13.6 fps / 5.8 ms drape against
14.1 / 5.8 for 1024 x 3 at z16 tilt 45, i.e. drift — because the per-cascade cost is matrices and
varyings rather than sampling. Not the default only because of memory: ~33 MB of atlas at D24
against ~12.6 MB. mapbox pays ~16 MB by using D16, which is worth trying and **untested**.

**Dead end: the screen-space mask is not the limiter.** `SHADOW_MASK_DIVISOR = 1` (full resolution)
was tried on the theory that a quarter-resolution mask was blurring building shadow edges. It made
them look *worse*: the full-res mask exposed the shadow-map staircase the blur had been hiding. The
mask hides quantisation, it does not cause it. Leave it at 4.

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

`TileRenderer::LIGHTING_SHADER_3D` lights extrusions through `applyLighting3D`, which is the one
entry point for anything solid drawn in the 3D pass — extrusions today, source-driven 3D models next
(#131) — so a model and the wall beside it cannot disagree about the sun.

It is **mapbox's `fill-extrusion` model**, ported from `_prelude_lighting.glsl` and
`fill_extrusion.{vertex,fragment}.glsl` (semantics only — mapbox-gl-js v3 is under their TOS, not
BSD, so nothing is copied):

```glsl
ambient = ambientColor * ambientIntensity * verticalFactor * ambientDirectional
sun     = sunColor * sunIntensity * max(0.0, dot(N, sunDir)) * shadow
lit     = pow(ambient + sun, 1.0 / 2.2)          // summed LINEAR, returned to sRGB once
```

Four things matter here, and each was a separate round to find:

- **The sum is linear, the output sRGB.** `linearProduct(c, k) = c * pow(k, 1/2.2)`, which is
  `linearTosRGB(sRGBToLinear(c) * k)` with one `pow` instead of three. A straight sRGB multiply
  crushes the midtones and is why facades read harsh here long after the model was otherwise right.
- **Ambient is direction-aware.** `verticalFactor = mix(0.92, 1, N.z*0.5+0.5)` (light blocked from
  below), and `ambientDirectional` takes up to 30% off faces pointing away from the sun, scaled by
  the sun's own luminance. That variation across the ambient is what separates wall tones — mapbox
  has **no facade gradient at all** in this path (`u_vertical_gradient` is read only in their legacy
  branch, and even there it clamps flat below ~106 m). `buildingVerticalGradient` defaults to 0.
- **Ambient and sun simply add**, with no headroom coupling. Both default to mapbox's **0.5**, so
  they sum to exactly 1 in direct sun: a facade the light reaches keeps its own colour at any hour.
- **The shadow scales the sun only** — see the back-face trap below.

Roofs and walls both take `N.L`, and `resolveLighting` sets `buildingLightIntensity` from the sun
**unconditionally** — `terrainLightingEnabled` decides whether the *ground* is lit and nothing else.

This model reproduces mapbox's day/dusk inversion for free: a roof's `N.L` is `sin(altitude)` and a
sunward wall's is `cos(altitude)`, so **roofs read lighter than walls in daylight and darker at
dawn**. Measured on one roof pixel at the Grenoble camera: sun 45° → `(168,94,27)`, sun 8° →
`(140,76,21)`, with the sunward wall overtaking it at the low sun.

A **60° floor on the sun's altitude for buildings** was added in `e830b54ee` and removed again: it
existed because facades went black at a low sun, and that blackening was the back-face bug below,
not the sun angle. While it stood, every roof was lit as if the sun were at 60° and the inversion
above could never happen. Note mapbox's directional light defaults to elevation **30°**
(`direction: [210, 30]`) and it is an ordinary style property, not a floor.

The **ambient does not follow the ground's** either, and that asymmetry is deliberate. Ambient is the floor
the directional term is added on top of, so at `ambientIntensity` 1 the model collapses to a
constant and every facade goes flat. Flattening the ground that way is a normal thing for an app to
do when a hillshade layer supplies the relief — the demo ships `AMBIENT_INTENSITY = 1.0` for exactly
that reason — and it should not cost every building its side shading, without which an extrusion does
not read as 3D at all. mapbox's `fill-extrusion` shades from its own light intensity rather than the
scene ambient for the same reason. A style ties them back together with `building-ambient`.

This was found the hard way: with the ambient coupled, `--es terrainLight false` produced completely
flat buildings, and `terrainLight true` hid it — the shadow pass's back-face rule was darkening
away-facing walls and doing the job the sun should have been doing.

### The back-face trap: a shadow must not take the ambient with it

`shadowFactorSlope` ends with `lit = min(lit, facing)`, `facing = smoothstep(0, 0.15, N.L)` — a
surface turned away from the sun is in its own shadow whatever the depth map says, which is right and
is what lets the bias stay small everywhere else. The bug was **where the result was applied**:
`polygon3DFsh` multiplied the finished colour by it, ambient included, so every wall not in direct
sun collapsed to near-black rather than dropping to sky-only. mapbox instead passes the shadow in as
the *directional* factor and never touches ambient.

The term now goes into `applyLighting3D`, which requires the extrusion lighting to run **per
fragment** (`LightingShader(perVertex=false)`) — the shadow exists nowhere else. Unmeasured cost: one
dot, one mix and one `pow(vec3)` per building fragment.

Worth recording how long this hid: three separate rounds blamed the lighting model, the ambient
value and the sun altitude in turn. What settled it was bypassing stages rather than reasoning —
`applyLighting3D` returning its input unchanged still gave black walls, which ruled the lighting out
in one build; removing the shadow multiply fixed them in the next. A histogram of the frame was
actively misleading here, because the dominant dark tone was the *ground*, not the walls.

Until 2026-08 there were two models here instead, switched on `buildingLightIntensity > 0`: with the
terrain sun off, walls used `N.L * mainLightColor + ambientColor` from the pre-`LightOptions`
`Options` properties and **roofs used the VIEW direction**, so from above — where roofs are most of
what you see — the buildings did not answer to the sun at all, and they visibly changed shape when
terrain lighting was toggled. The colour was dropped from the surviving branch as well, on the
grounds that `u_lightColor` (the legacy grey `143,143,143`) darkened the walls below the ground lit
by the same formula. That was the wrong uniform to reach for: `sunColor` is the one the terrain uses,
and without it a warm evening sun warmed the slope while the facade in front of it stayed grey.

`Options.MainLightDirection` / `MainLightColor` / `AmbientLightColor` no longer reach the tile
extrusions at all — see [migration.md](../../migration.md). `Polygon3DRenderer` (app-supplied
`Polygon3D` vector elements, not tile extrusions) still carries a third, unrelated lighting model.

It is installed **per vertex**
(`LightingShader(true, ...)`). That has a consequence worth knowing before touching it: any function
of height in there only reaches the screen through the values at the base ring and at the roof - the
wall carries the linear interpolation between them, whatever curve the formula draws. A falloff
"over the first metre" is therefore a full-height ramp on screen, and the only thing that changes the
look is the endpoint value.

The ambient term at the foot of a wall is the cue that makes an extrusion stand on the terrain rather
than float over it: that corner is occluded by the ground and by the building's own footprint
whatever the sun does, and the shadow map cannot resolve it - its texels are metres wide. Measured
luminance down a wall on the device: 206 at the roof, 100 at the foot (124 with the previous 0.5,
which read as too light).

It is `mix(1 - buildingVerticalGradient, 1, wallT)`, and **`wallT` is baked into the vertex by the
tesselator**, not computed in the shader: `clamp(h / building-vertical-gradient-height, 0, 1)` where
`h` is the vertex's **absolute height above the ground** (`TileLayerBuilder::appendWallQuad`, packed
0..127 in `_attribs[3]`). The style sets `building-vertical-gradient` (default `0.65`, the foot at
35% of the wall colour) and `building-vertical-gradient-height` (default `20` m).

Two things follow from where it is evaluated, and both were learned the hard way:

- **Absolute, not a 0..1 parameter of each wall.** OSM models one building as a parent plus
  `building:part` polygons of different heights. A per-wall parameter makes every part ramp over
  *its own* height, so a 10 m part is fully bright exactly where the 30 m part beside it is still
  mid-grey: the building reads as a stack of blocks with a band at every junction.
- **In the tesselator, not the shader.** There, the height and the reach are both style values in
  one unit. The shader's `height` was not: `aVertexHeight * uAbsHeightScale` worked out to
  `metres × 262144` — `POLYGON3D_HEIGHT_SCALE` is circumference/4 and `tileHeightScale` is
  `WORLD_SIZE / 2^zoom`, and the tile-local factors cancel to exactly `WORLD_SIZE / 4`. Comparing
  that against a reach in metres saturated at 7.6 × 10⁻⁵ m, so every vertex above the base ring
  evaluated to 1 and each wall quad ramped over itself no matter what the reach said. On device that
  looked like *one gradient per floor*, identical at 10, 20 and 40 m. `uAbsHeightScale` and
  `POLYGON3D_HEIGHT_SCALE` are gone with it — nothing else used them.

The reach is therefore **decode-time geometry**, not a uniform: changing it re-decodes the tiles.
The strength stays a live uniform.

### Two dead ends, both shipped before being caught

Worth knowing, because both look correct in isolation:

- `1 - 0.65/(1 + h*h)`, the original. Given the per-vertex rule above it produced almost the right
  screen ramp for an ordinary building — 0.35 at the foot, ~1.0 at the roof — so it is *not* the
  "tall facades are flat" bug [#132] describes; a tall facade was never flat. It broke a wall whose
  base is not at zero: a part starting 20 m up evaluated `1 - 0.65/401 ≈ 0.998` at its own foot and
  got no gradient at all. That silent failure is also what made the stacked-parts case *look* right,
  since a gradient-less upper part blends with the bright top of the part below it.
- `mix(1 - g, 1, t)` with `t` the wall's own 0..1 parameter, which fixes the raised part in isolation
  and is what replaced the above. On real Grenoble data it is visibly worse: it gives every part a
  full ramp, so the junctions that used to blend now band hard. Verified on device.

### The reach needs a vertex, not just a uniform

The lighting is per vertex, and a wall has vertices only at its base ring and its roof — so any curve
in there reaches the screen as a straight line between those two values. A reach of 20 m on a 40 m
wall does not stop at 20 m; it stretches the whole facade. The reach only bit on walls *shorter* than
itself, which is a knob that silently does nothing on exactly the buildings you set it for.

`TileLayerBuilder::setPolygon3DGradientHeight` fixes that in geometry: the tesselator inserts an
extra ring where the gradient knees, so the per-vertex line has a joint in the right place, and the
ramp above it is flat. `TileReader` reads the value from the `Map` block with the **tile's own**
zoom — this decides geometry, so it has to be fixed when the tile is built, and a zoom-dependent
reach is sampled once per tile.

Cost is one extra quad per wall taller than the reach (2 triangles → 4), nothing for shorter ones.
That was chosen over moving the lighting per fragment, which is the general fix and is what A1 needs
anyway for a metric AO falloff, a roof-edge term and emissive — none of which a vertex split
approximates. Per-fragment lands as its own measured PR rather than smuggled in under a gradient fix.


### Rounded roof edges

`building-edge-radius` (metres, **0 = off**, mapbox's default too). The wall stops at
`maxHeight - radius`, the roof ring is inset by the same amount, and **one quad per footprint edge**
bridges the two — against [#132]'s projected +30% roof vertices, there are no extra roof vertices
beyond the inset ring.

The rounding is in the **normals**, not in subdivision. `_attribs[1]` went from a 0/1 `sideVertex`
flag to a 0..127 blend, and the vertex stage does `normalize(mix(aVertexNormal, aVertexBinormal,
side))`: the band's lower edge carries the wall's normal, its upper edge the roof's, so
wall → bevel → roof shades continuously. Reusing that byte means no vertex-size increase, and the
same blend now weights the facade gradient so it fades out as a surface turns to face up.

**0.8 m matches mapbox** on Grenoble data; 2 m already reads as too soft.

The bevel follows the wall dedupe — an edge whose wall was suppressed as a duplicate gets no bevel,
or it would round an edge it did not draw. It is skipped entirely for a building shorter than
`2 × radius`.

Insetting is where this goes wrong, twice over, and both are worth knowing:

- **Clamp per vertex, not per footprint.** A global clamp to the narrowest edge let one 1 m jog cost
  the whole building its bevel, so most roofs stayed sharp while a few clean rectangles got the full
  radius — which reads as the feature not working rather than as a clamp.
- **Clamp the DISPLACEMENT, not the radius.** The miter runs to 5× at a sharp corner, so a radius
  already clamped to half an edge still moved the vertex two and a half edges — across the footprint.
  The ring self-intersected and the tesselator answered with inverted triangles: black wedges along
  the roof edge, worst where corners are sharpest.

Degenerate edges are skipped rather than rejected: an MVT ring that repeats its first point at the
end has one, and bailing on it left **every** building sharp.

### Contact shadow on the ground

The other half of standing on the terrain, and the reason buildings otherwise read as pasted on.
`building-ao-ground-radius` (metres, **0 = off**, so nothing changes until a style asks),
`building-ao-intensity` and `building-ao-ground-attenuation`, on mapbox's names.

`TileLayerBuilder::appendGroundSkirt` emits a flat skirt around each footprint at the building's
**base** height — a quad per edge reaching `radius` outward with the distance packed 0..127 in
`_attribs[3]`, plus a triangle filling the wedge that offsetting leaves at every convex corner
(without it a rectangular building has four undarkened notches; reflex corners need none, the quads
overlap there instead). Which way is *out* comes from the ring's signed area, so an exterior ring
and a courtyard hole each get the skirt on the side with no building.

It is its own `TileGeometry::Type::POLYGON3DGROUND` with its own accumulation arrays, because the
builder has ONE vertex stream keyed by `_builderParameters.type` and this needs its own program and
blend state. `renderGeometry3D` draws it **before** the extrusions, multiplied into the ground
(`GL_ZERO, GL_SRC_COLOR`), depth read but **not written** — it lies on the surface and must not
become an occluder for the walls standing on it.

Chosen over baking it into the drape, which is what [#132] originally proposed. The drape cannot see
extrusions at all (`isDrapeableGeometry` excludes `POLYGON3D`), so it is not free at draw time; a
drape texture is 0.6–2.4 ground metres per texel at z15–17, making a 3 m radius one to three texels;
and the drape is a terrain feature, so a plain 2D map would get no contact shadow. mapbox uses a
scene pass (`groundEffect`) for the same reasons.

The falloff is **smoothstepped**, not just `pow()`. `pow(d, 0.69)` reaches 1 with a slope of 0.69,
and the eye reads that derivative step as a crease where the skirt ends — a visible outline around
every building, which is the opposite of what a contact shadow is for. Smoothstep lands at both ends
with zero slope, so the band fades into the ground and is flattest against the wall, where the
occlusion really is most complete.

The skirt is **one offset ring**: every vertex moves outward by its own miter (capped at 3× radius),
and quads span consecutive pairs. The first version offset each EDGE by its own normal and filled
the corner gap with a triangle — but that triangle interpolates its darkening from a single wall
vertex while the quads beside it interpolate from an edge, so the shading disagreed along both
shared edges and produced one radial streak per footprint vertex. Per-vertex offsetting has no
corners to fill and is fewer triangles.

It also **scales by the tile's fade**, which arrives as the style colour's alpha. An extrusion fades
in by growing, so without this its contact shadow appeared at full strength on a building that was
not there yet: a footprint painted on the ground as a tile arrived.

**Its own packing scales have to be measured, not assumed.** `packGroundSkirt` calls the explicit
`packGeometry` overload, and the scales it passes are what the vertices are quantised to on the way
into int16. Passing `1.0f` snapped every skirt vertex onto integer tile coordinates — one triangle
per block instead of a contact shadow, multiplied into the ground as a black wedge. It needs the **index split** too: the index
buffer is `UNSIGNED_SHORT`, and a dense tile runs past 65535 skirt vertices, wrapping the indices so
triangles stitch unrelated vertices into slivers hundreds of metres long.

**The radius is in metres and the tesselator works in tile-local units.** Converting is
`transformer->calculateHeight(p, radius)` — `2^zoom / circumference / cos(latitude)`, which is what
a metre is worth horizontally as well as vertically. Skipping it made a 3 m skirt reach three tile
widths, about 1.8 km at z16, and multiplied the entire map to black. That is the second time a
style value in metres has been compared against something in another unit here (see the facade
gradient above); when a constant crosses into vt, check what it will be measured against.

Screen-space AO over the scene depth was not tried: the whole 3D pass is ~9 ms on an Adreno 610.
It is the only option that would also darken building-against-building, which this does not.

### Coincident walls

A stipple on tall walls that looks exactly like shadow acne is usually **not** acne. OSM models a
building whose height varies as a parent `building` plus several `building:part` polygons, and
duplicate footprints from two source layers do the same thing: two extrusions end up with walls on
the same footprint edge, a few centimetres apart, and they z-fight. The extrusion depth bias is
uniform across the pass (`TERRAIN_EXTRUSION_DEPTH_DELTAS`), so nothing separates building from
building.

**Telling them apart costs one broadcast**: `--es shadow 0`. Acne cannot survive shadows being off;
z-fighting can. Second discriminator — acne tracks the sun (`--es sunHour`), z-fighting tracks the
camera and is unchanged by the hour.

`TileLayerBuilder` therefore keeps `_polygon3DWalls`, a map from footprint edge to the height range
already walled on it for this layer and tile. A wall is emitted only over the range no earlier
feature covered — as up to two quads, since a wall taller at both ends sticks out below *and* above
what is there. The edge key quantises both endpoints to 1/32768 of a tile (7 cm at z14, finer than
any tiler's grid) and is undirected, because two features walk a shared edge in opposite directions.

This is ours, not a port. **mapbox-gl-js does not do it**: `fill_extrusion_bucket` tesselates every
polygon independently, and its `buildingGroups` — keyed on a `building_id` property carried in the
tile — exists only to give every part of one building an identical centroid for the ground AO and
flood light. Their tiles ship parts already resolved against their parent, so coincident faces never
reach the renderer. Tangram has no handling either (`Builders::buildPolygonExtrusion`, one extrusion
per polygon), so it shows the same artifact on the same data.

Known gaps:

- **A roof is matched whole, not clipped.** `_polygon3DRoofs` keys the footprint plus the height it
  sits at, summed over the vertices so the ring's start vertex and winding do not matter — which
  catches a **duplicated footprint**, the case that actually z-fights. A `building:part` normally
  differs in height from its parent, putting its roof on another plane entirely; a part that shares
  its parent's height *and* only overlaps it partly would still fight, and would need real polygon
  overlap to fix.
- The covered range per edge is a single interval, so a wall split into disjoint pieces by two
  earlier walls at different heights is approximated by their hull.
- Where a shared wall survives, it keeps the colour of whichever feature reached the builder first.
  Deterministic per tile, arbitrary between the two.
- Fixing the tiles upstream is still the better answer, and the one mapbox ships.

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
