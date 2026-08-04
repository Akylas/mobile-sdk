# Us vs tangram-ng: what differs, and why

Scope: a single place to answer "why don't we just do what tangram does?". Everything here was read
in their source, not assumed. Their paths are relative to `/Volumes/dev/carto/tangram-ng`.

The standing rule is in [README.md](README.md#two-rules-that-shaped-everything-here): **where they do
something differently, we adopt their way.** This page therefore has two kinds of entries — *ported*
and *still different*, the latter with the reason it is not simply copied.

## Side by side

| | tangram-ng | this fork | state |
|---|---|---|---|
| terrain surface | ONE shared static 64-grid VBO for every tile, per-tile uniforms (`core/src/style/rasterStyle.cpp:61`) | same shared grid (`buildCompiledTerrainGridSurfaces`), resolution = `MeshResolution` | **ported** |
| content on terrain | displaced per vertex, one `texture2D` fetch (`res/scenes/terrain-3d.yaml`) | same | **ported** |
| content depth | `gl_Position.z += (proxy − layer)·(2⁻¹⁹·w + depth_shift)`, `depth_shift` a flat 0.02, `proxy *= 48` for the raster | same, with the shift derived from the stack's ordinal span so the *budget* matches | **ported** ([05](05-depth-model.md)) |
| near plane | `m_pos.z / 50` (`core/src/view/view.cpp:452`) | same in terrain mode | **ported** |
| per-layer depth pre-pass, stencil tile masks | none anywhere in `core/src` | none (shared ground) | **ported** |
| map background | the framebuffer clear colour (`core/src/map.cpp`) | global terrain base fill before all layers; no per-tile background meshes | **ported** |
| contour labels | generated from the elevation texture, no contour geometry (`core/src/style/contourTextStyle.cpp`) | label stubs in `ContourTileDataSource`, same algorithm | **ported** ([07](07-hillshade-contours.md)) |
| hillshade / contours / hypsometric | fragment blocks on the terrain raster draw (`res/scenes/hillshade.yaml`) | hillshade and contours are a paint/shader block; hypsometric is still its own layer | **partly** |
| content subdivision | none at all | area fills to two surface cells; lines cut at the lattice | **different — see below** |
| elevation texture | source raster bound directly, ancestors via uv sub-rects, edges extrapolated in-shader (`res/scenes/elevation.yaml`) | per-tile CPU re-encode with a 1-texel border from up to 8 neighbours | **different — see below** |
| tile LOD | subdivide while screen area > `(2·pixelScale·256)²` (`core/src/tile/tileManager.cpp:214`) | distance rule, ~one zoom level finer | **different — measured not to matter** |
| tile decode threads | 2 (`SceneOptions::numTileWorkers`) | 1 (`Options::setTileThreadPoolSize`) | **different — measured not to matter** |
| terrain depth read-back | worker thread, shared context, half res, never waited on | worker thread, **unshared** context, submit-interval limited | ported with a difference |
| terrain shadows | none | cascaded shadow maps (currently off on the shared ground) | **we are ahead** ([08](08-lighting-sky-fog.md)) |
| style system | YAML scenes, one global ordered style list | CartoCSS + a composite layer that buys back a single ordered list ([09](09-composite-layer.md)) | structural |

## The differences that are deliberate

### Area fills are still subdivided

Tangram does not subdivide anything, and for lines neither do we (they are cut exactly at the
surface lattice, which is cheaper *and* exact). Fills are the one place their model cannot be copied
verbatim: **their terrain base map is a raster inside the ground draw**, so they have no large flat
polygons draped over relief to begin with. Ours does — a landcover polygon can span a whole valley —
and an un-subdivided one chords far enough below the displaced surface that no affordable
`depth_shift` covers it.

Two surface cells is the measured compromise (20.6 fps against 16.6 for one cell, with the artifacts
that source density shows still absent). See [02-tiles.md](02-tiles.md#geometry-density-what-gets-subdivided-and-why).

### The elevation texture is re-encoded

Their scheme is cheaper: upload the tile's own raster once, address ancestors through uv offsets,
extrapolate edges in the shader. Ours re-encodes a padded texture per tile with borders taken from up
to eight neighbour grids, including a **cross-level box filter** along shared edges.

That border machinery is a seam feature they do not have: it is what makes DEM tiles from different
zoom levels meet without a visible ridge. The port that keeps it is to upload the grid's own samples
and patch the borders as small `glTexSubImage2D` strips — not to drop the feature.

### Draped fills (the old path) are being removed, not maintained

Not documented here on purpose; see [README.md](README.md#two-rules-that-shaped-everything-here).

## Measuring against them

`PROF` is ours only and is **not comparable** to anything they report — it read 20–27 fps for a
config a cross-app instrument put at 11–13. Use SurfaceFlinger for both:

```sh
adb shell dumpsys SurfaceFlinger --timestats -disable ; --timestats -clear ; --timestats -enable
# drive the motion, then
adb shell dumpsys SurfaceFlinger --timestats -dump --maxlayers 8
```

Read `averageFPS` of the `SurfaceView[<pkg>/...](BLAST)` layer. Two traps: our app also reports an
activity-window layer that reads ~30–50 fps and means nothing, and `dumpsys gfxinfo` counts only the
UI layer because the map renders on its own thread.

Their demo APK is prebuilt at `platforms/android/demo/build/outputs/apk/release/demo-release.apk`.
**Tap the "3D" chip after every launch** — it sets `global.terrain_3d` *and* tilts to 1.0 rad, and it
does not survive a restart. 1.0 rad ≈ **tilt 33** in our convention.

**The last published head-to-head is not valid.** It compared their release APK against ours built at
`-O0` ([10-performance.md](10-performance.md#build)). Re-run it before quoting any gap.
</content>
