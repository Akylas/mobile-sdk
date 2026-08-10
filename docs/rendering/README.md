# How the renderer works

Technical documentation of the CARTO Mobile SDK (Akylas fork) rendering path: what it does, how it
is implemented, how it compares to [tangram-ng](https://github.com/farfromrefug/tangram-ng), and why
it differs where it does.

It is split so that a reader — human or AI — can open **one** file and get a whole subsystem without
loading the rest. Every page states its scope at the top and links out rather than repeating.

## Read this when…

| You are working on | Read |
|---|---|
| anything at all, first time | this page, then [01-frame.md](01-frame.md) |
| frame order, threads, what runs where, redraw requests | [01-frame.md](01-frame.md) |
| which tiles are chosen, LOD, fetching, decoding, caches | [02-tiles.md](02-tiles.md) |
| the GL draw path, style layers, draw counts, shaders | [03-vt-renderer.md](03-vt-renderer.md) |
| 3D terrain: elevation data, surfaces, the ground pass | [04-terrain.md](04-terrain.md) |
| z-fighting, see-through, content sinking into the ground | [05-depth-model.md](05-depth-model.md) |
| labels: placement, flicker, anchoring onto terrain | [06-labels.md](06-labels.md) |
| hillshade, contour lines, contour labels, hypsometric tint | [07-hillshade-contours.md](07-hillshade-contours.md) |
| sun, shadows, sky, fog | [08-lighting-sky-fog.md](08-lighting-sky-fog.md) |
| CompositeVectorTileLayer, style-driven slots | [09-composite-layer.md](09-composite-layer.md) |
| markers, popups, app-drawn lines/polygons, picking | [12-vector-elements.md](12-vector-elements.md) |
| navigation maneuver arrows on a route | [15-maneuver-arrows.md](15-maneuver-arrows.md) |
| sun/moon/stars/aircraft: objects placed in the sky | [13-celestial.md](13-celestial.md) |
| full-screen effects, the relief look, layers drawn above them | [14-post-process.md](14-post-process.md) |
| making it faster, or measuring anything | [10-performance.md](10-performance.md) |
| "why don't we just do what tangram does?" | [11-tangram-diff.md](11-tangram-diff.md) |

Working notes with the measurement history live in [../render-performance.md](../render-performance.md).
That file is a lab notebook (dated rounds, dead ends, numbers); these files are the current design.

## Two rules that shaped everything here

**1. tangram-ng is the reference implementation, and we copy it.**
It renders the same data, on the same devices, sharply and with no see-through. Where it does
something differently, we adopt its way rather than designing an alternative. Its constants are
copied, not derived — every constant this project derived instead turned out wrong (see the failure
catalogue in [05-depth-model.md](05-depth-model.md#four-ways-to-get-this-wrong-all-of-them-tried)).
And each mechanism is ported **whole**: half of their depth model was measurably worse than none of
it, three times over.

When comparing, read their **scene files** (`res/scenes/*.yaml`), not only their shaders — the
shader defaults are overridden there.

**2. The RTT drape is being deleted and is deliberately not documented.**
The old path baked flat 2D content into per-tile render-target textures and textured the terrain
with them. It still exists behind `TerrainOptions.DrapeFillsEnabled` and in `--es drape true` in the
demo, but it is on its way out; the shared ground ([04-terrain.md](04-terrain.md)) replaced it.
Do not build on it, and do not extend these docs to cover it.

## Where the code lives

| Path | What |
|---|---|
| `all/native/renderers/` | frame orchestration (`MapRenderer`), per-kind renderers, `TileRenderer` (the bridge into `vt`) |
| `all/native/layers/` | layer types: `TileLayer`, `VectorTileLayer`, `RasterTileLayer`, `HillshadeRasterTileLayer`, `CompositeVectorTileLayer` |
| `all/native/terrain/` | `ElevationManager`, `ElevationTileGrid`, `TerrainTileTransformer` |
| `all/native/datasources/` | tile sources, including the on-the-fly `ContourTileDataSource` |
| `libs-carto/vt/` (submodule) | the GL vector-tile renderer: `GLTileRenderer`, `TileSurfaceBuilder`, `Label`, `LabelCuller`, shaders |
| `libs-carto/mapnikvt`, `cartocss` | tile decoding and style evaluation |
| `all/modules/*.i` | the SWIG public API surface, mirroring `all/native` |

A change under `libs-carto/` is a commit in that submodule plus a pointer bump here — see the
working agreement in `.claude/CLAUDE.md`.

## What this set does not cover yet

Stated so a reader does not mistake silence for "there is nothing there":

- **Startup / first frame** — ~3.8 s to first content with a warm cache, of which ~1.3 s is before
  the first tile is even requested (JVM attach, GL init, ~0.6 s enumerating system fonts). Measured,
  not yet attributed; the numbers are in [../render-performance.md](../render-performance.md).
- **Spherical projection mode** (`RenderProjectionMode::SPHERICAL`) — exercises the non-trivial
  `TileTransformer` paths; none of the terrain work above applies to it.
- **Post-process effects** (`PostProcessEffect`, the offscreen path in `onDrawFrame`).
- **Platform specifics** — iOS/UWP GL context setup, and the `angle-metal` backend.
- **The RTT drape**, on purpose (see above).
</content>
