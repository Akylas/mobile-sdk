# 3D terrain draping — architecture, comparison, and roadmap

How CARTO drapes vector map content over 3D terrain, how it compares to tangram-ng
and MapLibre Native, and where to take it next.

## The problem

Draped content (polygon fills, lines/contours, and VectorLayer elements like routes)
must **follow the terrain surface exactly**. Get it wrong and you see one of:

- **cracks / poke-through** — the terrain mesh rises above the content and occludes it in
  patches;
- **see-through** — content behind a ridge shows *over* a nearer ridge;
- **holes** — fills sag below the surface and are occluded.

All three come from the same root: content and the terrain **occluder surface** are two
different piecewise-linear approximations of the same height field, and any depth bias used
to separate them trades one artifact for another (the depth-slack tuning that dominated the
terrain work).

## Three approaches

| | CARTO (this SDK) | tangram-ng | MapLibre Native |
|---|---|---|---|
| Drape mechanism | VTF vertex displacement; **fills draped as RTT texture** | VTF vertex displacement, no subdivision | **render content flat → texture → drape onto mesh** |
| Surface mesh | shared regular grid (meshResolution), DEM-displaced in VTF | shared 64-grid, VTF | shared 128-grid, VTF |
| Fill/line hug | RTT fills (exact) + lattice-clamped geometry lines | source-density, one global `depth_shift` | RTT (exact) for all drapeable layers |
| Occlusion of draped content | true-depth drape surface writes depth; content `GL_LEQUAL`, zero bias | one global constant bias (accepts leak, fog hides) | none needed — content is baked into the surface texture |
| Labels/symbols over terrain | elevated + billboard-occluded (CPU) | CPU elevation + depth-readback | not wired on the terrain-3d branch |
| Decode cost vs flat | ~flat for draped content (no subdivision) | flat (no subdivision) | flat (flat render) |

**Key lesson:** the artifact-free approaches (CARTO RTT fills, MapLibre) never let draped
content depth-interact with the terrain — the content *is* the surface's texture, so there
is nothing to z-fight. tangram's single-bias model is cheap but has a permanent quality
ceiling (it leaks; fog hides it). CARTO takes the MapLibre route for fills and keeps sharp
geometry for lines/elements where texture softness would matter.

## Current state (what this PR ships)

- **Fills** — maplibre-style render-to-texture drape (`TerrainOptions.DrapeFillsEnabled`).
  Baked flat per tile, sampled on the terrain grid surface, cached per tile. Zero holes /
  see-through by construction. The drape surface writes **true depth** and is the terrain
  occluder.
- **Contours / tile lines** — sharp displaced geometry, lattice-clamped to the grid, drawn
  `GL_LEQUAL` + zero bias (no leak). Optionally draped too (`DrapeLinesEnabled`) — softer but
  zero-cost hug. The anti-diagonal crack is fixed by finer line subdivision
  (`REGULAR_GRID_LINE_SUBDIVISION`).
- **Vector elements** (blue line) — `GL_LEQUAL` + zero clip bias, tunable painter-order
  clearance (`ElementTerrainSlack`). Far-distance leak fixed; **low-zoom leak remains** (see
  below).
- **Perf** — draped content skips terrain subdivision (baked flat), so its VBOs upload at
  source density instead of ~meshResolution² per tile. LRU elevation-texture cache (no full
  flush). Together these cut the fast-zoom render-thread stall.

Working config:

```java
terrainOptions.setPainterOrderDepthEnabled(true);
terrainOptions.setDrapeFillsEnabled(true);   // also the true-depth occluder
terrainOptions.setDrapeLinesEnabled(true);   // optional: cheap+draped contours
terrainOptions.setMeshResolution(64);
```

## Roadmap — next improvements

Ranked by value / tractability.

### 1. RTT draping for VectorLayer elements (fixes the low-zoom element leak)
The blue element line still leaks at low zoom: it is CPU-baked to the **fine bilinear**
height while the low-zoom occluder is a **coarse grid**, so it rides above the surface and
`GL_LEQUAL` passes it behind ridges. No depth bias wins this (above→leak, below→crack).
The MapLibre fix applies: bake the element layer flat into the per-tile drape texture so it
*becomes* the surface. **Cross-layer work** — elements live in different layers than the
terrain-tile layer that owns the drape textures; needs a drape-element callback from the tile
renderer into the element renderers (per-tile ortho, flat render into the drape FBO), plus an
MVP-override draw path on `LineRenderer`/`PolygonRenderer`. Softens the line (texture).

### 2. Stream / off-thread geometry VBO upload (zoom hang for sharp content)
`buildCompiledTileGeometry` uploads each tile's VBO with `glBufferData` on the **render
thread** on first draw. Draped content now skips subdivision, but **sharp** contours/roads
(DrapeLines off) still upload subdivided geometry — a fast-zoom tile burst stalls. Options:
per-frame upload budget (careful: content pop-in), or a proper async/streamed upload path
(PBO / worker context). This is the remaining first-zoom cost when sharp geometry is kept.

### 3. Drape-texture productionization
Per-tile drape textures are cached and bake-once. Follow-ups: fade-in (currently pops in at
full opacity), re-bake on progressive multi-layer load (a late layer can miss the cached
bake), configurable drape resolution (currently 512²; thin content sharpness vs memory), and
a shared texture pool to avoid alloc churn.

### 4. Zero-slack regular-grid surfaces (backlog)
Tile surfaces are red-green edge-local refinements, not a true regular grid, so the shader
lattice-clamp only approximates them. A genuinely regular grid surface (tangram/maplibre
style, one shared static grid VBO) would let the shader lattice-clamp *exactly* (zero slack)
and remove the per-tile surface tesselation entirely. Groundwork exists
(`RegularGridEnabled`, `buildRegularGridSurface`, in-shader `uElevationLatticeCell`).

### 5. Comp-op / overlay layers bypass terrain occlusion
`_overlayBuffer2D` has no depth attachment, so comp-op style layers render without terrain
depth testing (they would draw over terrain). Fine today (default styles have none) but a gap
if such styles are used with terrain.

### 6. Labels
Label re-anchoring re-samples elevation per point on every tile-set change (mutex per point).
It is correct but not cheap; a batched or grid-cached elevation query would reduce the cost
without the placement-instability that a naive per-label defer caused (reverted). MapLibre
Native has not solved terrain symbols at all — CARTO is ahead here.

## References
- tangram-ng: `core/src/style/rasterStyle.cpp`, `core/shaders/*.vs`, `maps/assets/scenes/{terrain-3d,elevation}.yaml`
- MapLibre Native (feature/terrain-3d): `src/mbgl/renderer/{render_terrain,texture_pool,render_target}.cpp`, `include/mbgl/shaders/gl/terrain.hpp`
- CARTO: `libs-carto/vt/src/vt/GLTileRenderer.cpp` (drape pass, depth model), `all/native/terrain/TerrainTileTransformer.cpp` (subdivision), `all/native/components/TerrainOptions.h` (API)
