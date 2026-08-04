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

`setSurfaceResolution` caps the elevation level to what the mesh can express (roughly one texel per
half surface cell), which costs about two zoom levels of detail. That cap is right for geometry and
wrong for per-fragment shading, which is why the terrain paint can opt out of it
([07-hillshade-contours.md](07-hillshade-contours.md#the-dem-level)).

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

Their far plane (`2·height/cos(pitch + fovy/2)`) is available as
`TerrainOptions::FarPlaneFactor` but changes nothing at the cameras tested: the ground-derived far is
already inside the bound it gives.

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
