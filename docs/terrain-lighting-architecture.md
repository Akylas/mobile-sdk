# Sky, sun, shadows and peak finder: can this architecture carry them?

Short answer: **yes, and the RTT drape is what makes it true.** Three of the four features are
cheap once all 2D map content lives in a per-tile texture and the terrain mesh is the only thing
in the depth buffer. On the pre-drape architecture — 2D content displaced into 3D and separated
from the terrain surface by depth biases — each of them would have been a per-shader retrofit
with an artifact budget of its own.

This document records what was compared, what the target architecture is, and the concrete design
for each feature.

## 1. What the two reference engines actually do

Both clones are read-only references in `/Volumes/dev/carto`.

### tangram-ng (`core/src/util/elevationManager.{h,cpp}`, `res/scenes/terrain-3d.yaml`)

Terrain is a **style mixin, not a renderer feature**. `terrain-3d` adds a `position` block to every
style that opts in:

```
float elev3d = max(getElevation(), TANGRAM_MIN_ELEVATION);
position.z += TANGRAM_TERRAIN_SCALE * elev3d;
```

Every vector style therefore displaces its own vertices by sampling the shared elevation raster
(`TANGRAM_VERTEX_RASTERS`). There is no draping at all. Consequences:

- All content is native-resolution and crisp. Nothing is resampled.
- Content and ground agree only where they share vertices, so the scene needs a depth fudge —
  `depth_shift = -0.02*u_proj[2][3]`, plus `proxy *= 48.0` for raster styles. That is the same
  class of constant we spent rounds 45–56 tuning, and it has the same failure mode (it is a
  distance²/near-plane tolerance in eye space).
- `ElevationManager::renderTerrainDepth` renders a **terrain-only depth pass** into its own
  framebuffer, and `getDepth(screenpos)` reads it back. That is exactly the primitive a peak
  finder needs, and CARTO already has the equivalent (`PostProcessEffect` +
  `uTerrainDepthTex`).
- `skyManager.{h,cpp}` is a small full-screen sky pass — the same shape as CARTO's
  `SkyRenderer`.
- **No shadows anywhere.** Nothing in tangram-ng casts or receives.

So tangram-ng is a good reference for elevation sampling and for the terrain-depth pass, and a
bad reference for the depth model: it is the model this fork already ported (round 52) and then
kept having to patch.

### maplibre-gl-js (`src/render/terrain.ts`, `src/webgl/render_to_texture.ts`)

Render-to-texture draping, and it is the model worth copying:

- `LAYERS_TO_TEXTURES` = `background, fill, line, raster, hillshade, color-relief`. Symbols,
  circles, heatmaps and `fill-extrusion` render live in 3D. Nothing else is negotiable — a layer
  is either flattened into the terrain skin or it is real 3D.
- The drape render uses an **orthographic tile-local matrix** with terrain uniforms suppressed:
  the FBO holds a flat 2D render of one tile square.
- **Stacks**: a run of contiguous drapeable layers becomes one texture. A non-drapeable layer in
  the middle starts a new stack — another texture per tile and another terrain mesh draw.
- The terrain mesh is a **shared static grid** reused by every tile (plus skirts), so per-tile CPU
  tesselation is zero.
- Drape textures are cached on the tile and invalidated by a **fingerprint** of the contributing
  source tile keys plus the source revision.
- The terrain mesh draws `LEQUAL` with depth read+write and shares its depth range with 3D
  extrusions. Everything draped is `DepthMode.disabled`.
- Symbols lift themselves by elevation and fade against a **packed RGBA screen-space depth
  texture** rendered from the terrain mesh.

**maplibre-native has no 3D terrain, no sky and no shadows** — it is not a reference for any of
this. gl-js is the only one.

## 2. Where CARTO stands now

The fork already had the seed of the maplibre model (`GLTileRenderer::renderDrapeTextures`,
`renderTileSurfaceDrape`, `TerrainDrapeCache`, `setTerrainRegularGrid`). What it did not have was
a drape that actually reached the screen. Three defects, each hiding the next:

1. `MapRenderer` collected drape layers with a `dynamic_cast` over the **top-level** layer list.
   A `CompositeVectorTileLayer` therefore contributed only its own group-0 renderer; its
   hillshade slot, raster slots and later style-layer groups were neither baked nor suppressed,
   and kept their own terrain pre-pass and depth domain. Layers now report their drapeable tile
   layers through `Layer::collectDrapeLayers`, which the composite overrides.
2. The shared surface draw used the default `GL_LESS`, while the global terrain background had
   already written the depth of the **same meshes**. Every surface fragment was rejected, so the
   terrain showed the background colour and nothing else. It draws `GL_LEQUAL` now.
3. The bake inherited back-face culling from the previous pass. The bake matrix maps tile-local
   xy straight to clip space with no y flip, while the on-screen matrix goes through a projection
   that does — so every triangle baked with the opposite winding and the fills were culled. The
   bake now owns its complete GL state.

With those fixed, the composite demo renders identically to the non-draped reference at every
camera tested, and the "landcover holes" cannot occur by construction: draped content never
enters the depth buffer.

### What the drape buys, restated as invariants

- The depth buffer contains **terrain surfaces and true 3D content only**.
- 2D content has **no depth interaction whatsoever** — no bias, no slack, no painter-order
  ordinal, no per-layer depth domain, no tile-layer `glClear(GL_DEPTH_BUFFER_BIT)`.
- The terrain surface is a **single shared regular grid**, so it is watertight and its
  parametrization is exactly the drape uv.
- Terrain-driven geometry subdivision at decode time is unnecessary for anything draped.

Those four are the preconditions for everything below.

## 3. Feature by feature

### 3.1 Sky with a sun, driven by the hour — **done, small**

`SkyOptions` + `SkyRenderer` render one full-screen pass. The per-pixel world ray comes from
`inverse(getRTEModelviewProjectionMat())`: RTE space puts the camera at the origin, so
unprojecting the near plane *is* the ray direction. The built-in shader is a horizon gradient plus
a sun disc, glow and halo.

`LightOptions::setSunPositionFromTime(year, month, day, hourUTC, minute, lat, lon)` computes
azimuth/altitude with the NOAA low-accuracy solar algorithm (~0.1°) and stores them, so the sun,
the terrain lighting and the shadows all read one direction.

Gotchas already paid for, which any future shader work must respect:

- `Shader::getUniformLoc` returns **0** for a uniform the compiler removed, so uploading an unused
  optional uniform overwrites location 0. Use `glGetUniformLocation` and skip −1.
- Sun glow must **tint** toward the sun colour, not add; additive glow saturates a bright sky to
  white far from the sun.
- Angular distance to the sun: `length(rayDir - u_sunDir)` (chord ≈ radians), not `acos`/`pow`.

### 3.2 Custom sky shader (clouds) — **done**

`SkyOptions::setShaderSource` swaps in user GLSL that defines `vec4 skyColor(vec3 rayDir)`. Clouds
are a function of `rayDir` plus `uTime`, so they need no further engine support. The same
uniform-location rule applies to any optional uniform the user's shader may or may not reference.

### 3.3 Dynamic lighting / hillshade on the terrain — **shipped**

This is the feature the drape unlocks outright. The draped surface is now the *only* lit ground
surface in the scene, so lighting is one multiply in one fragment shader:

```
color = texture2D(uDrapeTexture, vDrapeUV);       // all 2D map content
normal = normalFromElevationTexture(worldPos);    // central difference on the DEM
lit    = ambient + sunIntensity * max(0, dot(normal, sunDir)) * sunColor;
gl_FragColor = vec4(color.rgb * lit, color.a);
```

The elevation texture, its decode vector and its world mapping are already bound for the surface
draw (`GLTileRenderer::TerrainTexture`, `setupTerrainUniforms`), so the normal costs four extra
texture fetches and no new plumbing.

Consequences worth stating:

- It **replaces the pre-baked hillshade raster layer** for the common case: no second DEM decode,
  no `NormalMapBuilder`, no separate layer, no separate depth domain, and the light follows the
  hour instead of a fixed illumination direction.
- The pre-baked layer stays useful where the look is deliberately non-physical (IGOR, custom
  `applyLighting` shaders, contour-from-normal-map tricks), so it is not removed.
- In the old architecture this same feature would have had to be added to *every* 2D style shader,
  each at a slightly different depth than the surface it is meant to be lying on.

### 3.4 Dynamic shadows on terrain and buildings — **shipped**

Requirements: terrain shadows itself, buildings cast on terrain and on each other, buildings
receive. That is a classic directional shadow map, and the drape architecture makes both ends of
it clean:

**Caster pass.** One depth-only render from the sun, orthographic, fitted to the visible terrain
bounds. Casters are exactly the things already in the depth buffer:
- the terrain grid surfaces, displaced by the same vertex-shader elevation fetch (so the shadow
  geometry is bit-identical to the rendered geometry — no self-shadow acne from a mismatched
  proxy mesh);
- `POLYGON3D` extrusions;
- optionally 3D vector elements / NML models.

**Receiver side.** Two shaders, not twenty:
- the draped terrain surface shader (§3.3) multiplies its `lit` term by the shadow test;
- the `polygon3d` shader does the same.

Everything else in the scene is either inside the drape texture (so it is shadowed by the surface
it is painted on, which is correct) or a screen-space label (which should not be shadowed).

What shipped: `TerrainShadowMap` (packed-depth colour texture plus a depth renderbuffer),
`GLTileRenderer::calculateShadowViewProj` / `renderShadowCasters`, and
`LightOptions.setShadowStrength` / `setShadowMapSize` / `setShadowBias`. Casters are the terrain
grid surfaces and `POLYGON3D` extrusions; receivers are the draped terrain surface and the
extrusions. Verified on the emulator: a 10-degree western sun casts clean ridge shadows into the
valleys east of them, and extruded buildings cast onto the streets and shade each other.

The bias that mattered turned out to be the **slope-scaled polygon offset on the caster**, not a
constant in the comparison: one shadow texel covers tens of metres of ground, so on a slope lit at
a grazing angle the depth varies across a single texel by far more than any constant bias can
absorb, and the surface shadows itself in a regular hatch. A constant bias big enough for that
detaches the shadows from the ridges casting them.

Design notes:
- The light frustum is currently fitted to the **terrain tiles being drawn**. Fitting it to the
  camera frustum ∩ terrain bounds instead would raise the effective resolution; one cascade is
  enough at map tilts, two if close-range building shadows must stay sharp.
- Depth precision: reuse the packed-RGBA depth encoding already used for `uTerrainDepthTex`
  where a real depth texture is unavailable (GLES2 without `OES_depth_texture`).
- Bias: normal-offset bias, not constant depth bias — the terrain has huge slope variation and a
  constant bias reproduces the whole round-45..56 problem in light space.
- Cost: one extra pass over the terrain grid (a handful of draw calls, no per-tile tesselation
  since the grid is shared) plus the extrusions. Re-render only when the sun or the view changes.

**Why this was not sane before the drape:** every 2D style shader would have needed the shadow
lookup, and content sitting at a slightly different depth than the surface it visually lies on is
shadow-acne by construction.

### 3.5 Peak finder view — **shipped**

Wanted: a view showing terrain shape and peaks only, with a *relaxed* occlusion rule so that
slightly hidden peaks still show.

The pieces that exist:
- `PostProcessEffect` renders the map into an offscreen buffer and hands a fragment shader
  `uColorTex`, `uInvScreenSize`, `uNear`, `uFar`, `uTime` and custom float parameters.
- With `setTerrainDepthRequired(true)` it also provides **`uTerrainDepthTex`**: a packed 24-bit
  linear terrain depth plus a coverage alpha. That is tangram's `renderTerrainDepth` equivalent,
  and it is enough to draw silhouettes, ridge lines, distance shading and horizon extraction in
  one shader.
- Terrain labels already have an occlusion test hook (`setLabelOcclusionTest`) that is evaluated
  **per anchor**, not per pixel.

What was missing was a **tolerance** on that occlusion test — it had a hard-coded 2% slack, just
enough to absorb the mismatch between a label anchor and the terrain it sits on.
`TerrainOptions.setBillboardOcclusionTolerance` now exposes it: raising it deliberately lets partly
hidden features label, so a summit just behind a nearer ridge still shows its name. Both occlusion
paths (the depth-buffer read-back and the ray fallback) use the same value.

Verified: `PostProcessEffect.CreateReliefOutlineEffect()` over the Chartreuse at low tilt renders
clean ridge silhouettes. One trap: attaching a post-process effect *before* the GL surface exists
leaves the offscreen colour buffer unwritten and the screen goes black — attach it after the first
frame.

## 4. Recommended target architecture

1. **RTT draping is the terrain path.** Not an option among others. Backgrounds, fills, lines and
   rasters bake; points, 3D extrusions, labels and vector elements stay live.
2. **Stacks**, maplibre-style, once a non-drapeable layer sits between drapeable ones. Single-stack
   is implemented and is the usual configuration.
3. **Retire the depth machinery** that only existed to reconcile displaced 2D content with the
   surface: per-tile-layer depth domains, the clip-space slack, the painter-order ordinals, the
   proxy push, and terrain subdivision of fills and lines at decode time.
4. **The terrain surface shader becomes the lighting stage** — sun, DEM normal, shadow lookup —
   because it is the one place where all 2D content and the ground geometry coincide.
5. **Keep a non-terrain path untouched.** None of this applies when terrain is off.

Costs accepted, unchanged from the earlier analysis: VRAM for the per-tile textures (512²–1024²
RGBA each), resampled rather than native-resolution vector content, and one extra terrain mesh
draw per stack.

## 5. Test harness

`scripts/android-dev` now takes intent extras, so one build covers many configurations:

```
adb shell am start -n com.akylas.cartotest/.MainActivity \
    --es demo composite --es drape true --es sat false --es hs true \
    --es tilt 60 --es zoom 14.2 --es lon 5.760595 --es lat 45.244172 --es ui false
```

Recognised keys: `demo` (composite/terrain/nuti), `lon`, `lat`, `zoom`, `tilt`, `rotation`,
`drape`, `drapeLines`, `drapeResolution`, `meshResolution`, `exaggeration`, `sat`, `hs`,
`contour`, `bg` (bare hex, `#` is a comment in the adb shell), `ui`, and `anim`/`animDelay`/
`animDuration`/`animZoomDelta`/`animLonDelta`/`animRotation` for scripted camera moves that can be
captured with `adb shell screenrecord` — animation defects do not show in still frames.

The drape state line in logcat reports what actually happened:

```
MapRenderer: RTT drape ACTIVE - layers 4, collected tiles 12, drawn tiles 12,
             resolution 1024, baked 60 tiles / 236 primitives, surface draws 12
```

`layers 1` where a composite is in use means the children are not participating; `surface draws 0`
means the drape never reached the screen. Both were real bugs, and both are invisible in a
screenshot.
