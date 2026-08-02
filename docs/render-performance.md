# 3D terrain render performance — measurements, design differences, next steps

Working document for the `perf/terrain-render` branch. It records what was **measured** (with the
method, so the numbers can be reproduced or refuted), how our renderer differs from
[tangram-ng](https://github.com/farfromrefug/tangram-ng) — the reference implementation we compare
against, because it renders the same data sharply, smoothly and with no see-through — and what is
worth doing next.

It is also meant to grow into the "how it is implemented and why" documentation: every section
states the *choice* behind the code, not only the number. When a design decision is made or
reversed, record it here with the evidence.

---

## 0. Next session: start here

Before anything, reproduce the slow case — every ranking below depends on it:

```sh
cd scripts/android-dev && ./gradlew :app:assembleDebug -x lint -PprofileRender
adb install -r -t app/build/outputs/apk/debug/app-debug.apk
cd bench && ANDROID_SERIAL=<serial> sh north.sh baseline
```
Expect ~6.8 fps, ~131 ms/frame, ~130 render tiles/frame. If it is not that, the camera or the layer
set is wrong — fix that before optimising anything.

**Move 1 — hillshade AND contours as shader blocks on the terrain draw (§4).** The one with real
leverage, and now also the fix for a correctness problem: the hillshade layer alone multiplies render
tiles ~5× (132 → 693 per interval), and contours drawn as geometry are what see through ridges when
any depth bias is applied (§3.1). **Hillshade landed — see §9**; contours and the hypsometric tint
are the remaining kinds, then the contour labels (§9.4).
*Acceptance:* render tiles per frame in the north pan drop toward the base-only count (~40) with the
hillshade still in its style position, and `bench/north.sh` improves materially. Read
`res/scenes/hillshade.yaml` and `core/src/style/style.cpp` (`m_rasterType`, `TANGRAM_NUM_RASTER_SOURCES`)
before designing it.

**Move 2 — screen-area LOD as an option (§7.2).** Mechanical, already quantified (+11%):
`--es maxTileZoomOffset -1` emulates it today. Port tangram's rule into
`TileLayer::calculateVisibleTilesRecursive` behind an option defaulting to current behaviour.

**Move 3 — the unexplained blocking (§6).** ~23 ms per frame sits inside GL calls that no counter
explains, and it does not move with pixels, vertices, draws or bakes. If it turns out to be one
systemic stall it could outweigh moves 1 and 2. Needs per-pass GPU timer queries *inside* the layer
pass (the section machinery exists in `FrameProfiler.h`; Adreno returns `0xFFFFFFFF` for sections it
cannot time, so accumulate per section and drop those).

Do **not** re-run the dead ends in §6 — nine of them are already measured.

**Decided already:** the tangram content model is rejected as a default — Martin saw contour lines
from the far side of ridges (§3.1). The switches stay for experiments; the lattice split (§2.2) is
the default. Still Martin's call: whether the coarser LOD's label density is acceptable (Move 2).

---

## 1. How to measure (do this before believing anything)

Build the demo with the profilers on and read `PROF` / `PROF GPU` / `RenderStats` from logcat:

```sh
cd scripts/android-dev && ./gradlew :app:assembleDebug -x lint -PprofileRender
```

- `PROF` — CPU ms per frame section: `sky prelude prepare cover drape layers layers3D billboards`.
  `sky` is mostly the swap-buffer wait, not work.
- `PROF GPU` — the same sections timed on the GPU (`GL_EXT_disjoint_timer_query`, Android only).
  Switch off with `adb shell setprop debug.carto.gputimer 0`.
- `RenderStats` — draw counts, index counts, per-draw µs, surface/label/drape counters.

**Device numbers drift.** On the Crosscall (Adreno 610) the *same build* measured 14.6-17.4 fps
across a morning. Only **interleaved** A/B is trustworthy: build two APKs, alternate them, take
medians over ≥40 one-second windows. Helper scripts used for the numbers below:

They live in [`scripts/android-dev/bench/`](../scripts/android-dev/bench/README.md) — `ab.sh`,
`ab2.sh` (mountain camera), `north.sh` (the slow case), `abapk.sh` / `abprop.sh` (interleaved A/B by
APK or by system property), `startup2.sh`, `absum.py` (median summary, discards idle windows).

**The camera decides what you measure.** The demo default is Grenoble **city, z16.22, tilt 26** —
content-heavy. Panning *east* crosses the flat valley and is cheap; panning *north* climbs into the
mountains and is the case that gets slow (`--es animLatDelta 0.06`). Tilt matters just as much: at
tilt 85 most of the screen is sky.

| camera / motion | fps | frame | render tiles / frame |
|---|---|---|---|
| east over the valley, light config, mesh 64 | ~20 | 50 ms | 21 |
| mountain camera (45.244172/5.760595 z13.2 t55) | 26 | 32 ms | — |
| tilt 0 vs tilt 85, same camera | 51 ms vs 25 ms | | 21 vs 6 |
| **north into the mountains, contours + hillshade** | **6.8** | **131 ms** | **~130** |

---

## 2. What landed, with the measured effect

All on `perf/terrain-render` (main repo) and the matching `libs-carto` branch.

| change | commit | effect |
|---|---|---|
| GPU timer queries (`PROF GPU`) | `ff422a128` | tool |
| Occlusion depth read-back on its own thread + GL context | `d02002dda` | 13.7 → 14.9 fps |
| Demo terrain mesh 128 → 64 (tangram's value) | `8cd585887` | 8.5 → 15.2 fps |
| Draped lines cut at the surface lattice, not halved 3× finer | `0112fad65` | 16.7 → 18.1 fps, output unchanged |
| Tile decode pool knob (`--es tilePool N`) | `142e8095d` | no effect — see §5 |
| Switches to measure the tangram content model | `005998a37` + libs-carto `9c9319a` | 18.0 → 19.3 fps when enabled |
| Scripted pan can move north (`--es animLatDelta`) | `83eabcbcc` | benching tool for the slow case |
| Hillshade shades the terrain DEM instead of a tile set of its own | §9 | render tiles −22%, surface draws −16% (emulator counters; device bench pending) |

Cumulative at the demo camera: **16.7 → 20.3 fps (+22%)** with the optional switches on.

### 2.1 Occlusion depth on a worker thread

`glReadPixels` is a pipeline stall (55-62 ms measured). It now runs on a worker thread with its own
EGL context; the render thread only collects meshes (0.8 ms). The context is deliberately **not
shared** — the depth pass draws CPU meshes from client memory with its own program and FBO, so a job
just holds `shared_ptr`s and nothing crosses contexts.

Two GL contexts still share one GPU, and on this Adreno the per-job context switch is the real cost,
so the submit interval matters: every frame → 13.2 fps, 250 ms → 14.3, **500 ms → 14.9** (against
13.7 synchronous). Uploading the meshes into worker-side VBOs changed nothing (tried, reverted).

### 2.2 Lattice line splitting

Regular-grid mode used to subdivide draped lines to 0.35 of a grid cell so no segment chorded across
a cell's diagonal fold and sank below the surface under the zero-slack painter-order depth test.

Now each segment is cut exactly where it crosses `x = k·cell`, `y = k·cell`, `x + y = k·cell` (tile
uv space — the surface builder emits `y = 1 - v`, so the shader's `fg.x + fg.y = 1` fold reads as
`u + v` here). Every sub-segment then lies inside **one** surface triangle: exact instead of
approximate, with fewer vertices. Device screenshot diff at the ridge camera: 0.09% — unchanged.

**Do not** instead add depth slack for lines in painter-order mode: a forward clip bias there is
what leaks over ridges at range (`GLTileRenderer.cpp:2784`, and the rounds 45-56 history).

---

## 3. Where we differ from tangram-ng

Verified in their source, not assumed.

| | tangram-ng | us |
|---|---|---|
| **Draping** | none — vector geometry is displaced per vertex (`res/scenes/terrain-3d.yaml`: `position.z += TERRAIN_SCALE * getElevation()`, one `texture2D` fetch) | fills baked into per-tile RTT drape textures (1024² default), then the surface is textured |
| **Terrain surface** | ONE shared static 64-grid VBO for every tile (`rasterStyle.cpp:61`, `vec2 GL_SHORT`), per-tile uniforms only | same shared-grid design (`buildCompiledTerrainGridSurfaces`), resolution = `meshResolution` |
| **Content subdivision** | none at all | lines cut at the lattice (§2.2); fills subdivided to one cell |
| **Content vs surface depth** | constant clip-space pull: `gl_Position.z += (proxy - layer) * (2⁻¹⁹·w + depth_shift)`, `depth_shift = -0.02·u_proj[2][3]` | content follows the surface exactly, no bias (painter-order) |
| **Hillshade / contours / hypsometric** | fragment-shader blocks on the *same* terrain raster draw (`res/scenes/hillshade.yaml`) | separate tile layers, each with its own tile set, surface pass and stencil mask |
| **Tile LOD** | subdivide while screen area > `(2·pixelScale·256)²` (`tileManager.cpp:214,231`) — ~920 px edge on this phone | distance rule `zoomDistance < SUBDIVISION_THRESHOLD·√2`, ~256 px tiles → one zoom level finer |
| **Elevation texture** | raster bound directly, ancestor sampled through uv offsets; edges clamped, extrapolated in-shader | per-tile CPU re-encode with a 1-texel border from up to 8 neighbour grids, re-uploaded when any neighbour changes |
| **Terrain depth read-back** | worker thread, shared context, half res, never waited on | same now (§2.1) |
| **Stencil tile masks** | none | one full grid draw per tile per layer |
| **Tile decode threads** | 2 (`SceneOptions::numTileWorkers`) | 1 (`Options::setTileThreadPoolSize`) |

### 3.1 The depth shift, and why theirs is safe

`depth_shift` is a **constant clip-space** offset, so its NDC effect is `0.02/w`: strong near the
camera — where an un-subdivided segment chords furthest below the surface — and vanishing at range.
That is structurally different from a constant-**NDC** bias, whose eye tolerance grows as
distance²/near and which is what produced the see-through in rounds 45-56.

Our shader already has the term (`uLayerDepthOffset * (2⁻¹⁹·w + uDepthShift)`); we feed it 0.
Measurable with `adb shell setprop debug.carto.depthshift 0.02` plus
`debug.carto.linesourcedensity 1` (lines at source density): **18.0 → 19.3 fps, 2.7× fewer content
indices per render tile**, and no visible difference at the ridge camera beyond label placement.

**VERDICT (Martin, on device): rejected as a default — "a bit of see-through, on ridges I see bits of
contour lines from the other side".** Which is the expected worst case: contours lie exactly ON the
surface, so any pull towards the viewer lifts the far side of a crest over the near side.

The important conclusion is *why tangram does not have this problem*, and it is not the depth shift:
**their contours are not geometry.** `res/scenes/hillshade.yaml` computes hillshade, contour lines
and the hypsometric tint in the `color:` block of the terrain raster draw — painted onto the surface,
so they have no depth relationship with it and cannot show through a ridge. Their `depth_shift` only
ever has to separate roads and buildings, which are sparse and far less sensitive than lines lying on
the ground.

So the answer for contours is §4 (compute them in the terrain draw), not a depth bias. Keep our
lattice split (§2.2) as the default for line geometry: it is exact, needs no bias, and is only 7%
slower than the un-subdivided version. A smaller shift (0.005) was never measured — worth a try only
for road-like content, and only once contours no longer depend on it.

---

## 4. The dominant cost in the real config: layers multiply tiles

Render tiles per one-second interval, north pan, same camera:

| configuration | render tiles | surface draws |
|---|---|---|
| base only | 132 | 265 |
| base + hillshade | 693 | 926 |
| base + hillshade + contours | 492 | 618 |

**Adding the hillshade layer multiplies render tiles ~5×.** Each layer brings its own tile set, its
own terrain surface pass and its own stencil mask — in the north pan that reaches ~130 render tiles
and 161 surface draws per frame, 3.9M surface indices. Tangram computes hillshade and contours in
the fragment shader of the terrain raster draw it is already doing: zero extra tiles, zero extra
draws.

This is the biggest structural gap for the configuration that actually feels slow.

**Does that break the composite layer, where hillshade can sit at any style level?** Not
necessarily — tangram's mechanism is not "hillshade is fixed at the bottom". Any style can declare
`raster: custom` and call `sampleRaster()` / `getElevationAt()` in its own shader blocks, because
the DEM raster is *bound to the tile draw*. The port that preserves our composite slots is
therefore: bind the shared DEM to the tile draw and let the slot's shader compute shading where the
style says, instead of giving the slot its own tile layer. Ordering is preserved; what disappears is
the duplicate tile set and its geometry.

---

## 5. Startup: 3.8 s to first content

Launch → first drawn tile geometry, warm cache, demo camera:

| style source | first tile requested | first content |
|---|---|---|
| inline | 1.33 s | 3.77 s |
| zip | 1.25 s | 3.89 s |
| **assets** | **5.75 s** | **6.86 s** |

Two independent problems:

1. **The assets style costs ~4.4 s before the first tile is even requested.** (Consistent with the
   6.45 s style decode noted earlier.) Only affects `StyleSource.ASSETS`.
2. **Even with inline/zip it is 3.8 s**: ~1.3 s before the first tile request (JVM attach, GL init,
   and ~0.6 s enumerating 220 system fonts + loading Roboto), then ~2.4 s until tiles are decoded
   and drawn.

Tile decoding is **not** the limit: `Options::setTileThreadPoolSize` defaults to 1 where tangram
uses 2, but raising it to 4 changed nothing warm (3.8 → 3.9-4.7 s) or cold (3.2 → 3.6 s), and four
workers really do start. The decoder holds its mutex only to copy `shared_ptr`s, so decode does
parallelise. The remaining 2.4 s needs timestamps inside the fetch → decode → upload path; one-second
`RenderStats` intervals are too coarse to place it.

---

## 6. Measured to cost (almost) nothing — do not re-run these

Each was an interleaved A/B at the demo camera unless stated.

| hypothesis | result |
|---|---|
| Source-density fills (no fill subdivision) | 16.6 vs 16.7 fps — the existing comment in `TileLayer.cpp:280` was right |
| Lattice clamp on surfaces (16 taps → 4) | 16.8 vs 16.9 — correct but unmeasurable, reverted |
| Shadows off | no change (cached/snapped) |
| Sky shader off | ~0 (−1 ms GPU) |
| Drape bakes disabled entirely (`DRAPE_BAKE_TIME_BUDGET = 0`) | 16.6 vs 16.6 |
| Flat 1×1 stencil mask instead of the full grid | 16.6 → 17.3 (+4%, not the 19% first claimed from a drifted run) |
| Half display resolution (¼ the pixels) | 17.9 → 20.3 (+13%) — not fill-bound |
| Worker-side VBOs for the depth job | no change |
| Elevation texture border re-encode | 0.1-0.8 encodes/s at rest (<1 MB/s); **3.5-6.8 MB/s during zooms and when panning into new terrain** — a transition cost, not steady state |

**Unexplained:** timed GPU sections total 21-25 ms against a 53 ms frame, while `layers` shows ~28 ms
CPU of which only ~5 ms is attributable (per-draw counters + mask/drape timers); the rest is blocking
inside GL calls. `glFinish` brackets (which inflate everything ~2.4× by destroying pipelining) put
the layer block at 116 ms and the vt 2D pass at 78 ms of it. Placing that gap properly needs
per-pass GPU queries inside the layer pass.

---

## 7. Next steps, in the order I would take them

1. **Hillshade (and contours) as shader blocks on the terrain draw** — §4. Biggest structural win
   for the configuration that feels slow; needs the composite-slot design above so ordering
   survives. Expect it to remove most of the ~130 render tiles / 161 surface draws per frame.
2. **Adopt tangram's screen-area LOD rule** in `TileLayer::calculateVisibleTilesRecursive` —
   `--es maxTileZoomOffset -1` already emulates it: **17.6 → 19.5 fps**, `layers` 28.3 → 22.2 ms, and
   the near field actually fills *faster* (fewer tiles to load). It is a behavioural change for every
   layer, so it wants to be an option, not a silent default.
3. **Decide on the tangram content model** (§3.1) — +7% and 2.7× fewer content indices, needs a
   see-through verdict at several cameras.
4. **Startup** — split the 2.4 s fetch → decode → upload gap with real timestamps; move the ~0.6 s
   font enumeration off the critical path; look at the assets-style decode (§5) if that path matters.
5. **Drop the stencil mask pass** where nothing needs it (+4%), and revisit draping itself: it costs
   7-11 ms GPU, and tangram does without it entirely.
6. **Place the unexplained GPU/CPU gap** (§6) with per-pass timer queries.

---

## 8. Open questions worth an experiment

- Why does `layers` CPU stay ~28 ms regardless of pixels, vertices, subdivision, draws and bakes?
- Does the elevation border re-encode explain the "slower in some places" feel during transitions,
  and would tangram's clamp-and-extrapolate be acceptable visually?
- Is the drape still worth its cost once content follows the surface exactly (§2.2)?

---

## 9. Terrain paint: the hillshade without a tile set

Landed for hillshade. The layer keeps its class, its API and its place in the layer order; what
disappears is everything behind it.

### 9.1 The shape of it

A layer in **paint mode** holds no tiles at all. It shades the elevation texture the 3D terrain has
already bound, as ONE quad per terrain tile, baked into the shared drape texture at its own position
in the bake order (`MapRenderer` bakes drape layers in layer order, so the style's placement of the
hillshade is preserved by construction — nothing in the composite layer changed).

What is gone per frame: the DEM tile set (its cull, fetch, decode, normal map build and upload), its
stencil mask, and its share of the render tiles. What is added: one two-triangle draw per tile per
**bake** — and bakes are cached, so in steady state it costs nothing.

- `vt`: `GLTileRenderer::setTerrainPaint`, `renderTerrainPaint`, `terrainPaint*` shaders.
- `all/native`: `HillshadeRasterTileLayer` decides paint mode, `TileRenderer::setTerrainPaint`
  carries it, `TileLayer::drapeStackSignature` watches it.

The lighting is *the same code*: the normal-map lighting shader (built-in or a custom one) is
injected over a prelude that reads the terrain DEM, and is handed a normal rebuilt from the DEM
gradient. All five hillshade methods, the colours and custom `getElevation()` shaders work unchanged.

### 9.2 When it engages

3D terrain **with draped fills**, the layer's data source is the terrain's own, and the built-in
contour lines are off. Anything else keeps the normal-map tile path, untouched: a different DEM must
not be silently replaced by the terrain's, without the shared drape there is no layer-ordered bake to
paint into, and the contour lines live in the normal-map fragment shader *outside* the lighting
shader the paint reuses — in paint mode they would simply vanish (until the CONTOUR kind lands).

**It is not pixel-identical, by construction.** The sampling is the terrain's elevation grid, so the
layer's own zoom level bias no longer reaches it, and the gradient is recomputed per drape texel in
floats instead of interpolating an 8-bit-packed 256² normal map — crisper, and blockier where the DEM
grid is coarse. A custom normal-map lighting shader that reads `getRawColor()` sees the terrain's
re-encoded DEM texel, not the source tile's; `getElevation()` is the portable one.
Switch it off with `HillshadeRasterTileLayer.setTerrainPaintEnabled(false)` or, for an interleaved
A/B that also reaches a composite layer's internal hillshade child,
`adb shell setprop debug.carto.terrainpaint 0`.

### 9.3 Two things the port had to get right

**The relief boost follows the sampling density, not a tile id.** MapLibre's low-zoom boost is keyed
off zoom; the normal-map path multiplied the tile zoom by its bitmap resolution, so a 512-texel grid
at z11 was worth a z12 tile of 256 texels — which is exactly what the terrain's elevation grids are
(measured: z11 grid, 514² texture, 38.2 m/texel — the same density as an old z12 hillshade tile).
Keyed off the grid's own zoom the paint read the same data as one level coarser and came out ~1.5×
too strong. It now derives the zoom from metres per texel.

**A paint has no per-tile fingerprint.** It is not made of the layer's tiles, so the drape cache
cannot notice a parameter change through one. Reporting the previous frame's cover instead made every
tile that had just entered it look incomplete and bake twice (measured: surface draws *up* 12%). The
paint's appearance now rides `TileLayer::drapeStackSignature`, and the layer reports no tiles at all.
Because the paint is baked, a change re-bakes: with `IlluminationMapRotationEnabled` (the default)
that includes the map rotation, quantised to 2° so a rotation gesture re-bakes a bounded number of
times instead of every frame.

### 9.4 The DEM level: why full detail is off

The terrain caps the elevation grid at what the MESH can express (`ElevationManager::clampTileZoom`:
one texel per half surface cell), which drops two zoom levels. Shading is per fragment and resolves
far more than that, so on the paint that cap is visible as blur from z15 up - it is why the hillshade
"does not render to the DEM's max zoom". `getFullDetailDataTile` + `ElevationTextureCache::setFullDetail`
lift it for the paint's own cache.

It stays OFF by default, because the elevation texture pipeline cannot pay for it (Crosscall, north
pan, `debug.carto.paintdetail 1`): **2.5 fps against 6.7**, with `drape` at 172-218 ms. Measured
cause: each DEM grid is 512², re-encoded into a 514² RGBA texture **on the render thread** and
uploaded there (53 ms + 52 ms per texture at full detail), and the working set jumps ~16× past the
96-texture cache. At the mesh level the same path barely runs (fewer than 64 encodes over a whole
run, `drape` 6-8 ms) - it is the full-detail working set that breaks it, not the encode as such.

Tangram pays none of this: `elevation.yaml` binds the source raster (256², `filtering: nearest`)
as the tile's own texture, uploaded once when the tile loads, ancestors addressed through
`u_raster_offsets` uv sub-rects, edges extrapolated in the shader. Porting that means: build the
texture payload where the grid is decoded (a worker) instead of in the drape section, upload the
grid's own samples with no per-texel re-encode, and patch the neighbour border as small
`glTexSubImage2D` strips instead of rebuilding the tile. Our border machinery (cross-level backfill,
edge box filter) is a seam feature tangram does not have, so it has to survive the port - which the
strip-patch form allows.

One step of that already landed: the encode's interior is now a straight copy of the grid's own
rows. It used to run a per-texel lambda with neighbour dispatch and four edge-filter tests over the
whole tile (79 ms per texture measured; 45-53 ms after).

### 9.5 Measured, and what is left

Device (Crosscall, north pan, interleaved, `bench/abpaint.sh`, 33-34 windows each):

| config | fps | frame | drape | layers |
|---|---|---|---|---|
| terrain paint | 6.7 | 134 ms | 10.2 | 47.2 |
| paint + full DEM detail | 2.5 | 400 ms | 218 | 79 |
| normal-map hillshade | 7.0 | 117 ms | 7.7 | 53.3 |

**No win yet in this configuration** - and that is informative: the hillshade layer's cost here is
tile loading (network, decode, normal map), not the render thread, while `layers` is the base map's
own geometry. The paint removes the former, which this bench does not measure. Measuring the
hillshade alone is the right next bench, and it needs the gap in §9.5 closed first.

Emulator counters for the same pan (structural, fps there is capped and means nothing):

| | render tiles | style layers | surface draws | surface indices |
|---|---|---|---|---|
| normal-map hillshade | 7000-7300 | ~460 | 8750-9000 | 215-220M |
| terrain paint | 5300-5700 | 390-420 | 7000-7600 | 172-186M |

Appearance at the ridge camera (45.244172/5.760595 z13.2 t55): 25% of pixels differ by more than 12,
mean absolute difference 8.4/255 — the paint is sharper, because it samples the DEM per fragment
instead of magnifying a 256² normal map. No brightness shift.

**Open: a hillshade-only stack draws nothing under the paint.** With no vector layer there is no
drape cover, so the paint is given the terrain's own cover (`TerrainRenderer::collectVisibleTiles`
via `TileLayer::needsDrapeCover`) - but nothing in such a stack ever loads elevation, and the drape
reports `tiles without elevation 12 of 12`. A DEM prefetch and a redraw pump from the seeding did not
close it; the load path needs a look. Note the same stack on the normal-map path is also flat (faint
shading, no relief), so 3D terrain in a hillshade-only stack is broken independently of the paint.

Left to do: that gap (it blocks benching the hillshade alone), the elevation texture port (§9.4), the
CONTOUR and HYPSO paint kinds (same quad, same prelude), contour labels as tangram generates them
(short seed-walk stubs from `ContourTileDataSource`, styled by the existing `#contour` text rules),
and a paint path for the no-drape configuration, where the layer still keeps its tiles.
