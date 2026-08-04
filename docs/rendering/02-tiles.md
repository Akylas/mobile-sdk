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
bool subDivide = screenArea >= _lodMaxTileArea;   // (2 * tileSizePixels)^2, x 4^-zoomLevelBias
```

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
  and DEM fetching) churns forever.
- **Target tiles may exceed the data source's max zoom** in terrain mode
  (`_terrainOverzoomTargets`, fed by the existing overzoom machinery). A z12 DEM-derived hillshade
  under a z15 camera otherwise renders surfaces many times coarser than the base map's, and those
  blunted ridges are leaky depth occluders - content and vector elements show through near crests.
- **The view distance stops the recursion**, tested against the nearest point of the tile. It is
  tangram's too (`ViewState::calculateViewDistance`): `2 * cameraHeight / cos(pitch + fovy/2)`,
  capped at 127 tile widths (their `MAX_LOD` 6), scaled by `TerrainOptions::ViewDistanceFactor`
  (1 = their rule verbatim, 0 = as far as the visible ground goes). A style may pin an absolute
  distance in metres with `terrain-max-visible-distance`. Pair a short one with fog
  ([08-lighting-sky-fog.md](08-lighting-sky-fog.md)) or the ground simply ends.

## Substitution, preloading, caching

- `TileSubstitutionPolicy` decides whether a missing tile is stood in for by a parent/child.
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
surface triangle. Exact, fewer vertices, and no depth slack needed.

## Where tiles come from

Standard sources (HTTP, MBTiles, PMTiles, assets) plus two of interest here:

- **`ContourTileDataSource`** — generates contour MVT from a DEM source on the fly, sharing the DEM
  with the hillshade so terrain tiles are fetched once. It has a label-stub mode that makes it emit
  only what labels need; see [07-hillshade-contours.md](07-hillshade-contours.md).
- **elevation sources** — decoded by `MapBoxElevationDataDecoder` / `TerrariumElevationDataDecoder`
  into `ElevationTileGrid`s held by `ElevationManager` ([04-terrain.md](04-terrain.md)).
</content>
