# CARTO Mobile SDK (Akylas fork)

C++ map SDK for Android / iOS / UWP (and desktop via the same native core). This is the
Akylas/farfromrefug fork of CartoDB/mobile-sdk with many custom features (hillshade,
Valhalla routing, custom label rules, PMTiles, ...).

## Repository layout

| Path | What it is |
|------|-----------|
| `all/native/` | Core SDK C++ (layers, renderers, datasources, projections, ui, vectortiles...) |
| `all/modules/` | SWIG interface files (`*.i`) — public API surface, mirrors `all/native` |
| `libs-carto/` | **git submodule** (farfromrefug/mobile-carto-libs): `vt` (GL vector-tile renderer), `mapnikvt`, `cartocss`, `geocoding`, `sgre`/`osrm` routing, `nml` |
| `libs-external/` | **git submodule** (Akylas/mobile-external-libs): third-party deps (cglib, freetype, harfbuzz, ...). `boost` is expected as a symlink here (see BUILDING.md) |
| `android/`, `ios/`, `dotnet/`, `winphone/` | Platform glue code |
| `scripts/` | Build scripts (`build-android.py`, `build-ios.py`, `swigpp-*.py`, CMake in `scripts/build/`) |

**Submodule gotcha:** changes under `libs-carto/` or `libs-external/` must be committed
inside the submodule (branch `develop`), then the submodule pointer updated in the main
repo. Commit style is conventional-commits (`fix:`, `feat:`, `chore:`).

## Building / checking

Full builds take 1+ hour (see `BUILDING.md`; requires SWIG fork + boost symlink).
For fast iteration on the vt renderer, a syntax/type check is enough:

```sh
clang++ -fsyntax-only -std=c++17 \
  -I libs-carto/vt/src -I libs-external/cglib -I libs-external/stdext \
  -I libs-external/angle-metal/include \
  -I <dir-with-boost-or-stub> \
  libs-carto/vt/src/vt/<file>.cpp
```

boost is only used for `boost::math::constants::pi` in vt; a one-line stub header works
if `libs-external/boost` is not set up.

Useful cglib semantics (libs-external/cglib): `bbox::inside(bbox)` = *intersects* (not
containment); `frustum3::inside(bbox)` = *intersects frustum*.

## Rendering architecture (vector tiles + labels)

Threads: GL render thread (MapRenderer/onDrawFrame), tile-loading threads, plus
background workers in `all/native/renderers/workers/` (`CullWorker` computes visible
tiles per layer, `VTLabelPlacementWorker` runs label placement).

Data flow for a `VectorTileLayer`:

1. `CullWorker` → `TileLayer::calculateDrawData` → visible tile set.
2. `VectorTileLayer` decodes tiles (mapnikvt + cartocss) → `vt::Tile` with `TileLayer`s
   containing geometry + `TileLabel`s.
3. `TileRenderer` (all/native) wraps `vt::GLTileRenderer` (libs-carto/vt) which does all
   GL work: `startFrame` → `renderGeometry` → `renderLabels` → `endFrame`.

### Label pipeline (the flicker-sensitive part)

- `GLTileRenderer::setVisibleTiles` → `buildLabelMaps`: on **every tile-set change**, all
  `vt::Label` objects are recreated from the current tiles. Labels with the same
  `globalId` from different tiles are merged (`mergeGeometries` — one geometry copy per
  tile, identified by `(tileId, localId)`), and visibility/opacity/placement are carried
  over from the previous object via `snapPlacement`.
- `VTLabelPlacementWorker` (triggered by `MapRenderer::vtLabelsChanged` whenever draw
  data changes) creates **one fresh `vt::LabelCuller` per pass** and calls
  `TileRenderer::cullLabels` for every vector layer sequentially — the culler's screen
  grid intentionally accumulates across layers so labels of different layers collide.
  `LabelCuller::process` must therefore NOT clear the grid.
- `LabelCuller::process`: captures `wasVisible`, calls `Label::updatePlacement` (only
  re-places a label when its envelope fully left the frustum; resets opacity), projects
  envelopes to screen space, sorts by priority → wasVisible → layerIndex → size →
  opacity, then greedily inserts into a 16x32 screen grid with SAT polygon-overlap tests.
  A visible label keeps its slot unless a strictly higher-priority label overlaps it.
- The GL thread fades labels via `updateLabel` (`opacity` toward `visible ? 1 : 0`);
  invisible-but-fading labels stay rendered until opacity reaches 0.
- Custom per-label rules (fork additions): `allowOverlapSameFeatureId`,
  `sameFeatureIdDependent`, group ids with `minimumGroupDistance`. These compare the
  **placement's** `localId`, so placement identity stability matters.

**Placement stability invariant** (fix for labels jumping/disappearing while panning):
`snapPlacement` / `findSnappedPointPlacement` / `findSnappedLinePlacement` prefer the
geometry copy with the same `(tileId, localId)` as the previous placement. Without this,
re-snapping picks a winner by merged-list order (which changes with the tile set), and a
placement rebuilt from a differently-clipped copy of a line can fail line fitting
(`buildLineVertexData`) → the culler hides an already-visible label. Keep this invariant
when touching `Label`/`LabelCuller`.

Known remaining cost: `buildLabelMaps` reallocates every `Label` on every tile-set
change during panning; reusing unchanged labels would be the next perf win (careful: it
relies on fresh caches / snapPlacement semantics).

## Elevation / terrain (pointers for 3D terrain work)

- Elevation tile decoders: `all/native/rastertiles/ElevationDecoder.h` +
  `MapBoxElevationDataDecoder` (RGB-encoded) and `TerrariumElevationDataDecoder`.
- `all/native/layers/HillshadeRasterTileLayer.{h,cpp}`: consumes elevation tiles,
  has `getElevation(s)` queries; shading uses `vt::NormalMapBuilder`
  (libs-carto/vt/src/vt/NormalMapBuilder.cpp).
- Tile geometry/mesh generation: `vt::TileSurfaceBuilder` builds per-tile surface
  meshes; `vt::TileTransformer` (planar + spherical implementations in
  TileTransformer.cpp) abstracts tile-local → world transforms — 3D terrain would plug
  in here (displace surface meshes by elevation) plus depth handling in
  `GLTileRenderer`.
- Rendering projection modes: `Options::setRenderProjectionMode` (PLANAR / SPHERICAL);
  spherical mode already exercises the non-trivial TileTransformer paths.
