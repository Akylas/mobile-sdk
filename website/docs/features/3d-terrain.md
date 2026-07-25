---
title: 3D Terrain
sidebar_position: 1
---

# 3D Terrain

Render the map draped over **real elevation**, with correct depth occlusion (near ridges hide
far slopes), fill draping and fast zooming.

:::info Fork feature
3D terrain is an addition of the Akylas fork. It is configured through
`TerrainOptions`, attached to the map via `Options.setTerrainOptions()`.
See PR [#21](https://github.com/Akylas/mobile-sdk/pull/21) and the design notes in
[`docs/terrain-3d-draping.md`](https://github.com/Akylas/mobile-sdk/blob/master/docs/terrain-3d-draping.md).
:::

<figure class="docs-figure">

![3D terrain over Saint-Eynard](/img/features/terrain-hero.svg)

<figcaption>3D terrain with draped basemap fills and hillshade. <em>(replace with a real capture — see <a href="/docs/contributing-docs#screenshots">capturing screenshots</a>)</em></figcaption>

</figure>

## How it works

Terrain consumes **RGB-encoded elevation tiles** (MapBox or Terrarium encoding) from any
`TileDataSource`. The renderer builds a per-tile surface mesh, displaces it by the decoded
elevation, and draws the map on top of it.

- **Fill draping** — polygon fills and the style background are baked *flat* into a per-tile
  offscreen texture (MapLibre-style render-to-texture) and used as the terrain surface's texture.
  Fills therefore follow the terrain exactly: zero holes, zero see-through, no depth slack. The
  bake is cached per tile, so steady-state panning does no offscreen work.
- **Depth occlusion** — the draped surface writes true depth and *is* the occluder, so a near
  ridge correctly blocks the far slope's raster, contours and route lines (shared depth buffer,
  painter-order model).
- **Sharp geometry** (contour and tile lines) is displaced and lattice-clamped to the surface,
  drawn `GL_LEQUAL` with zero depth bias so it hugs the terrain without leaking through ridges.

## Quick start

```kotlin
import com.carto.components.TerrainOptions
import com.carto.datasources.HTTPTileDataSource
import com.carto.datasources.MemoryCacheTileDataSource

// 1. An RGB-elevation source (Terrarium or MapBox encoding).
val demSource = MemoryCacheTileDataSource(
    HTTPTileDataSource(0, 12, "https://your.tiles/dem/{z}/{x}/{y}.png").apply {
        // the "encoding" metadata selects the decoder: "terrarium" or "mapbox"
        setMetaData("encoding", "terrarium")
    }
)

// 2. Build terrain options. The decoder is resolved from the "encoding" metadata.
val terrain = TerrainOptions(demSource).apply {
    isEnabled = true
    painterOrderDepthEnabled = true   // recommended depth model
    drapeFillsEnabled = true          // render-to-texture fill draping
    meshResolution = 64               // grid cells per tile edge (2..256)
    exaggeration = 1.0f               // 1.0 = true-to-scale
}

// 3. Attach to the map.
mapView.options.terrainOptions = terrain
```

```swift
let dem = NTMemoryCacheTileDataSource(dataSource: httpDem)
let terrain = NTTerrainOptions(dataSource: dem)
terrain?.setEnabled(true)
terrain?.setPainterOrderDepthEnabled(true)
terrain?.setDrapeFillsEnabled(true)
terrain?.setMeshResolution(64)
mapView.getOptions()?.setTerrainOptions(terrain)
```

:::tip Share the DEM with hillshade
`TerrainOptions` can share its elevation `TileDataSource` with a
[`HillshadeRasterTileLayer`](/docs/features/hillshade). Wrap the source in a
`MemoryCacheTileDataSource` so both features hit the same tiles instead of downloading twice.
:::

## `TerrainOptions` reference

| Property | Default | Notes |
|---|---|---|
| `Enabled` | `true` | When off, the map renders flat but the DEM stays attached. |
| `Exaggeration` | `1.0` | Height multiplier. Changing it re-tesselates loaded tiles (costly). |
| `MeshResolution` | `32` | Grid cells per tile edge, clamped `2..256`. Limited by DEM resolution. |
| `PainterOrderDepthEnabled` | `false` | Recommended depth model; content draws in painter order over a true-depth surface. |
| `DrapeFillsEnabled` | `false` | Render-to-texture fill/background draping. |
| `DrapeLinesEnabled` | `false` | Optionally drape tile lines too (softer, zero-cost hug). |
| `ElementTerrainSlack` | — | Painter-order clearance for vector elements (routes). Tune if route lines leak. |
| `RegularGridEnabled` | `false` | Shared regular grid (tangram-style, faster) vs adaptive tesselation (crack-free). GPU-draping/planar only. |
| `BackgroundColor` | — | Fill color drawn before tiles (works even with zero tile layers). |
| `BackgroundBitmapEnabled` | `false` | Drape `Options.getBackgroundBitmap()` over the terrain (world-anchored, repeats). |
| `MinZoom` / `CameraClearance` / `CameraClampDuration` | — | Camera behavior near/inside slopes. |
| `ElevationCacheCapacity` | — | LRU capacity for the elevation-texture cache. |

Recommended configuration:

```java
terrainOptions.setPainterOrderDepthEnabled(true);
terrainOptions.setDrapeFillsEnabled(true);
terrainOptions.setMeshResolution(64);
```

## Querying elevation

`TerrainOptions` (and the shared `ElevationManager`) can return heights for map positions:

```kotlin
val metres: Double = terrain.getElevation(mapPos)          // single
val many: DoubleVector = terrain.getElevations(mapPosVector) // batched
```

## Performance notes

- **Draped content skips terrain subdivision** (it is baked flat), so vertex buffers upload at
  source density instead of ~`meshResolution²` per tile. This, plus an LRU elevation-texture cache
  (no full flush), removes most of the fast-zoom render-thread stall.
- Prefer `meshResolution = 64` as a good quality/cost balance; go higher only if you see terraced
  slopes at close range.
- `RegularGridEnabled = true` removes per-tile CPU tesselation (fastest) but can show thin cracks
  where adjacent tiles differ in zoom level.

## Known limitation

At low zoom, `VectorLayer` **element** lines (e.g. long routes) can still leak through a ridge:
they are CPU-baked to a fine bilinear surface while the low-zoom occluder is coarse, and no depth
bias wins that case. The fix (render-to-texture element draping across layers) is on the roadmap.

## See also

- [Hillshade](/docs/features/hillshade) — shaded relief from the same DEM.
- [On-the-fly Contours](/docs/features/contours) — contour lines that drape over terrain.
- [Composite Vector Tile Layer](/docs/features/composite-vector-tile-layer) — mix terrain-aware sources into one style.
