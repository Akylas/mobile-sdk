# 3D terrain: render-to-texture draping

Plan for making maplibre-style RTT draping the primary terrain architecture, replacing the
current "displace 2D content in 3D and separate it from the terrain surface with depth
biases" model.

## Why

**1. It eliminates a whole bug class by construction.** Today every draped layer is real 3D
geometry sharing a depth buffer with the terrain surface, so content and surface must agree
on height to within a depth-buffer step. They routinely do not, because the two are
tesselated differently, sample different DEM levels, and use per-tile lattices. Every such
disagreement is a visible artifact, and the mitigation is a bias that has to be
simultaneously large enough to clear chord error at range and small enough not to leak over
ridges up close. That constraint has no good solution — the history of this feature is a
long series of attempts at it.

With draping there is nothing to reconcile: 2D content is rendered flat into a per-tile
texture with no elevation and no depth, and the terrain mesh is the only geometry in the
depth buffer. maplibre sets `DepthMode.disabled` (test `ALWAYS`, writes off) for all draped
layers under terrain and orders them purely by painter's algorithm.

**2. Dynamic sun position and shadows become tractable.** With RTT the terrain mesh is the
single lit surface in the scene: sun direction, DEM-derived normal and a shadow-map lookup
all happen in one fragment shader modulating the draped color, and every draped layer gets
consistent lighting for free. In the current model each style shader would have to implement
lighting and shadow lookup itself, and content sitting at marginally different depths than
the surface is shadow-acne territory.

It also makes hillshade **dynamic**: computing the normal from the DEM in the terrain shader
and lighting it with the live sun replaces the pre-baked hillshade raster layer — which also
retires the separate hillshade depth domain.

**3. Perf.** Draped content is baked once and cached, so static frames cost only the mesh
draw, and terrain-driven geometry subdivision at decode time disappears (currently a known
CPU cost — vt geometry is subdivided to the mesh cell size on every tile decode).

## Costs, accepted up front

- **VRAM.** maplibre uses 1024² RGBA per tile per stack (~4 MB); ~20 visible tiles × 2 stacks
  ≈ 160 MB. CARTO's current drape texture is 512² (1 MB). Resolution must become an API knob.
- **Content is resampled, not native-resolution.** Thin lines get softer. maplibre
  compensates with a 2× quality factor and anisotropic filtering.
- **Layer ordering costs passes.** Every non-drapeable layer sandwiched between drapeable
  ones forces a new "stack": another texture per tile and another full terrain mesh draw.
- **Label occlusion becomes per-anchor**, not per-pixel.

## Reference: maplibre-gl-js

Verified against v6.0.0. Key mechanics:

- `LAYERS_TO_TEXTURES` (`src/webgl/render_to_texture.ts`) — only `background`, `fill`, `line`,
  `raster`, `hillshade`, `color-relief` are draped. `symbol`, `circle`, `heatmap`,
  `fill-extrusion`, `custom` render live in 3D.
- Drape render uses an **orthographic tile-local matrix**
  (`mat4.ortho(0, EXTENT, EXTENT, 0, 0, 1)`), with terrain uniforms suppressed, so the FBO
  content is a flat 2D render of one tile square.
- One **shared FBO** whose color attachment is swapped per tile; textures recycled through a
  pool.
- Terrain mesh is a **shared static 128×128 grid** (129² verts) reused by every tile, with
  **skirts** to hide cross-zoom hairline stitches. Drape UV is simply `a_pos3d.xy / EXTENT`.
- Elevation sampled per-vertex from the DEM with manual bilinear (`get_elevation`), and
  `u_terrain_matrix` maps a child tile into a region of a **parent** DEM — so a terrain tile
  never lacks elevation.
- Terrain mesh draws `LEQUAL`, depth read+write, in a depth range shared with 3D extrusions.
- Symbols lift themselves by elevation and fade against a **packed RGBA screen-space depth
  texture** rendered from the terrain mesh; labels never touch the depth buffer.
- Drape textures cached on the tile, invalidated by a fingerprint of contributing source tile
  keys + source revision.

maplibre-native has no 3D terrain at all — gl-js is the only reference.

## What CARTO already has

`GLTileRenderer::renderDrapeTextures` + `renderTileSurfaceDrape` are a working seed:
per-tile 512² RGBA texture, one shared FBO, bake-once-and-cache with a 4-tiles-per-frame
budget, an orthographic tile-local MVP override, and a `DRAPE` shader flag sampling the
texture as the surface color. The drape surface draws at true depth **with depth write**, so
it becomes the real occluder, and content tests `GL_LEQUAL` with zero bias.

Gaps that keep it from being primary:

1. **Only fills + backgrounds are draped** (lines optional). Rasters/hillshade are explicitly
   excluded — so the depth-bias machinery has to stay alive anyway, i.e. two architectures in
   parallel rather than one.
2. **Drape ownership is per-`GLTileRenderer`, i.e. per tile layer.** There is no cross-layer
   stack concept, so a hillshade layer and a vector tile layer cannot share a drape texture
   and instead keep their separate depth domains.
3. **VectorLayer elements are not draped** (cross-layer FBO ownership).
4. No texture pool (alloc churn on pan), no re-bake when a late style layer arrives, no
   fade-in (content pops at full opacity), fixed 512² with no API.
5. `_drapeFBO`/`_drapeTextures` are not released in `deinitializeRenderer`/`resetRenderer` —
   leak / stale handles on GL context loss.

## Target design

Lift drape ownership from `GLTileRenderer` to the renderer level so that all drapeable
content across all tile layers bakes into one texture set per terrain tile:

1. **Stack splitter** in `MapRenderer`: walk the layer order, group contiguous drapeable
   layers into stacks, and for each visible terrain tile bake one texture per stack.
2. **Drapeable set**: backgrounds, fills, lines, rasters/bitmaps, hillshade. Not: 3D
   polygons, labels/billboards, and (initially) VectorLayer elements.
3. **Terrain mesh draw per stack**, sampling that stack's texture, `LEQUAL` + depth write —
   the only depth-writing geometry.
4. **Non-drapeable layers render live in 3D between stacks**, depth-testing against the mesh.
5. **Retire** the per-tile-layer depth domains (`glClear(DEPTH)` per layer), the clip-slack
   bias, the painter-order ordinal machinery and the terrain geometry subdivision, once
   nothing depends on them.
6. **Texture pool** keyed by size, with a fingerprint-based invalidation like maplibre's.

## Staged plan

Each stage should be independently testable and shippable.

- **S1 — Productionize the existing drape path.** Texture pool + release on context loss,
  configurable resolution, re-bake on late layer arrival, fade-in. No architecture change.
- **S2 — Drape rasters/bitmaps and hillshade.** This is the stage that actually removes the
  depth interaction for the layers that currently misbehave.
- **S3 — Cross-layer stacks.** Lift ownership to `MapRenderer`; one texture set per terrain
  tile shared by all tile layers; terrain mesh drawn once per stack.
- **S4 — Retire the depth-bias machinery** for everything now draped; keep it only for 3D
  polygons and elements.
- **S5 — Elements and labels.** Decide whether VectorLayer elements join the drape or stay
  live with a depth test against the mesh; port maplibre's packed-depth-texture occlusion for
  labels.
- **S6 — Dynamic sun + hillshade in the terrain shader**, then shadow maps.

## Open questions

- Drape resolution vs. sharpness for thin vector content at high zoom. maplibre's 2× quality
  factor is the starting point; CARTO's current 512² is likely too low.
- Whether comp-op style layers can be draped at all (`_overlayBuffer2D` has no depth
  attachment today).
- Whether to keep a non-RTT fallback path for devices without the memory budget, and if so
  whether it is worth maintaining two models long-term.
