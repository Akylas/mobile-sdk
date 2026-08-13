# 3D terrain: elevation, surfaces, and the shared ground

Scope: how the ground is built and drawn. Depth relationships are in
[05-depth-model.md](05-depth-model.md); shading of the ground is in
[07-hillshade-contours.md](07-hillshade-contours.md).

## Elevation data

`ElevationManager` (all/native/terrain/) owns decoded `ElevationTileGrid`s (one per DEM tile) in an
LRU cache and answers height queries: `getTileGrid`, `getDisplayHeight`, `getMinMaxDisplayHeight`,
with a `LoadMode` (`CACHED_ONLY` never blocks). Every consumer — the mesh, label anchoring, element
placement, billboard occlusion — must go through it, or two parts of the frame disagree about where
the ground is.

**The grid cache is a tile count, not a byte budget.** A grid is the source raster (768 KB for a
512×512 RGB DEM, 192 KB for a 256×256 one), so a fixed 64 MB meant 85 grids for one source and 340
for another. One terrain view needs 122–167 of them — the cover pyramid, the contour source's finer
tiles, the border prefetch — and every grid past the limit evicted one still in use, which was then
decoded again on the next pass: **1525 loads of 167 distinct tiles, 32 s of WEBP decode per
startup** on a Crosscall. The cache now grows on the first decoded grid to hold `MIN_CACHED_GRIDS`
(192) of them, i.e. 144 MB for a 512² source and 36 MB for a 256² one; 192 was the first value where
loads equalled distinct tiles (128 still re-decoded ~20%). `TerrainOptions::setElevationCacheCapacity`
still wins over the rule, for an app that cannot spend the memory.

`setSurfaceResolution` caps the elevation level to what the mesh can express (roughly one texel per
half surface cell), which costs about two zoom levels of detail. That cap is right for geometry and
wrong for per-fragment shading, which is why the terrain paint can opt out of it
([07-hillshade-contours.md](07-hillshade-contours.md#the-dem-level)).

### CPU height queries

`getDisplayHeight`/`getElevationMeters` are point queries, and their callers are dense: the label
re-anchor walks every vertex of every label, the raycast marches a ray. Three things make one sample
cheap, and each of them was a measured frame cost before it existed:

- **The last grid, kept.** `getGridForInternalPos` remembers the last resolved grid per thread and
  reuses it while the point is inside its bounds. Without it every sample paid a projection
  transform, a tile id (`IntPow` alone was 21% of the render thread), a flip and the zoom clamp
  before reaching the cache — to find the tile the previous sample had just used. `lookupTileGrid`
  keeps a second memo for the callers that arrive with a tile id already.
- **The latitude scale, quantised.** `getDisplayScale` is `tanh`-based and was 21% of the render
  thread on its own (`tanh` + `expm1`). It is now memoised over a ~40 m latitude quantum, which
  moves a height by under two millimetres and — being a function of the position alone — keeps the
  same vertex at the same height frame after frame.
- **Two versions, and what they mean.** `getVersion` moves on *any* change; `getDataVersion` moves
  when the elevation DATA changes, **including a tile load**. A consumer tells an exaggeration ramp
  (heights scale on the GPU, surfaces stay valid) apart from new data by comparing the two.
  `_dataVersion` used to stand still for tile loads, so `TileRenderer` read every arriving DEM tile
  as scale-only and took the blanket invalidation path — a whole screen of label anchors resampled
  per tile, instead of the labels over that tile. Setting the same exaggeration twice is now a
  no-op for the same reason.

### The elevation texture

Displacement happens in the **vertex shader** (vertex texture fetch), so each tile needs its DEM as
a texture. `ElevationTextureCache` (all/native/renderers/utils/) turns a grid into one:

- keyed by the **grid's own tile**, so overzoomed tiles and all layers share one texture per DEM
  tile, and neighbours sampling the same level sample one continuous texture;
- the payload is a **padded (W+2)×(H+2) RGBA re-encode** with a 1-texel border taken from up to 8
  neighbour grids (cross-level backfill and an edge box filter), so shared tile edges agree
  bit-exactly and the surface does not crack;
- encoding **and** the `Bitmap` construction run on a worker thread; the render thread only uploads,
  under a per-frame budget (`MAX_UPLOADS_PER_FRAME`, `MAX_UPLOAD_MS_PER_FRAME`). A tile with no
  texture yet renders flat, which is the visible cost of a budget set too tight;
- a neighbour landing patches only the **2-texel ring** (`encodeTextureBorders` →
  `applyBorderPatches`, `glTexSubImage2D` into the live texture *and* into the bitmap behind it,
  which is what survives a context loss). Measured over a cold load: full re-encodes 353 → 24. It
  changes no frame rate — the encode was never on the render thread — so treat it as work removed,
  not speed gained. In a warm pan the whole pipeline is **idle**: zero encodes, zero patches;
- `_frameResolved` memoises the per-frame tile → grid resolution, because the provider is called
  once per tile **per render pass** and each miss costs 9 locked cache lookups.

**How tangram does it:** the DEM raster is bound as the tile's own texture, uploaded once when the
tile loads, ancestors addressed through uv sub-rects (`u_raster_offsets`), edges extrapolated in the
shader (`res/scenes/elevation.yaml`). No re-encode, no border machinery — but also no cross-level
edge filtering, which is a seam feature this fork wants. The port that keeps both is to upload the
grid's own samples and patch borders as small `glTexSubImage2D` strips.

## Surfaces

Two mechanisms exist; **regular-grid mode is what runs**:

- **Shared regular grid** (`TileSurfaceBuilder::buildRegularGridSurface`): ONE unit grid of
  `TerrainOptions::MeshResolution` cells, built once, drawn for every tile with the tile's own
  matrix and uniforms. `surfIndices / surfDraws` comes out at exactly `24576 = 64·64·2·3` — one grid
  per draw. This is tangram's `RasterStyle` arrangement (`core/src/style/rasterStyle.cpp`), and it is
  already implemented: there is nothing left to port here.
- **Per-tile adaptive surfaces** (`buildTileSurface`, red-green edge-local refinement over corner
  fans) — used only by the non-grid draw path and by ray-cast picking
  (`findTileBitmapIntersections`). In grid mode `_tileSurfaceMap` stays **empty**, and both
  `invalidateTileSurfaces` and `resetTileSurfaces` iterate nothing. Verified with counters:
  `surfBuilt=0 surfInval=0` for a whole pan.

Because the grid is regular, it also means picking through `_tileSurfaceMap` finds nothing in grid
mode — the pick path is the one consumer left that would need a lazily built surface.

### Edge stitching

A coarser neighbour interpolates the DEM between its own (2^k wider) lattice nodes, so a fine tile
must chord across the same nodes on that shared edge or the seam cracks open.
`buildTerrainEdgeCoarsening` computes, per tile, how much coarser each of its four neighbours is, and
the surface shader collapses the edge accordingly. Two things this got wrong once:

- the map must be built from **the cover that is actually drawn**, not from a layer's own visible
  tiles (`GLTileRenderer::terrainSurfaceTileIds`: ground cover, else paint cover, else own tiles);
- the lattices only line up when the resolution is a multiple of the level difference, which caps k.

Draped **content** takes the same coarsening, not just the surface: a road or a contour crossing the
seam has to land on the same stitched edge as the ground it lies on, or its two halves meet at
different heights — invisible from straight down, a step as soon as the camera tilts. The edge test
is `pos.x < 0.00001`, which is only meaningful for surface vertices (they *are* the unit square), so
content converts first with `uTileUnitScale`. Only the outermost cell is affected. Note the feature
is off by default (`TerrainOptions::TileEdgeStitching`), and on its own it does not fix content
mismatches at junctions — see the tile clipping in
[03-vt-renderer.md](03-vt-renderer.md#lines-over-terrain), which is the dominant cause.

**Open:** this conversion uses the scale alone, so for a **stand-in** (an ancestor tile serving a
finer target while it loads) it measures from the ancestor's origin rather than the drawn tile's,
and picks the wrong edge. The tile clip had the same bug and was fixed with a `uTileUnitOffset`; the
same offset applies here by the same argument, but adding it moves settled contour positions by
changing the elevation interpolation (2.8 % of the frame at the camera above), so it is left for a
deliberate on-device comparison rather than folded into the clipping fix.

### Skirts: deliberately absent

Tile border skirts (walls dropped at tile edges to hide cracks) are **disabled**. Their walls,
textured with stretched edge pixels, rasterize over neighbouring content wherever a displaced tile
edge leans off-nadir — solid fill-coloured patches that grow with the tile size. Tangram has none
either. Cross-LOD cracks are handled by stitching instead.

## The shared ground

One cover for the whole layer stack, one ground pass, then every layer composites onto it in layer
order. This replaced a per-layer depth pre-pass and a per-layer stencil mask
(4209 mask draws and 24.7 ms per interval → **0**), and it is what tangram does: one shared grid
mesh, one draw per tile, no pre-pass, no masks anywhere in `core/src`.

The cover comes from `MapRenderer::collectTerrainCover` and is seeded by the terrain's own visible
tiles — what the camera can see, not what the layers happen to have fetched. Two rules hold it
together:

1. **The ground is drawn at its true depth and is never pushed back.** Everything after it is
   `GL_LEQUAL` with no bias in either direction.
2. **Ground-shaped content is drawn on the cover tiles, not on the layer's own tiles.** Two
   tesselations of one height field do not agree; on the cover they are coincident to the bit. This
   is why a hillshade at z12 is drawn on the z14 cover.

### Stand-ins

A cover leaf whose DEM has not arrived walks up to the coarsest **loaded** ancestor and is drawn
there once (duplicates collapsed). Drawing it flat instead makes every tile flash in the bare ground
colour during a zoom. Tiles that stand in carry a proxy depth; live tiles carry zero, however coarse
they are — see [05-depth-model.md](05-depth-model.md#proxy-depth).

## Near and far planes

Terrain mode floors the near plane at **camera height / 50**, which is tangram's
`core/src/view/view.cpp:452`. The old behaviour — near taken from the nearest visible ground point,
floored at 1/16 of an internal unit — gave centimetre near planes next to a slope, a far/near ratio
of 10⁴–10⁶, and NDC depth so non-linear that a constant-NDC bias was worth hundreds of metres at
range. That is the mechanism behind every see-through this project has had.

"Camera height" is the smaller of the distance to the **focus** and the height above the **terrain
under the camera** (`ViewState::setTerrainCameraReference`, published every frame by the renderer
next to the clearance). Tangram's `m_pos.z` is the distance to what the camera looks at and their
camera is held a distance away from the terrain itself (the depth at the screen centre against
`minCameraDist`); ours is held a *clearance above the ground under it*, so at a low tilt the focus
is kilometres away while the ground is a couple of hundred metres below — and a fiftieth of the
focus distance then parks the near plane in front of the ground at the bottom of the screen and
cuts it away. Over flat ground with the focus close the two distances are the same; the cost of the
smaller one is bounded by 1/sin(tilt) (2× at tilt 30), so the depth budget is only spent in the
close-to-terrain case that needs it.

The floor is a floor and the ground walk is a **ceiling** too, but only when the view is pitched
away from the camera geometry — free roam looking up, or a first person camera
([13-celestial.md](13-celestial.md#seeing-them-free-roam)). The walk takes the near plane from
where the sampled rays MEET THE GROUND; as the view pitches up those hits move off into the
distance, the near plane follows them out, and everything close to the camera is clipped away —
worse the higher the view goes. What is near the camera does not move when the view turns, so in
that case `near` is capped by the same camera-height rule, which does not depend on the view
direction at all.

Their far plane (`2·height/cos(pitch + fovy/2)`) is available as
`TerrainOptions::ViewDistanceFactor` but changes nothing at the cameras tested: the ground-derived far is
already inside the bound it gives.

## The camera against the terrain

`TerrainOptions::CameraClearance` keeps the camera a height above the ground under it. It is a
**bound on the zoom** (`ViewState::getTerrainMaxZoom`, clamped in `CameraZoomEvent::calculate`)
plus a per-frame correction in `MapRenderer` for the paths that lower the camera without zooming —
panning into a hillside, tilting, a DEM tile arriving. Three rules keep a gesture against that
bound from throwing the map somewhere else:

- **The bound stops a zoom in; it never drives a zoom out.** A zoom event scales the map about its
  pivot, and with the pivot under the fingers, clamping a zoom-*in* request to below the current
  zoom scales the map the other way about that point — the map jumps sideways, once per pinch tick.
  Getting back onto the shell is the renderer's correction, which zooms about the focus and moves
  nothing sideways.
- **A zoom is never cancelled for want of a ground hit.** `TouchHandler::calculatePivotPos` falls
  back to the focus when the ray under the fingers misses the anchor plane or lands past the far
  plane. Close to the terrain the far plane is short and half the screen is sky, so requiring a hit
  (which the pinch, the wheel and the double tap all did) left the map unable to zoom out at all —
  the "I have to pan somewhere else before I can move" symptom.
- **The scale and the angle come from the SCREEN, not from the ground.** A pinch and a two-finger
  turn are what the fingers did (tangram: `InputHandler::handlePinchGesture` /
  `handleRotateGesture`, fed by the platform gesture detector). Taking them from where the two rays
  meet the ground makes a grazing ray — a low camera, a finger near the horizon — into most of the
  answer. The pan is still world-anchored (that is the point of a map pan) and goes through one
  path for both gestures, `TouchHandler::panBetween`, which honours `PanningSpeedMode` and, below
  tilt 15, caps the travel at what the finger's pixels are worth at the map scale — tangram's
  `getTranslation` guard for a near-horizontal view.

`isValidScreenPosition` tests the plane the gesture is actually anchored to (the terrain height
under the touch, `_gestureAnchorHeight`), not sea level: in the mountains the two are hundreds of
metres, and at a low tilt kilometres of ray, apart.

### The zoom pivot sank the focus, and everything was drawn at the wrong scale (fixed 2026-08-13)

**Symptom.** In 3D, zoom very close to the terrain, pan, then pinch back out: the map sticks in a
state where everything is blurry and oversized, and stays that way while zooming out. Enough
movement clears it. In 2D the same state shows labels, shields, peak icons and line widths several
times too large for the zoom on screen, the `VectorLayer` route line with them. Reported as
"blurry", but it is a SCALE fault, not a resolution one. Only ever reproduced with a real style
(the packaged one) — an inline style whose widths and sizes are constants shows almost nothing,
because the fault is in what the zoom-dependent style functions are evaluated at.

**Cause.** `CameraZoomEvent::calculate` shifted the map about the pivot with the **full 3D**
offset `pivot − focus` (`ProjectionSurface::calculateTranslateMatrix`), and the pinch pivot carries
the terrain height under the finger (`TouchHandler::calculatePivotPos` → `_gestureAnchorHeight`).
Every zoom-*out* about a pivot above the focus therefore pushed the focus DOWN by
`(pivotZ − focusZ)·(scale − 1)`. Close to a slope that is a few hundred metres per gesture, and it
accumulates.

The focus height is not cosmetic: `dist(camera, focus)` is the distance the whole zoom scale is
calibrated on (`_zoom0Distance / 2^zoom`, [Near and far planes](#near-and-far-planes) above). With
the focus below the ground, that distance stops describing the distance to what is on screen — so
the tile walk asks for a zoom several levels too coarse (the blur) while every zoom-dependent width
and label size is evaluated for that same far-out zoom (the oversizing), against terrain that is
actually a tenth as far away.

**The fix** is tangram's model verbatim: the pivot moves the map **along the surface only**. Their
pinch correction is a ground translate in x/y (`View::translate`, `core/src/view/view.cpp:258`) and
their view height is derived from the zoom, so a pivot on a mountain cannot move the view point up
or down. `CameraZoomEvent` now forces the pivot to the focus's own height before building the
shift, which means it can no longer change the focus height in any mode — including the lifted
viewpoints of free roam and the peak finder, which set that height deliberately (and which the old
code could silently drag back down to the ground).

The visible trade is theirs too: pinching with a finger on a summit holds the point at the *focus
height* under the finger, so a high point drifts slightly on screen during the pinch.

**How it was found, in numbers.** A probe on `dist(camera, focus)` against `zoom0Distance / 2^zoom`,
printed once a second next to the focus and camera heights, during the gesture on the device:

```
zoom=12.15 dist=589   ratio=1.0000  focusZ=-218  camZ=76.5
zoom=11.06 dist=1259  ratio=1.0000  focusZ=-501  camZ=128.1   <- label depth to the terrain: 115
```

`ratio` staying at 1.0000 is what makes this readable: the invariant the SDK maintains was intact
the whole time — the camera distance did match the zoom. What was broken is the *unwritten* second
invariant, that the focus is on the ground you are looking at. The 1259 against a terrain depth of
115 is the entire bug.

Two things this rules out, both of which cost a round: the camera-clearance clamp (it was active
and correct — `maxTerrainZoom` tracked the zoom throughout), and the sag tesselation above (both
arms measured identical through a scripted zoom sequence — edge energy 17.4/24.7/17.2/25.0 against
18.0/24.8/17.8/24.8 — and the report predates it). A scripted `setZoom` sequence never reproduces
it either: it zooms about the focus, so there is no pivot to sink anything. The demo's
`--es anim approach` (dive, pan, pull out) is that sequence, and its clean run is what pointed at
the pivot.

**Labels partly hid it.** `Label::calculateTerrainScaleFactor` rescales a label by
`depth / focusDistance` to cancel the perspective divide, and that ratio cancels exactly this error
too (it read 0.09 while the fault was worst). Geometry, fills and vector elements have no such
cancel, which is why lines looked worse than text at first and why the 2D screenshot — where the
cancel is near 1 — was the clearer evidence.

**Residual, not fixed here.** Even with the focus where the app put it, on a z=0 plane under a
1000 m ridge the focus still sits below the ground, so `dist` still overstates the distance to what
is on screen — the same error, milder and always on. Tangram's answer is to derive the render zoom
from the terrain depth at the screen centre (`m_zoom` from `m_elevationManager->getDepth(centre)`,
clamped to `[m_baseZoom, m_maxZoom]`, `core/src/view/view.cpp:403-415`). Porting that redefines what
`getZoom()` means for tiles, styles and labels alike, so it is its own change — see
[11-tangram-diff.md](11-tangram-diff.md#the-zoom-is-calibrated-on-the-focus-not-on-the-terrain).

## The surface shader

`TerrainOptions::setSurfaceShaderSource` lets the application paint the terrain surface itself. It
replaces the background bitmap and the background colour as the base fill (precedence: shader >
bitmap > colour) and is drawn by `TerrainRenderer::renderSurface` where those are — globally,
before any tile layer, with the same `keepDepth` semantics. So a map with **no tile layer at all**
still shows shaded relief; that is the relief (peak-finder) case, and it is what
[14-post-process.md](14-post-process.md) draws its lines over.

The shader defines `vec4 surfaceColor()` and gets `v_normal` (world space), `v_worldPos`,
`v_elevation` (metres, before exaggeration), `v_dist` (metres from the camera), the resolved sun
(`u_sunDir`, `u_sunColor`, `u_sunIntensity`, `u_ambientIntensity`), the resolved fog (`u_fogColor`,
`u_fogRange`, plus a `fogAmount(dist)` helper), `u_time`, `u_zoom`, `u_resolution` and every
parameter set with `setSurfaceParameter` / `setSurfaceColorParameter`. Sun and fog come from
`resolveLighting` / `resolveFog` ([08-lighting-sky-fog.md](08-lighting-sky-fog.md)), so a shaded
surface, the tile content and the sky agree on the light. Redeclaring a provided name is a compile
error and the shader is dropped (logged, falls back to the bitmap/colour fill) — the same trap as
`SkyOptions::setShaderSource`.

Two implementation notes:

- **Normals are per-vertex and lazy.** `TerrainRenderer::ensureSurfaceAttribs` fills a
  normal + elevation array from the mesh's own height field the first time a mesh is used by the
  surface pass — central differences in tile-local space, which is a world direction because the
  tile matrix scales x, y and z alike. The depth passes never allocate it (at grid 96 it would be
  150 kB per tile).
- **Nothing else asks for elevation when there is no tile layer.** The tile layers are what
  normally drive DEM loads, so the surface pass prefetches the DEM for its own visible tiles
  (`ElevationManager::prefetchTileGrid`) and keeps requesting frames until they arrive — the same
  argument, and the same code, as the terrain paint cover. Without it the surface shades a flat
  height field and the map goes idle on it.

## Occlusion depth

Billboards and vector elements need to know whether a point is behind a ridge. The terrain is
rendered into an FBO and read back — `glReadPixels` is a full pipeline stall (55–62 ms measured), so
it runs on `TerrainDepthWorker`: its own thread, its own EGL context, deliberately **not shared**
(the pass draws CPU meshes from client memory with its own program and FBO, so a job just holds
`shared_ptr`s and nothing crosses contexts). The render thread only collects meshes (~0.8 ms).

Two GL contexts still share one GPU, so the submit interval matters more than the work:
every frame 13.2 fps, 250 ms 14.3, **500 ms 14.9** (13.7 synchronous). Tangram does the same thing
with a shared context and never waits on it.
</content>

## Draped lines sagging into the terrain (no regular grid)

Symptom: lines do not sit on the surface — a route reads as sunk into a ridge or floating over it,
worst at low zoom, straightening as you zoom in, at any tilt.

`TerrainTileTransformer` has two line-subdivision paths, and only one of them is exact:

- **regular-grid mode** (`TerrainOptions::setRegularGridEnabled`, **off by default**) cuts each
  segment exactly where it leaves a surface triangle (`tesselateSegmentOnLattice`), so every
  sub-segment lies *in* one triangle and follows the surface exactly;
- **without it** there is no lattice to cut against, so segments are only halved until shorter than
  a threshold. A sub-segment one mesh cell long still chords across the cell's diagonal fold and
  sags below it.

The bug was that this second path used `lineDivideThreshold = divideThreshold`, i.e. lines shared the
fill threshold **including its DEM-texel floor** (`max(tileMeters / meshResolution, demTexelMeters)`).
That floor answers "how much elevation detail exists", which is the right bound for a fill but the
wrong one for the sag: the sag is against the surface **mesh**, not the DEM. Since the threshold is
proportional to the tile, the error scaled with tile size — hence better on every zoom in.

Lines are now cut `LINE_SUBDIVISION_FACTOR` (4) times finer than the mesh cell, with no texel floor.
Lines are 1D, so the same factor costs a fraction of what it would on a fill, and the residual sag
falls linearly with the sub-segment length.

Not fixed here: without the regular grid the sag is only *reduced*, never zero. Turning on
regular-grid mode is what removes it, and that is a larger change ([05-depth-model.md](05-depth-model.md)).

### What that subdivision costs over a city

**Line subdivision is the single reason panning over a city is slow.** Crosscall, the app's own
style, a 25 s scripted pan at 45.188/5.724 z15 t45, interleaved:

| | fps | GPU `layers` | geometry indices / frame |
|---|---|---|---|
| shipped | 6.6 | 51.3 ms | 2.90M |
| 3D buildings off | 6.6 | 50.7 ms | — |
| **area** subdivision off entirely | 6.7 | 50.6 ms | 2.83M |
| **lines** at source density | **13.5** | **20.9 ms** | 0.74M |
| terrain off altogether | 21.7 | 11.8 ms | 0.72M |

Fills are innocent: turning area subdivision off changes nothing, because fills are draped and baked
once. Lines are never draped — they are drawn as terrain geometry every frame — and a city is mostly
lines. In regular-grid mode the **lattice split** does the cutting, at every cell edge and diagonal:
about 64 cuts per tile crossing at z15 with `meshResolution` 64, per road.

Two things this reveals:

- **The split runs whatever the relief.** The only flatness gate is `FLAT_HEIGHT_RANGE_EPSILON`
  (0.001 m), so a valley tile is cut exactly like a cliff to protect against a fold it cannot have.
  `debug.carto.latticerelief <metres>` skips the split under a given relief: the city goes 6.61 →
  7.57 fps (`layers` 51.3 → 36.9 ms) at 200 m, and adding `debug.carto.linethreshold 8` on those
  tiles reaches **8.43 fps / 32.5 ms**. The mountain camera does not move (11.4–12.0 fps) — the gate
  never fires there, which is the point.
- **`debug.carto.linethreshold` alone does nothing** in regular-grid mode: the lattice split is tried
  first and returns, so the threshold is only a fallback for segments spanning very many cells. Any
  measurement of line cost has to go through the lattice, not the threshold.

The remaining gap to source density (8.4 against 13.5) is the tiles that legitimately have relief —
the mountains standing in the far half of a tilted city view. They are cut as finely as if they were
under the camera, because subdivision cost is per tile and **independent of the tile's size on
screen**.

### Where this should go: pay in depth, not in vertices

Tangram does not subdivide at all. `res/scenes/terrain-3d.yaml` displaces every vertex in the vertex
shader and pays for the chord with depth instead — `depth_shift = -0.02*u_proj[2][3]`, larger near
the camera where the chord error is. We already ported that shift, and we already have the better
tool for a line: `uDepthClearance`, a clearance worth the same number of METRES at any range, which
is exactly what a chord over relief needs.

What blocks using it is that `setTerrainLineClearance` is **one global value**, so it has to cover
the worst tile on screen — which is why the code notes that un-subdivided lines need a lift so large
it "shines everything through".

### Cutting a line by its sag instead of by the tile's cell count

`debug.carto.linesag <metres>` (`tesselateSegmentBySag`, 0 = shipped) splits a segment only where the
terrain under it actually leaves the chord, recursively, until the residual sag is under the
tolerance — expressed in METRES so it is the same currency as the depth clearance that lifts these
lines. It replaces both the lattice split and the fixed threshold.

**The insight is that sag measures curvature, not slope.** A road running along a constant slope
chords perfectly: its sag is zero and it needs no cut at all. Only a break in slope needs one. The
lattice, which cuts at every cell edge and diagonal, was therefore paying about 4x the geometry the
terrain's shape actually asks for.

Crosscall, the app's style, 25 s scripted pan, `linesag 2`:

| | fps | GPU `layers` | indices / interval |
|---|---|---|---|
| city, shipped | 6.61 | 51.0 ms | 19.2M |
| city, sag 2 m | **13.42** | **21.0 ms** | 9.96M |
| mountain, shipped | 12.76 | 23.6 ms | 17.7M |
| mountain, sag 2 m | **21.12** | **11.1 ms** | 8.3M |

The mountain gains MORE than the city in relative terms, which is the point: relief does not imply
curvature. 2 m and 10 m give the same geometry — the tolerance is not what binds at these zooms —
while 0.01 m against 0.5 m does differ (3.43M against 3.39M indices), so the splitter is live and
tracks the tolerance.

Checked on screen at 45.244172/5.760595 z13.6 t45 and z11 t60: roads, tracks, trails and contours
land identically with and without it. The GeoJSON route line is broken at z11 in BOTH arms — that is
the open route-following issue, not this. Still opt-in: what it has not had is a slow pan across a
convex break at low zoom, which is the shape that produced cracks before.
