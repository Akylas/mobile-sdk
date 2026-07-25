# Sky, sun and dynamic shadows

Plan of record for the `feat/sky-sun-shadows` branch: a shader-driven customizable sky, a
first-class sun/light, dynamic shadows over terrain and buildings — and the terrain draping
rewrite that shadows depend on.

## Why draping comes into it

The requested features are not independent of the terrain draping model. Shadows need a
**single lit surface** whose fragment shader knows the world position, the DEM normal and a
shadow-map lookup. Today draped content is dozens of independent style shaders drawing
displaced 3D geometry that shares a depth buffer with the terrain surface, so:

- lighting would have to be re-implemented in every style shader, and
- content sits at marginally different depths from the surface it is supposed to be painted
  on, which is shadow-acne territory before a single shadow is cast.

Draping fixes this by construction: the content *is* the surface's texture, so there is one
shader to light and one depth-buffer occupant.

## The reported defects (what we are actually fixing)

Reported on master, at low tilt while **zooming** — animation artifacts, not static ones:

1. Draped content does not animate with the zoom — it is baked once per target tile and the
   texture is not re-baked, so it lags/pops against the geometry that does animate.
2. Artifacts appear and disappear during the animation — the tile set changes and each tile
   independently flips between "draped" (texture, true depth) and "not draped" (displaced
   geometry, depth-biased). The two paths disagree, so the transition is visible.
3. Contour lines break up — sharp displaced line geometry loses the depth test against the
   terrain surface in patches.

All three have the same root: **only part of the content is draped, and which part changes
per tile and per frame.** Master drapes native (non-overzoomed) fills only; lines optionally;
rasters/hillshade never. Every boundary between the two models is a visible seam.

maplibre has no such boundary — every drapeable layer type is draped, always, and nothing
draped ever touches the depth buffer.

## Reference: what maplibre-gl-js does (verified against `main`, clone at ../maplibre-gl-js)

`maplibre-native` has **no 3D terrain and no sky**. gl-js is the only reference.

| Mechanism | Where | Detail |
|---|---|---|
| Drapeable set | `webgl/render_to_texture.ts` `LAYERS_TO_TEXTURES` | `background`, `fill`, `line`, `raster`, `hillshade`, `color-relief`. Everything else (symbol, circle, heatmap, fill-extrusion, custom) renders live in 3D. |
| Drape render | same | orthographic tile-local matrix, terrain uniforms suppressed — a flat 2D render of one tile square |
| Stacks | same | contiguous runs of drapeable layers share one texture; a non-drapeable layer between them starts a new stack (another texture + another mesh draw) |
| RTT size | `rttSize = tileManager.tileSize * terrain.qualityFactor` | 1024 × 2 = **2048** |
| Terrain mesh tiles | `tile/terrain_tile_manager.ts` | `minzoom 0`, **`maxzoom 22`** — *not* the DEM's maxzoom. `tileSize = source.tileSize * 2**deltaZoom` with `deltaZoom = 1`. |
| DEM for a mesh tile | same, `getSourceTile` | walks *up* from `overscaledZ - deltaZoom` until a tile has DEM; the child is mapped into a region of the ancestor by `u_terrain_matrix` |
| Mesh | `render/terrain.ts` | one shared static 128×128 grid reused by every tile, **with skirts** to hide cross-zoom hairlines. Drape uv = `a_pos3d.xy / EXTENT`. |
| Depth | `draw_terrain.ts` | mesh draws `LEQUAL`, read+write. Draped layers: `DepthMode.disabled`. |
| Labels | | lift by elevation, fade against a packed-RGBA screen-space depth texture rendered from the mesh; never touch the depth buffer |
| Sky | `shaders/glsl/sky.fragment.glsl` | full-screen quad, **screen-space** horizon line + two-colour gradient. No sun, no rays. |
| Atmosphere | `shaders/glsl/atmosphere.fragment.glsl` | full-screen quad, per-pixel view ray from `u_inv_proj_matrix`, Rayleigh+Mie raymarch (`glsl-atmosphere`), `u_sun_pos`. Globe mode only. |
| Shadows | — | **none.** maplibre has no shadow support at all. |

So: for draping, maplibre is the model to follow. For sky, maplibre's is a starting point but
weaker than what is being asked for (no sun, no customization). For shadows there is no prior
art to port — it is new work.

## What CARTO already has

- `feat/terrain-rtt-draping` — an unmerged branch that already implements the maplibre shape:
  `TerrainDrapeCache` above the layers, `GLTileRenderer` as a content baker
  (`collectDrapeTiles` / `bakeDrapeTile` / `renderDrapedSurface`), full drapeable set, texture
  pool, fingerprint re-bake, `TerrainOptions.setDrapeResolution`. Blocked on one open bug
  (some tiles sample their drape texture as black) and it does **not** decouple mesh tiles
  from the DEM zoom.
- `TerrainOptions.setRegularGridEnabled` — the shared static grid, without skirts.
- `vt::NormalMapBuilder` — DEM → normal map, used by the hillshade layer. The input a dynamic
  sun needs.
- `Options.setSkyBitmap` / `setSkyColor` + `BackgroundRenderer::drawSky` — a gradient bitmap
  band drawn as world geometry. To be superseded (kept working) by the shader sky.

## Stages

Ordered so that each stage is independently visible and testable in `scripts/android-dev`.

### S1 — Shader sky + sun (no dependency on draping)
New `SkyOptions` (enabled, sky/horizon colour, custom fragment shader source) and
`LightOptions` (sun azimuth/altitude or date+location, colour, intensity, ambient).
New `SkyRenderer`: one full-screen quad drawn before everything, depth write off, with a
per-pixel world-space view ray. Default shader = horizon gradient + sun disc + glow;
`SkyOptions.setShaderSource` replaces the body with user GLSL (clouds etc.) against a
documented uniform contract.

### S2 — Terrain draping rewrite (maplibre model)
Rebase `feat/terrain-rtt-draping`, close the black-texture bug, then add what it lacks:
mesh tiles decoupled from DEM zoom (ancestor DEM + terrain matrix), skirts on the shared
grid, quality-factor resolution, re-bake on zoom so draped content animates. Retire the
depth-bias machinery for everything that is draped.

### S3 — Sun lighting on the terrain surface
Light the drape in the terrain fragment shader from the DEM normal and the live sun —
dynamic hillshade that replaces the pre-baked hillshade raster layer for the terrain case.

### S4 — Shadows
Directional shadow map from the sun over the terrain mesh and the 3D building geometry,
sampled in the terrain surface shader and the `Polygon3D` shader. Cascades / fit-to-frustum
as needed. Buildings both cast and receive.

## Notes

- The demo app (`scripts/android-dev`) is the verification loop; ~35 s incremental build,
  `adb shell screenrecord` for the animation defects (screenshots do not show them).
- SWIG proxies are regenerated manually, they are not part of the gradle build:
  `cd scripts && python3 swigpp-java.py --profile "standard+valhalla+geocoding+routing+packagemanager" --swig /Volumes/dev/carto/mobile-swig/swig`
