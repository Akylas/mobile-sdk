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

## Working in this checkout

`scripts/android-dev` is the live test bench. It is ONE composable demo, not a set of examples:

| File (under `app/src/main/java/com/akylas/cartotest/`) | Role |
|---|---|
| `demo/DemoConfig.java` | every default, one static field per knob + the intent-extra key map (`applyIntentOverrides`) |
| `demo/DemoCfg.java` | `cfgBool/cfgFloat/cfgInt/cfgStr/cfgColor` intent readers (`--es key value`) |
| `demo/DemoMap.java` | builds/updates the map: layer registry, shared sources, terrain/light/sky, camera |
| `demo/DemoStyles.java` | style decoders (dir / zip / inline CartoCSS / nuti project) + demo shaders |
| `demo/DemoSky.java` | day-cycle sun/sky + generated sky shader |
| `demo/DemoPanel.java` | on-screen panel — writes DemoConfig, then calls a `DemoMap.apply*()` |
| `demo/DemoTests.java` | one-shot actions (routing, search, GeoJSON) |
| `ui/main/SecondFragment.java` | Android glue only (view, permissions, map listener) |

Layers (`base`, `satellite`, `hillshade`, `hypso`, `contour`, `routes`, `elements`) toggle live
from the panel or with `--es <name> true|false`; the base map has `--es base plain|composite` and
`--es style dir|zip|inline|nuti`. `dir` reads the style from a FOLDER via `DirAssetPackage`
(`/sdcard/alpimaps_mbtiles/osm`), falling back to `osm.zip` then to inline CartoCSS.

Change defaults in `DemoConfig` only — those fields are also what the panel mutates. These files
may carry **uncommitted** local edits (camera, per-demo knobs): read before touching, keep changes
additive, never restore from a backup or an older commit.

Comparing against older SDK code (A/B-ing a regression) takes three steps, not one:

```sh
git checkout <sha> -- all/                       # 1. old sources
(cd libs-carto && git checkout <matching-sha>)   # 2. matching submodule commit
cd scripts && python3 swigpp-java.py --profile "standard+valhalla+geocoding+routing+packagemanager" \
  --swig /Volumes/dev/carto/mobile-swig/swig     # 3. regenerate wrappers, else the build fails
```

The checked-in `generated/` wrappers reference the newer API and will not compile against older
headers. Restore the same way (`git checkout HEAD -- all/`, submodule back to its branch,
regenerate). SWIG is never run by gradle — any change to `all/modules/*.i` needs step 3 too.

`gh pr create` needs `--repo Akylas/mobile-sdk` (or `--repo farfromrefug/mobile-carto-libs`).
Both repos are forks of the archived CartoDB originals, and without `--repo` gh targets the
upstream and fails with "Repository was archived so is read-only".

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
