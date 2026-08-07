# The frame: threads, order, and what triggers a redraw

Scope: what happens between two `onDrawFrame` calls, on which thread, in which order.
For what is drawn inside the layer pass see [03-vt-renderer.md](03-vt-renderer.md).

## Threads

| Thread | Runs | Notes |
|---|---|---|
| **GL render thread** | `MapRenderer::onDrawFrame` and everything it calls | the only thread allowed to touch GL, except the two below |
| tile loading pool | `TileLayer::loadData` → data source fetch → decode → `vt::Tile` | `Options::setTileThreadPoolSize`, default **1** (tangram uses 2) |
| `CullWorker` | visible tile calculation per layer | `all/native/renderers/workers/CullWorker.cpp`, one per layer, debounced |
| `VTLabelPlacementWorker` | label placement for every vector layer | see [06-labels.md](06-labels.md) |
| `BillboardPlacementWorker` | billboard placement/visibility | kicked from `onDrawFrame` when `_billboardsChanged` |
| `TerrainDepthWorker` | terrain occlusion depth render + read-back | **its own EGL context, deliberately not shared** — see [04-terrain.md](04-terrain.md#occlusion-depth) |
| `ElevationTextureCache` encode worker | DEM → padded RGBA texture payload + `Bitmap` | GL thread only uploads |

Rule of thumb that has held for every perf round: **anything that allocates or frees megabytes, or
walks a whole tile, does not belong on the render thread.** The current hot list is in
[10-performance.md](10-performance.md).

## `MapRenderer::onDrawFrame` (all/native/renderers/MapRenderer.cpp:808)

In order:

1. **View state** — animation/kinetic handlers, camera clamp against the terrain height range, then
   `ViewState::calculateViewState` (projection, frustum, near/far — see
   [04-terrain.md](04-terrain.md#near-and-far-planes)).
2. **Optional offscreen bind** — only when a `PostProcessEffect` is set.
3. **Sky** — `SkyRenderer::onDrawFrame`; if it drew, the legacy sky band is skipped.
   `BackgroundRenderer` then draws the flat z=0 plane that fills the view past the terrain.
4. **`drawLayers`** — the whole map. Detailed below.
5. **Post-process, capture callbacks, billboard placement kick, idle notification.**

`PROF` timing sections (only in a `-PprofileRender` build) map onto this:
`sky` (which is mostly the swap-buffer wait, not work) `prelude` `prepare` `cover` `drape`
`layers` `layers3D` `billboards`.

## Inside `drawLayers` (MapRenderer.cpp:1755)

### a. Terrain prelude

When 3D terrain is on and the projection is planar:

- Every `TileLayer` is told its **stacking order** (`setTerrainRenderOrder`) and whether it is the
  **depth-writing** layer (the first visible, fully opaque tile layer).
- The **global terrain base fill** is drawn before all tile layers, so it shows through translucent
  layer content regardless of stacking order. With a depth-writing tile layer it is colour-only.
- The **cover** is computed: `collectTerrainCover` merges the terrain's own visible tiles
  (`TerrainRenderer::collectVisibleTiles`) with what the ground layers actually have, producing one
  shared list of tile ids, their **proxy depths**, and whether each is standing in for a finer tile.
- A cover leaf whose DEM has not arrived **walks up to the coarsest loaded ancestor** instead of
  being drawn flat. Without that, every tile on screen flashes in the bare ground colour during a
  zoom.
- Each layer is given `setTerrainGroundTiles`, an **ordinal base** (its slice of the depth order)
  and the stack's total **ordinal span** — see [05-depth-model.md](05-depth-model.md).
- Sun and shadow state are resolved **before** the ground draws, because each layer otherwise sets
  the sun from its own `onDrawFrame`, which runs later — the ground would light itself with the
  previous frame's sun.

### b. The ground

One draw pass over the shared cover, by the layer that *paints* it if any layer does (the hillshade
paint lives on that layer's renderer), otherwise by the first layer:
`groundDrawer->renderTerrainGround(groundColor)`. This is the only depth writer for the terrain
surface. Details in [04-terrain.md](04-terrain.md).

### c. The layers

Each layer's `onDrawFrame` in stack order, 2D pass first, then the 3D pass (`onDrawFrame3D`), then
billboards. For a vector tile layer this reaches `TileRenderer::onDrawFrame` →
`vt::GLTileRenderer` ([03-vt-renderer.md](03-vt-renderer.md)).

## What causes a frame

The renderer is **not** a free-running loop. A frame is drawn when something calls
`MapRenderer::requestRedraw()`: camera events, animations, a tile arriving, a label fade in
progress, a drape/ground cover change, an elevation version change. `MapRenderer::logRedrawSources`
exists to find out who is asking when the map will not settle.

Two consequences worth knowing:

- **fps is meaningless when the map is idle** — the bench scripts drive a scripted pan for exactly
  this reason ([10-performance.md](10-performance.md)).
- A subsystem that requests a redraw every frame without changing anything is an endless render
  loop, and it will not look like a bug — it looks like battery drain. `TileRenderer` logs when it
  has been waiting many frames on a pending elevation rebuild for this reason.
</content>
