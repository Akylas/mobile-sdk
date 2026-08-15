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
  ≈ 160 MB. Massif's current drape texture is 512² (1 MB). Resolution must become an API knob.
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

## S3 design: cross-layer stacks

The obstacle is structural: maplibre has one painter walking one layer list, whereas CARTO
gives every `TileLayer` its own `TileRenderer` → its own `GLTileRenderer`, each with its own
visible tile set, tile surfaces, drape textures and depth domain. A hillshade layer and a
vector tile layer therefore cannot currently share a drape.

Ownership moves out of `GLTileRenderer`:

- **`TerrainDrapeCache`** (native) owns the shared FBO, the texture pool, the per-tile
  textures keyed by `(terrainTileId, stackIndex)`, their fingerprints, and the resolution. One
  instance per map, held by `MapRenderer`.
- **`GLTileRenderer` becomes a content baker.** It no longer owns drape textures; it exposes
  which target tiles it would drape and their content fingerprint, bakes its own drapeable
  content for one tile into whatever FBO is currently bound, and can draw a terrain surface
  textured with an externally supplied texture.

Per frame, `MapRenderer`:

1. Splits the layer order into stacks of contiguous drapeable tile layers.
2. Unions each stack's drape tiles and combines their fingerprints.
3. For each stale tile: binds the shared FBO with that tile's texture, clears, and asks each
   participating renderer to bake **in layer order** into the same texture.
4. Draws the terrain surface once per stack per tile, sampling that stack's texture.
5. Renders non-drapeable layers live in 3D between stacks.

Sequencing note: in practice the common configuration (hillshade + vector tiles) is a single
stack, because both are drapeable and contiguous. Multiple stacks only arise when a
non-drapeable layer sits between drapeable ones. So the stack index is plumbed through from
the start, but single-stack is implemented and verified first — it delivers the whole win for
the usual case at a fraction of the risk.

Once a stack owns the surface draw, the per-tile-layer depth domains (`glClear(DEPTH)` per
layer), the clip-slack bias and the painter-order ordinal machinery have no remaining
consumers among draped layers and can be deleted (S4).

## Open questions

- Drape resolution vs. sharpness for thin vector content at high zoom. maplibre's 2× quality
  factor is the starting point; Massif's current 512² is likely too low.
- Whether comp-op style layers can be draped at all (`_overlayBuffer2D` has no depth
  attachment today).
- Whether to keep a non-RTT fallback path for devices without the memory budget, and if so
  whether it is worth maintaining two models long-term.

---

# UNPARKED — state as of this branch

This branch is **not merged and not a fix for the terrain see-through artifact**. It builds and
runs. It was parked once; it is now active again, because the landcover-holes investigation
(`terrain-landcover-holes-investigation.md`) identified the branch as one of the two real fixes
for that bug — the per-layer drape path cannot be made consistent across composite invocations,
a shared drape removes the disagreement by construction.

## Done

- **S1/S2** — full drapeable set (backgrounds, rasters, fills, lines), overzoomed/proxy content
  draped through a sub-rect bake, texture pool, fingerprint-based re-bake, release on context
  loss, `TerrainOptions.setDrapeResolution` (default 1024). Draping defaults on and implies the
  regular grid.
- **S3** — `TerrainDrapeCache` owns the FBO and per-tile textures above the layers;
  `GLTileRenderer` acts as a content baker (`collectDrapeTiles` / `bakeDrapeTile` /
  `renderDrapedSurface`); `TileRenderer::prepareFrame` splits `startFrame` out of `onDrawFrame`
  so every layer's tiles exist before any bake; `MapRenderer` drives collect → normalize → bake
  → single surface draw. Single stack.
- **S4** — the decode-time win is already live: draped content is baked flat, so
  `TerrainTileTransformer` skips terrain subdivision for both fills and lines.

## Deliberately not done

Collapsing to one renderer per map (the literal maplibre shape). Its advantage is that the drape
target and depth buffer are owned above the layers, which `TerrainDrapeCache` achieves;
collapsing would mean rewriting the public `TileLayer`/`Layers` API for little further gain.

## Defects found in this branch, all from S3's surface takeover

Taking the surface away from the per-layer path means reproducing every guarantee that path
quietly provided. Three were found one symptom at a time; assume more exist.

1. The drape tile set was a **union** across layers, so a coarse surface and the finer ones over
   the same ground were both drawn and fought. Fixed by normalizing to one non-overlapping cover.
2. A layer's **proxy parent and live children** both matched the covering test and baked into one
   texture in arbitrary order, so a parent's background painted over a child's content. Fixed by
   baking coarsest-first.
3. `collectDrapeTiles` reported **only tiles that already had content**, so tiles still loading
   got no surface at all and the global terrain background showed through. Fixed by reporting
   every visible tile.

Unverified after the last fix. If the artifact now appears only briefly while tiles load, the
next step is clearing the bake to the terrain background colour instead of transparent, so a
surfaced-but-contentless tile reads as terrain rather than a hole.

## Fixed in this round

1. **The main pass redrew everything the drape had already baked.** `drapedTile` was derived from
   `_drapeTilesThisFrame`, which only the per-layer path fills; under the cross-layer drape it was
   empty, so every layer painted its fills *and its depth-writing tile background* again as
   displaced 3D geometry — reinstating exactly the background-over-fill depth failure the drape
   exists to remove. Now `MapRenderer` hands each layer the frame's drape tile set explicitly
   (`setExternalDrapeTiles`), and `isTileDraped` tests coverage against it. The hand-off has to be
   explicit: the layer's own `startFrame` runs between the surface draw and its content pass, so
   anything the renderer infers from the draw itself is reset before it is read.
2. **The cover dropped ground instead of splitting it.** A coarse tile containing one finer tile
   was discarded whole, leaving 15/16 of its ground with no surface at all. The cover is now a
   real quadtree partition: split until no leaf contains a finer collected tile.
3. **The bake started from transparent.** A texel no style layer paints stayed transparent, and
   the map background plane — which in terrain mode is *behind* the terrain — showed through. The
   bake now clears to the terrain background colour, falling back to the layer's own style
   background colour (new `Layer::getBackgroundColor`).
4. **Unrelated crash, hit while testing:** `PersistentCacheTileDataSource::loadTile` dereferenced
   a null `TileData` whenever the wrapped source failed (offline, HTTP error). Pre-existing on
   master.

## The actual open bug: drape textures that sample black

Established by probe, in this order — each one killed a family of explanations:

- The bake **works**: reading the FBO back after baking gives landcover green, water, and the
  background clear colour, and a full-texture PPM dump is a correct flat map of the tile.
- The drape **surface is drawn** over the whole screen: forcing the drape fragment shader to
  output solid red paints all the terrain red.
- The drape **uv is in range**: flagging `vDrapeUV` outside [0,1] lights nothing.
- The sampled **texture is bound and non-zero**: the texture ids logged at bind time match the
  ids `MapRenderer` baked into.
- Yet forcing alpha to 1 while keeping the sampled rgb renders the near terrain **black**, and
  with the background plane disabled the near terrain is black while a mid-distance wedge shows
  correct draped content.

So *some* tiles sample their texture correctly and others sample black, with everything that is
easy to check (bake, binding, uv, coverage) already verified. Neither detaching the texture from
the FBO before sampling nor `glFlush()` after baking changes it.

Next probes, cheapest first: log `needsBake`/`baked` per tile alongside the drawn tile list, to
see whether the black tiles are ones `TerrainDrapeCache::acquire` marked baked without a bake
(note it sets `baked = true` before the bake actually runs, so any path that skips the bake
poisons the entry permanently); then check whether the black set is exactly the tiles created
from the texture pool rather than freshly allocated.

Note that fix 1 makes this visible where it was previously masked: before it, the duplicated 3D
content painted over the un-textured surface, so the map looked *nearly* right and failed only in
the depth-test holes. The RTT path is therefore currently worse-looking than before on the
emulator, and better-founded.

Diagnostics left in place: a `RTT drape ACTIVE/INACTIVE` state dump and a periodic
`RTT drape tiles zoom a..b, count n` line in `MapRenderer`. Remove when no longer useful.

## Note on process

Six causes were proposed for the artifact and five were wrong; two fixes introduced new bugs and
one commit described as inert crashed the app on start. What actually moved the diagnosis was
evidence, not inference: the wireframe check, the green/grey detail, the single-layer test and
the magenta bake-clear each eliminated whole families of explanation. Prefer a cheap decisive
probe over another plausible patch.
