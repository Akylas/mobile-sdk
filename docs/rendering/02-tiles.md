# Tiles: which ones, how they load, what comes out

Scope: from the camera to a decoded `vt::Tile`. The GL side is [03-vt-renderer.md](03-vt-renderer.md).

## The pipeline

```
CullWorker (per layer, background)
  └─ TileLayer::calculateVisibleTiles          visible + preloading tile lists
       └─ TileLayer::buildFetchTiles           what is missing from the caches
            └─ tile loading pool: data source → decoder → vt::Tile
                 └─ TileRenderer / GLTileRenderer::setVisibleTiles  (render tiles, labels)
```

- `TileLayer::loadData` (all/native/layers/TileLayer.cpp:269) runs a cull state through the layer.
- `VectorTileLayer` decodes with mapnikvt + cartocss into a `vt::Tile`: a list of `vt::TileLayer`s,
  each with geometries (`TileGeometry`) and labels (`TileLabel`).
- `RasterTileLayer` decodes to a bitmap; `HillshadeRasterTileLayer` additionally builds a normal map
  (`vt::NormalMapBuilder`) — except in paint mode, where it has no tiles at all
  ([07-hillshade-contours.md](07-hillshade-contours.md)).

## Which tiles are visible (the LOD rule)

`TileLayer::calculateVisibleTilesRecursive` walks the quadtree from the root and subdivides while
the tile's **projected screen area** is at least that of a 2x2 block of nominal tiles - tangram's
rule (`TileManager::updateTileSets` + `View::getTileScreenArea`), ported whole:

```cpp
// four tile corners at surface level -> clip space -> screen space, shoelace area
bool subDivide = screenArea >= _lodMaxTileArea;   // (2 * tileSizePixels * lodFactor)^2, x 4^-zoomLevelBias
```

`Options::TileLODFactor` scales it: 1 (the default) is their rule verbatim, larger keeps tiles
coarser everywhere at a tilt - fewer tiles, fewer far labels, less far detail - and 0 turns the area
test off, refining everything to the camera zoom (the behaviour before this rule).

with three bounds applied: the data source's `getMaxZoom()`, the camera's discrete zoom
(`viewState.getZoom() + bias + DISCRETE_ZOOM_LEVEL_BIAS`), and, in terrain mode,
`TerrainOptions::MaxTileZoomOffset`.

In the near field this is the same density as the distance rule it replaced (refinement stops when a
tile covers between one and two tile sizes on screen, which is what the discrete zoom bound gives
anyway). The difference is at a **grazing angle**, where a tile's screen area collapses with the
foreshortening while its distance barely grows: at tilt 10 the old rule kept refining the whole
horizon band, and measured at 45.187/5.719 z16.2 t10 the visible set went from ~66 tiles a frame to
~24, with the submitted index count down 4x. That band is also where every one of those tiles
contributed a full set of labels for a few pixels of screen.

Three terrain-specific details, each of which was a bug once:

- **The LOD area is taken at surface level, not from the elevation-expanded bbox.** Otherwise
  subdivision decisions change as elevation streams in, and the visible tile set (and with it tile
  and DEM fetching) churns forever. "Surface level" is the elevation **at the screen centre**
  (`_lodElevation`), tangram's choice too (`View::getTileScreenArea`, "use elevation at center of
  screen"): one value for the whole cull, so it moves with the camera and not with the tile.
  Measuring a mountain tile as if it lay at sea level puts it further away and makes it smaller, so
  it stays coarse exactly where the terrain is high and steep — blurred hillshade, a blunt depth
  occluder that ridges leak through, and a wide LOD spread that tears at tile borders.
- **Target tiles may exceed the data source's max zoom** in terrain mode
  (`_terrainOverzoomTargets`, fed by the existing overzoom machinery). A z12 DEM-derived hillshade
  under a z15 camera otherwise renders surfaces many times coarser than the base map's, and those
  blunted ridges are leaky depth occluders - content and vector elements show through near crests.
- **The view distance stops the recursion**, tested against the nearest point of the tile. It is
  tangram's too (`ViewState::calculateViewDistance`): `2 * cameraHeight / cos(pitch + fovy/2)`,
  capped at 127 tile widths (their `MAX_LOD` 6), scaled by `TerrainOptions::ViewDistanceFactor`
  (1 = their rule verbatim, 0 = as far as the visible ground goes). `cameraHeight` is the larger of
  the zoom-derived camera-to-focus distance and the camera's height above sea level: they are the
  same thing for tangram's camera, but a viewpoint standing on a 2600 m summit is high above the
  ground while its zoom says it is close to it, and the zoom-derived quantity alone ends the
  panorama a few kilometres out - the closer to the terrain, the less of it you see.
  **Above 1 the factor also deepens the far plane** (`calculateViewDistances`), because otherwise
  the extra tiles the walk fetches are drawn and then clipped, and raising the factor appears to do
  nothing. It costs depth precision — the depth model is calibrated on the far/near ratio (see
  [05-depth.md](05-depth.md)) — so it is an explicit trade, not the default. A style may pin an absolute
  distance in metres with `terrain-max-visible-distance`. Pair a short one with fog
  ([08-lighting-sky-fog.md](08-lighting-sky-fog.md)) or the ground simply ends.

### The coarsening floor times the view distance

`TerrainOptions::MaxTileZoomCoarsening` floors how coarse a far tile may get (default: 3 levels
under the camera), so that far surfaces stay usable as depth occluders. It **overrides the
screen-area rule**, which would happily coarsen the horizon on its own — and that is fine until it
is multiplied by a long view distance. The cost of a long view is not the distance, it is the
distance paved in tiles no coarser than the floor.

Measured at the demo's default camera, a 170 km pinned view distance:

| coarsening floor | tiles fetched | deepest zoom |
|---|---|---|
| 3 | **550** (all z13) | z13 |
| 8 | 50 | z15 |

The 550-tile case is a map that loads for minutes and blinks one tile at a time as each arrives,
because every tile is fetched, decoded, draped and re-baked. Note the near field gets *finer* with
more coarsening allowed (z15 against z13): the budget goes where it is visible instead of paving
the horizon.

`TileLayer::calculateVisibleTiles` therefore **relaxes the floor rather than shortening the view**:
whatever the app configured, it allows enough coarsening for the covered ground to fit in
`TERRAIN_COVER_TILE_BUDGET` (256) tiles, and logs when it does. Two settings that each look
reasonable can be ruinous multiplied together, and an app has no way to see that coming.

## Substitution, preloading, caching

- `TileSubstitutionPolicy` decides whether a missing tile is stood in for by a parent/child.
- **Stand-in depth is not overzoom depth.** `MaxOverzoomLevel` (6) says how far up the SDK may go for
  the *data* of a tile the source does not have. `MaxStandInLevel` says how far up it may reach for
  something to *show meanwhile*, and they were one number until it became clear they pull in opposite
  directions: a stand-in must cover a zoom-in of several levels or the map goes empty exactly when the
  user asked for detail (a depth of 1 blanks every layer on a two-level zoom), while a deep stand-in
  redraws the same area from ever coarser tiles. Default 6; lower it per layer for a source whose look
  changes with zoom.
- **The preview parent is fetched last, not first.** When several visible tiles share a missing parent,
  `TileLayer` fetches that parent too "to provide quick rendering". Its `PARENT_PRIORITY_OFFSET` used to
  be **+1** and the fetch list sorts by priority descending, so the coarse preview went out *before* the
  tiles actually wanted — the map showed a ladder of coarser redraws even when the real tile would have
  arrived just as fast, and for a source that generates tiles (traced contours) each preview was a full
  pass thrown away seconds later. It is **−1** now: wanted tiles first, preview after.
- Preloading tiles are those inside an enlarged frustum (`PRELOADING_TILE_SCALE`) but not visible;
  they are fetched at lower priority so panning does not start from nothing.
- Tiles live in the layer's memory cache plus an optional persistent cache
  (`PersistentCacheTileDataSource`). The persistent cache is why a device re-run is not a cold run —
  `pm clear` is the only reliable reset ([10-performance.md](10-performance.md)).

## Geometry density: what gets subdivided, and why

With the shared ground, content is displaced per vertex by the same elevation function the ground
uses, so it does **not** need to be tesselated to follow the terrain — tangram does not subdivide at
all. Ours subdivides **area fills only**, to two surface cells
(`TerrainTileTransformer.cpp`, `AREA_THRESHOLD_CELLS = 2`, overridable with
`adb shell setprop debug.carto.areathreshold N`).

The reason is not the displacement, it is the depth model: an un-subdivided fill chords across the
displaced surface, and the constant clip-space `depth_shift` has to cover that chord
([05-depth-model.md](05-depth-model.md)). Measured on device at the mountain camera:

| fill subdivision | fps | geometry indices per render tile |
|---|---|---|
| 1 cell | 16.6 | 158k |
| **2 cells (shipped)** | **20.6** | **48k** |
| 4 cells | 21.2 | 19k |
| source density (none) | — | shows floating-fill patches |

Lines are **not** subdivided by density: they are cut exactly where they cross a surface-lattice
line (`x = k·cell`, `y = k·cell`, `x + y = k·cell` in tile uv), so every sub-segment lies inside one
surface triangle. Exact, fewer vertices, and no depth slack needed. Tangram displaces line vertices
in the shader instead and subdivides nothing; that costs **13× the index throughput** here, and a
coarse or proxy tile's roads chord straight across the terrain until the finer tile arrives — the
"roads go straight when zooming out" report.

**Fills are subdivided even when draping is on**, and that is deliberate. Draping bakes fills flat,
so their subdivision is wasted work for a draped tile — but draping is decided **per tile at render
time** while the density is decided **globally at decode time**. An un-subdivided fill that then
does not get draped sags below the surface and leaves the bare background colour (the "landcover
holes"), and tiles fall through the drape cover constantly: the cover is capped at the camera zoom
and a hillshade contributes its DEM-limited zoom, so render tiles finer than the cover are normal.
Suppressing those tiles instead was tried — the map becomes a stretched coarse drape. The
subdivision is not what costs: emulator, `meshResolution` 128, drape on, scripted 3-level zoom gives
median 58–60 fps and the same worst-case bake spike either way.

The decode-time density flags must match between `TileLayer::calculateDrawData` and
`resetTileTransformer`, or tiles decoded for the other mode stay in the cache forever.

## Where tiles come from

Standard sources (HTTP, MBTiles, PMTiles, assets) plus two of interest here:

- **`ContourTileDataSource`** — generates contour MVT from a DEM source on the fly, sharing the DEM
  with the hillshade so terrain tiles are fetched once. It has a label-stub mode that makes it emit
  only what labels need; see [07-hillshade-contours.md](07-hillshade-contours.md).
- **elevation sources** — decoded by `MapBoxElevationDataDecoder` / `TerrariumElevationDataDecoder`
  into `ElevationTileGrid`s held by `ElevationManager` ([04-terrain.md](04-terrain.md)).
</content>

## Two binary formats: MVT and MLT

`MBVectorTileDecoder` reads either. `setTileFormat` takes `TILE_FORMAT_AUTO` (the default),
`TILE_FORMAT_MVT` or `TILE_FORMAT_MLT`. Everything downstream of the decode — CartoCSS,
symbolizers, nuti parameters, the `vt::Tile` — is shared; only the bytes-to-features step differs.

```
MBVectorTileDecoder ─ createFeatureDecoder() ─┬─ MBVTFeatureDecoder  (protobuf, pbf.hpp)
                                              └─ MLTFeatureDecoder   (libs-external/mlt)
                                                        │
                             LayerFeatureDecoder ───────┘  ← what LayerTileReader reads
```

`LayerFeatureDecoder` is the seam: it owns the tile-to-target transform, the clip box and the
feature-id override, and declares the four layer operations (`getLayerNames`, `hasLayer`,
`createLayerFeatureIterator`, `findFeature`). `LayerTileReader` — formerly `MBVTTileReader` — reads
through it, so it serves both formats unchanged. `TorqueFeatureDecoder` stays on the plain
`FeatureDecoder` base: its features are addressed by frame, not by layer.

### Detecting the format

There is no magic number, but the two are separable by **framing**. An MLT tile is a sequence of
`(varint layer length, varint layer tag, body)` that has to tile the buffer exactly, and the layer
tag is 1 or 2. MVT is protobuf — its first byte is `0x1A` (field 3, wire type 2) and it does not
fit that shape. `MLTFeatureDecoder::isTileData` runs that check; `TILE_FORMAT_AUTO` calls it.

Measured against maplibre-tile-spec's own fixture corpus — 663 `.mlt` and 134 `.mvt`, the same
OpenMapTiles and Bing tiles in both formats plus the synthetic set:

| rule | MLT detected | MVT false positives |
|---|---|---|
| framing, layer tag = 1 | 654/663 | 0/134 |
| framing, layer tag ∈ {1,2} | **663/663** | **0/134** |

The 9 tag-1 misses are `test/synthetic/0x02/`, an experimental layer tag `mlt::Decoder` skips
anyway. Note what the corpus does *not* cover: every `.mvt` in it comes from one encoder family, so
an exotic MVT producer — leading extension fields, say — has not been tested against this. Set the
format explicitly when the source is known and none of that matters.

MapLibre itself does not detect: its style spec puts `encoding: mvt|mlt` on the source, defaulting
to `mvt`. `TILE_FORMAT_MVT`/`TILE_FORMAT_MLT` are the equivalent, for a source that declares.

**A declaring source wins over detection.** `TileDataSource::getMetaData(key)` reads the container's
own metadata — the MBTiles/PMTiles table — and returns empty for sources that carry none. The
`VectorTileLayer` constructor asks the source for `encoding`, then `format`, runs them through
`MBVectorTileDecoder::parseTileFormat`, and pins the decoder when either is conclusive; a decoder
the app already set explicitly is left alone. The wrapper sources (`Cache`, `Contour`, `Ordered`,
`Combined`) forward the lookup the way they forward `getEncoding`.

`parseTileFormat` matches case-insensitively by substring, because generators spell this
differently. **`pbf` is deliberately inconclusive**: MapLibre's own demotiles declare
`"format": "pbf"` with `"encoding": "mlt"`, so treating `pbf` as proof of MVT would force the wrong
decoder on a tileset detection gets right. Only an explicit `mvt` / `…mapbox-vector…` pins MVT;
`maplibre` or `mlt` anywhere pins MLT; everything else falls through to the framing check.

Note this pins one format per decoder. A decoder shared between layers whose sources differ in
format should be left on `TILE_FORMAT_AUTO`, which costs 1-19 ns a tile.

Detection needs uncompressed bytes, so `MBVectorTileDecoder::createFeatureDecoder` inflates first
and hands the plain buffer to whichever decoder it picks — the decoder's own `inflate_tile` then
only re-checks three magic bytes.

Behaviour differences worth knowing:

- **MLT decodes the whole tile.** `mlt::Decoder::decode` returns every layer and every property
  column; the format has no lazy path and its C++ API takes no layer filter. MVT decodes per layer,
  on demand, and only the attributes the style asked for (`fields`). On an OpenMapTiles schema with
  a `name:*` column per language that is a real difference, and it has not been benchmarked yet.
- **No FeatureData cache on the MLT side.** `MBVTFeatureDecoder` dedups identical attribute sets by
  their tag-index vector; MLT properties are columnar and carry no equivalent key, so each feature
  gets its own `FeatureData`. The geometry cache (keyed by feature index) is kept for both.
- **Buffer features are clipped identically.** MLT `place` features in the OMT fixtures reach
  ±8000 on a 4096 extent; both decoders drop what falls outside the clip box, so the same features
  survive.
- Pre-tessellated triangles (`mlt::geometry::Geometry::getTriangles`) are **ignored** — vt still
  tessellates with tess2. Wiring them through would skip that work and is the obvious next win.

The decoder library itself is vendored decoder-only; what it costs in bytes is in
[../build-size.md](../build-size.md).

## GeoJSON tiles: the on-demand pyramid

`GeoJSONVectorTileDataSource` cuts MVT out of an in-memory GeoJSON layer through
`mbvtbuilder::MBVTTileBuilder`. It used to do this the direct way, and both halves scaled badly:

- `encodeLayer` walked **every feature of the layer for every tile**, keeping only a bounding-box
  test — so a request cost O(features) no matter how little of the layer the tile held;
- geometry was **re-simplified and deep-copied per zoom level** into `_cachedZoomLayers`, leaving one
  full copy of the dataset (geometry *and* every feature's `picojson` properties) resident per zoom
  the camera had visited.

It now uses mapbox's **geojson-vt** (three submodules under `libs-external/geojsonvt`), the same
library tangram-es uses for its `ClientDataSource`. Two of its ideas do the work:

- **Simplification importance is computed once at import.** `detail::convert` runs Douglas-Peucker
  against the tolerance of the deepest zoom and stores each vertex's importance on the point; a tile
  then only filters `p.z > sq_tolerance`. The per-zoom copy is gone entirely.
- **Tiles are cut from the slice their parent already made**, axis-separated (one x pass feeds both
  children), so a feature is walked once per *level* instead of once per tile.

Properties never enter the index: features carry a slot index and the `picojson` values stay in
`MBVTLayerData::infos`, so slicing never touches one.

### What is ours and not geojson-vt's

The pyramid is driven by our own drill rather than `geojsonvt::GeoJSONVT`, because four things about
that class cost measurable time in an on-demand SDK (it is written for a batch tiler):

| | geojson-vt | here | why |
|---|---|---|---|
| root | always z0 | deepest tile containing the layer | a city-sized layer paid ~9 levels of whole-dataset copies before the first real cut (209 ms, device) |
| int16 tile | built for every node walked | only for the requested tile | internal nodes are never drawn |
| built tiles | cached forever | not cached | the SDK caches the encoded MVT above us |
| stop condition | `indexMaxPoints` (100k) | nothing in the node is bigger than the **target** tile | see below |

**The stop condition is the load-bearing one.** Splitting costs a pass over the whole node per level
and only helps features that must be *cut*; a feature smaller than the tile is thrown out by the
clipper's per-feature bbox test for two comparisons either way. Splitting a layer of small scattered
features therefore just copies them down the tree: 5000 short routes cost **583 ms** over 256 tiles
at z14 where the old scan-every-feature builder took **209 ms**. The test is against the **target**
zoom, not the next level down — a piece that fits one child can still span dozens of tiles at the
zoom actually wanted, and stopping on that cost the long-route set 575 ms instead of 274 ms.

One more trap, because it inverted a result twice: `detail::clip` does
`clipped.reserve(features.size())`. Running it straight over a coarse node allocates for the entire
layer on **every** tile, which made serving from a coarse node cost more than the full scan it
replaced. Features touching the tile are picked by bbox first, and only those go to the clipper.

A builder pinned to one zoom (`minZoom == maxZoom`, which is how `ContourTileDataSource` uses it)
skips the index and cuts its single tile directly — geojson-vt's own `geoJSONToTile`, minus the
variant round-trip. There is nothing for an index to amortise over one tile.

### Measured (Crosscall HLTE556N, Adreno 610, `--es geojsonBench`)

640 tiles over z8–z17, four per side per zoom:

| dataset | before | after |
|---|---|---|
| 5000 short routes, 165k points | 1319 ms | **1176 ms** |
| 8 routes of 100–250 km, 303k points | 1709 ms | **513 ms** |

256 tiles at z14 alone (what panning at one zoom does):

| dataset | before | after |
|---|---|---|
| 5000 short routes | **209 ms** | 328 ms |
| 8 long routes | 362 ms | **272 ms** |

So: long lines win everywhere (up to 3.3x), and the many-small-features case is better across zooms
(where the old builder re-simplified per zoom) but **still 1.6x slower at a single zoom**, because
the old path clipped geometry already simplified for that zoom while this one clips at full
resolution and filters by importance afterwards. That gap is open.

### Binary size

geojson-vt is header-only, and the code it replaces went away, so the cost is small but not zero —
`libcarto_mobile_sdk.so`, arm64-v8a, debug build, stripped:

| | bytes |
|---|---|
| before | 16 281 408 |
| after | 16 385 664 |
| | **+104 256 (+0.64%)** |

That is template instantiation: the clipper, `InternalTile` and the geometry variant are stamped out
per geometry type. No new runtime dependency ships — nothing is linked, only included.

(The demo APK also grows ~10 MB from the two bench GeoJSON assets. That is the test bench, not the
SDK; nothing in `all/native` reads them.)

### Render cost is a different question, and simplification owns it

Loading the 5000-route set as a real layer over 3D terrain drops the device to **4 fps**. That is
**not** the tiler: the old builder measures the same (4.1 fps, 24.8M `geomIndices` per interval,
against the pyramid's 4.2 fps / 24.4M). It is the demo asking for `simplifyTolerance = 0`.

At tolerance 0 nothing is ever dropped, at any zoom, so all 165k source points reach the line
tesselator — twice, because the route style draws a casing under the fill with round joins — and
then get subdivided again at every terrain-lattice crossing. Panning at z13 / tilt 35, `--es
geojsonLayer many`, ten swipes:

| | fps | geomIndices / interval |
|---|---|---|
| no layer | 20 | 5.1M |
| layer, tolerance 0 | **4.2** | 24.4M |
| layer, tolerance 1 (SDK default) | 14.9 | 8.9M |
| layer, tolerance 2 | 15.7 | 7.7M |

The GL thread is not the limit at 4 fps — frames average 42–46 ms but only five are issued per
second, so most of the wall clock goes to the tile threads tesselating that geometry, and
`renderTiles` collapses from 714 to 212 because tiles cannot be built fast enough to keep the set
full.

`MBVTTileBuilder` already defaults `_simplifyTolerance` to 1.0; the route test sets 0 deliberately
(vertex-dense input is what its join cases need), and the bench layer used to inherit that. It has
its own `--es geojsonBenchSimplify` now, defaulting to the SDK's 1.0. **An app that leaves the
default alone does not hit this.** The tile-build table above was taken at tolerance 0.

The bench is `DemoTests.runGeoJSONBench` — it times `loadTile` directly, no renderer in the way, over
a tile set derived from the data extent so two builds are comparable. `--es geojsonLayer many|long`
adds the same datasets as a real styled layer instead, for render-side comparison.
