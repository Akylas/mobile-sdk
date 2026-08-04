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

Layers (`base`, `satellite`, `hillshade`, `hypso`, `contour`, `contourTiles`, `routes`, `elements`) toggle live
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

## The Android demo app (the main dev loop)

`scripts/android-dev` builds the native SDK *and* the demo in one gradle run. This is the
fast loop — not the full `build-android.py`:

```sh
cd scripts/android-dev && ./gradlew :app:assembleDebug -x lint   # ~40 s incremental (native included)
adb install -r -t app/build/outputs/apk/debug/app-debug.apk      # -t: the APK is test-only
adb shell am force-stop com.akylas.cartotest
adb shell am start -n com.akylas.cartotest/.MainActivity --es ui false --es drape false
```

- Install from `app/build/outputs/apk/debug/`. `app/build/intermediates/apk/debug/` also holds
  an `app-debug.apk` and it is **stale** — installing it silently runs old code. Verify with
  `unzip -p <apk> classes*.dex | strings | grep <new symbol>` (the demo's Java lands in classes5.dex).
- Tiles need **60-90 s** to settle before a screenshot means anything (network + persistent
  cache + label placement). `pm clear com.akylas.cartotest` resets camera/caches; without it the
  persistent tile caches stay warm, which is usually what you want.
- `--es demo terrain|nuti|composite` picks the configuration (default `composite`).
  Every knob in `applyTerrainConfig`/`applyCameraConfig`/`applySkyAndLightConfig` is an intent
  extra, so most experiments need no rebuild: `lon lat zoom tilt rotation`, `drape drapeLines
  drapeResolution meshResolution exaggeration`, `fog fogStart fogDistance viewDistance`,
  `hs sat satZoom contour bld3d stitch`, `daycycle sunHour sunAzimuth sunAltitude shadow`,
  `ui false` (hide the panel), `anim zoom|zoomseq`.
- Runtime UI: the gear at the bottom-left opens a settings panel (checkboxes + sliders for
  drape, mesh resolution, sun, shadows, fog, max visible distance). Driving it from adb works:
  `adb shell input tap 84 2236` toggles the panel, `input swipe` scrolls it and drags sliders.
- **Camera clamp gotcha**: the terrain keeps a `cameraClearance` above the ground, so a start
  position inside a slope (e.g. lat 45.2442 / lon 5.7606 at z14.68) auto-zooms out to ~z11.6 and
  you get an empty grid screen. Zoom out a notch (z13.6) instead of assuming the render broke.

## Debugging the renderer (what actually works)

- **A/B by feature, per screen row band, before probing the plumbing.** Screenshot with a layer
  on and off (`--es hs false`, `--es sat false`, `--es drape false`), diff the two, and count
  differing pixels per horizontal band (PIL, no numpy on this machine). If a band is 0.0%
  different, that content is *not being drawn there*; if it differs, it is drawn and the problem
  is elsewhere. This is what separated "tile never loaded" from "tile drawn but depth-rejected".
- Probes go in these places, in this order — layer culling → draw data → vt render tiles → draw:
  `TileLayer::buildFetchTiles` (visible set + cache misses, `typeid(*this).name()` tells you
  which layer), `TileRenderer::refreshTiles` (per-zoom tiles/bitmaps/geometries, log `this` to
  separate the composite's children), `RasterTileLayer::FetchTask::loadTile` (why a tile is not
  stored), `ElevationTextureCache::getTexture` (render zoom → DEM grid zoom), and
  `GLTileRenderer::renderGeometry2D` for what is actually visible/blended per target zoom.
- **vt has no logger** — use `__android_log_print(4, "carto-mobile-sdk", ...)` there. When
  throttling a probe with a frame counter shared by several renderer instances, use a **prime**
  modulus: `% 120` with 4 instances always logs the same one.
- `Shader::getUniformLoc` returns **0** for a uniform the compiler dropped, and 0 is a valid
  location — writing to it clobbers whatever lives at 0. `SkyRenderer` uses `glGetUniformLocation`
  and `>= 0` guards for this reason; copy that pattern.
- `setDebugWireframe` / `setDebugSurfacePrefill` in `TileRenderer.cpp` are hardwired false and
  mostly wash the frame out; the A/B diff above is more informative.

## Terrain, fog and sky wiring

- One resolution point for both: `all/native/components/StyleEnvironment.h` — `resolveLighting()`
  and `resolveFog()` merge the app's `LightOptions`/`TerrainOptions` with the style's Map-block
  values, and `resolveFog` also *lights* the fog (dark at night, warm at a low sun). Every
  consumer (`TileRenderer` → vt, `BackgroundRenderer`, `SkyRenderer`) must go through them, or
  the ground and the sky end up with different fog.
- `SkyRenderer` draws a full-screen ray-direction quad. Apps can replace the body with
  `SkyOptions.setShaderSource` — the wrapper declares `u_sunDir/u_sunColor/u_skyColor/
  u_horizonColor/u_groundColor/u_fogColor/u_fogBlend/u_time/u_zoom/...` and a `fogAmount(rayDir)`
  helper; redeclaring any of them is a compile error and the renderer silently falls back to the
  built-in sky (watch for that when a custom sky "does nothing").
- `BackgroundRenderer` draws the flat z=0 plane that fills the view past the terrain (and past
  `TerrainOptions.ViewDistanceFactor`). It uses `Options.getBackgroundBitmap()` — **not** the
  CartoCSS `Map { background-color }`, which is why changing the style background does not tint it.
- `TerrainOptions.ViewDistanceFactor` ends the ground (tangram's rule: 2 x camera height / cos(pitch
  + fovy/2), capped at 127 tile widths; 1 = their rule verbatim). Pair a short one with fog or the
  ground ends on a hard edge.

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

**Full technical documentation lives in [`docs/rendering/`](docs/rendering/README.md)**, split by
subsystem so one page can be read without the rest: the frame and threads, tiles and LOD, the GL
draw path, 3D terrain, the depth model, labels, hillshade/contours, lighting/sky/fog, the composite
layer, performance method, and the tangram comparison. The summary below is the orientation; that
set is the detail.

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
