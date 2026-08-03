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

> ### DECIDED — do not re-open
>
> **tangram-ng is the reference. Where it does something differently, we adopt its way; we do not
> weigh alternatives.** (Martin, explicitly: "they do it right, no need to think.")
>
> **The RTT drape goes.** Tangram does not bake anything into per-tile textures - vector geometry is
> displaced per vertex and drawn directly, the terrain raster is drawn once per tile. So we do not
> keep the drape "for ordering", "for quality" or "for the hillshade slot": the answer to every such
> question is what tangram does. Removing it removes the bake, the drape texture memory, the drape
> sample on the surface, and the whole stand-in/seed/stale machinery that keeps producing tile
> artifacts.
>
> **The drape cannot go on its own** (measured, device, north pan, background-only style):
> paint + drape 19.9 fps / `layers` 10.6 ms, paint without drape **15.5 fps / 26.0 ms**. Without the
> drape each layer still draws its own depth pre-pass surface AND its stencil mask, and a paint adds
> a third surface pass per tile - where the drape drew one shared surface plus one mask. Tangram has
> NEITHER a per-layer pre-pass nor stencil masks. So it is one change: drape + pre-pass + masks go
> together, leaving one ground draw per tile and displaced geometry for everything else.
> **That change has LANDED behind `--es drape false` — see §10.** The default is still the drape,
> because the device verdict is not in: on the emulator the ground side got 39% cheaper and the mask
> pass vanished, while the fills - baked once with the drape - now cost 13× the index throughput
> every frame. **§10.4 is therefore the next move: stop subdividing content** (tangram does not
> subdivide at all), then re-take the device numbers and flip the default. §10.3 has the device A/B
> so far: 26.6 vs 36.6 fps before the paint was made to work without the drape, not re-measured
> since (the phone went away).
>
> **Shadows stay** (Martin). They are their own caster pass and FBO, not part of the drape.
>
> Read the reference before designing: `res/scenes/terrain-3d.yaml` (per-vertex displacement),
> `core/src/style/rasterStyle.cpp` (one shared 64-grid mesh, vertex = 2 x GL_SHORT, one draw per
> tile, no depth pre-pass), `res/scenes/hillshade.yaml` (shading in the terrain draw's colour block).
>
> **Build trap:** gradle prints BUILD SUCCESSFUL while ninja has failed - two benches in one session
> measured stale native code. After any change under `libs-carto/` or a renamed symbol, verify the
> symbol is really in the library. The APK's copy is STRIPPED, so grep the unstripped one ninja
> produced and check the mtimes line up with the APK:
> ```sh
> strings -a scripts/android-dev/carto_mobile_sdk/build/intermediates/cxx/Debug/*/obj/arm64-v8a/libcarto_mobile_sdk.so | grep <new symbol>
> ```
>
> **Measure the terrain with a background-only style** (`--es minimal true`): with the full style the
> base map's geometry is most of the frame and hides everything else (measured: a change worth +29%
> read as a wash).

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

**`bench/ab2.sh` passes `--es base plain`, and the `#hillshade` slot only exists inside the
COMPOSITE base layer** - so a hillshade run started from it silently measures a frame with no
hillshade in it. Add `--es base composite` for anything involving the hillshade or the satellite
slot. (Cost one whole device A/B round before Martin spotted the missing shading on screen.)
**Tilt is 90 = straight down**, so a "tilted" camera for occlusion work is t=20, not t=55: at t=55
the view is close to plan and no ridge occludes anything.

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

Device (Crosscall, north pan, interleaved, `bench/abpaint.sh`). **Measure the hillshade with a
background-only style** (`--es minimal true`, which strips the inline style to the Map background
plus the composite slots): with the full style, the base map's own geometry is most of the frame and
hides the whole effect. 37-39 windows each:

| config | fps | frame | drape | layers |
|---|---|---|---|---|
| **terrain paint** | **22.0** | 38.7 ms | 2.8 | 10.0 |
| paint + full DEM detail | 13.2 | 66.6 ms | 26.6 | 11.6 |
| normal-map hillshade | 17.0 | 49.9 ms | 5.2 | 16.5 |

**+29% fps, 49.9 -> 38.7 ms per frame.** With the full style over the same pan the two are a wash
(6.7 vs 7.0 fps, 33-34 windows) - the hillshade's cost there is tile loading, which the frame timer
does not see, while `layers` is the base map:

| config (full style) | fps | frame | drape | layers |
|---|---|---|---|---|
| terrain paint | 6.7 | 134 ms | 10.2 | 47.2 |
| paint + full DEM detail | 2.5 | 400 ms | 218 | 79 |
| normal-map hillshade | 7.0 | 117 ms | 7.7 | 53.3 |

Emulator counters for the same pan (structural, fps there is capped and means nothing):

| | render tiles | style layers | surface draws | surface indices |
|---|---|---|---|---|
| normal-map hillshade | 7000-7300 | ~460 | 8750-9000 | 215-220M |
| terrain paint | 5300-5700 | 390-420 | 7000-7600 | 172-186M |

Appearance at the ridge camera (45.244172/5.760595 z13.2 t55): 25% of pixels differ by more than 12,
mean absolute difference 8.4/255 — the paint is sharper, because it samples the DEM per fragment
instead of magnifying a 256² normal map. No brightness shift.

### 9.6 Open bugs, as of 2026-08-02

- **Tiles blink while zooming in.** Distinct from the missing-tile bug (fixed, confirmed on device):
  a flash as tiles turn over. Most likely the drape cache's generation swap - seed, stand-in, then
  the tile's own bake - rather than the paint. Both this and the next one are properties of the
  drape, so `--es drape false` is now the way to confirm them: with §10 that path is the tangram
  arrangement rather than the old slow fallback. **Still to be verified on device.**
- **Artifacts at high zoom (z15+):** blurred ground with the background bitmap's pattern showing
  through. One 1024 drape texture per tile, magnified far past its resolution. Drawing the paint
  without the drape (`--es drape false`) is sharp at the same camera, which is corroboration, not
  proof.
- ~~**Tile edge stitching is probably not applied at all.**~~ **Fixed.** `buildTerrainEdgeCoarsening`
  ran only from `setVisibleTiles`, so the coarsening map was built from A LAYER'S OWN visible tiles
  while the surfaces actually drawn come from the drape cover (normalised leaves, drawn by
  `drapeLayers.front()`), from the shared ground cover (§10) or, for a paint, from the terrain
  cover. A paint has no tiles at all, so its map was empty and its surfaces stitched nothing. The
  renderer now keeps the cover it is handed (`GLTileRenderer::terrainSurfaceTileIds`: drape cover,
  else ground cover, else paint cover, else its own tiles) and rebuilds the map whenever that
  changes, so stitching follows the geometry that is drawn in every mode.

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

---

## 10. The shared terrain ground: no drape, no per-layer pre-pass, no stencil masks

This is the change §0 called for, and it is one change because the three costs are one arrangement.
Nothing is baked any more: the layer stack shares ONE cover of terrain tiles, the ground is drawn
once for that cover, and every layer then composites straight onto it in layer order - which is
what tangram does (`core/src/style/rasterStyle.cpp`: one shared grid mesh, one draw per tile, no
pre-pass, no masks anywhere in `core/src`).

Active whenever 3D terrain is on and draped fills are off. **The demo now defaults to it**
(`DemoConfig.TERRAIN_DRAPE_FILLS = false`); `--es drape true` brings the drape back for an A/B.
The SDK option `TerrainOptions.DrapeFillsEnabled` still defaults to on, so no app changes behaviour
until it opts in — that default flips, and the drape code goes, once §10.2 lands and the device
numbers are in. **The drape is being dropped, not kept as an option.**

### 10.1 What each piece becomes

| | with the drape | shared ground |
|---|---|---|
| ground geometry | one grid surface per tile, textured with the tile's baked 1024² RTT texture | one grid surface per tile, in the ground colour, lit and shadowed |
| fills | baked flat into that texture (cached) | drawn as displaced 3D geometry, every frame |
| tile backgrounds / rasters | baked | drawn on the COVER tiles, with the source uv sub-rect the overzoom path already computes |
| depth | the drape surface is the only depth writer | the ground pass is the only depth writer |
| per-layer depth pre-pass | already skipped | skipped |
| stencil tile masks | one grid draw per tile per layer | none |

Two rules hold the depth model together, both inherited from rounds 45-56 and unchanged:

1. **The ground is drawn at its TRUE depth and is never pushed back.** Everything after it is
   `GL_LEQUAL`, no bias in either direction: coincident content passes, content behind a ridge
   fails. A forward pull is what leaks far-slope content over a crest.
2. **Ground-shaped content is drawn on the cover tiles, not on the layer's own.** Two tesselations
   of one height field do not agree, so a hillshade at z12 drawn on its own tiles would z-fight the
   z14 ground. On the cover it is coincident to the bit. That is why the cover computation
   (`MapRenderer::collectTerrainCover`, extracted from the drape path) is shared by both modes.

Without the masks, a retained blend-out (proxy) tile could paint over the live tile that replaced
it during an LOD change, so the near-to-far sort inside a style layer now puts proxies first — the
same intent the mask stamping order had.

### 10.2 Measured (emulator, structural)

Ridge camera 45.244172/5.760595 z13.5 t20, hillshade + contours + elements, scripted pan, per
one-second interval. Emulator fps means nothing; these are counts.

| | drape | shared ground |
|---|---|---|
| stencil mask draws | 4209 | **0** |
| mask time | 24.7 ms | **0** |
| surface draws | 5612 | **3450** (−39%) |
| surface indices | 138 M | **85 M** (−38%) |
| geometry draws | 2806 | 9100 |
| geometry indices | **25 M** | **339 M** |

So the ground side is 39% cheaper and the mask pass is gone — and the fills that used to be baked
once now cost 13× more index throughput, every frame. That is the whole trade, and it is why the
next step is not optional:

### 10.3 The paint had to come with it (device, measured)

The first device A/B said **26.6 fps against the drape's 36.6** — and it was not the ground pass.
`HillshadeRasterTileLayer::isTerrainPaintActive` required draped fills, so turning the drape off
turned the *paint* off too and the layer went back to its own DEM tile set: fetch, decode, normal
map, upload and ~5x the render tiles, to draw what the terrain already had on the GPU. The paint now
only requires 3D terrain: with the drape it takes its place in the shared bake, without it it draws
itself as the terrain surface on the shared ground cover
(`GLTileRenderer::renderTerrainPaintSurfaces`, which until now had never actually run).

Two bugs surfaced the moment it did, both fixed:

- **The sky went black.** The paint surface pass left its VBOs bound, and `SkyRenderer` draws its
  quad from a CLIENT-SIDE array - with a `GL_ARRAY_BUFFER` bound, that pointer is an offset into it,
  so the sky quad flew off screen and the clear colour showed. Any renderer that feeds a client
  array is exposed to this; unbind after every vt draw loop.
- **White speckles over the shading.** The paint draws the same grid, displaced by the same DEM, as
  the ground pass already drew - but from a different program, so the clip z differs in the last
  float bits and GL_LEQUAL dropped a scatter of fragments, showing the bare ground colour. One
  `TERRAIN_LAYER_DEPTH_DELTA` of clearance (what backgrounds carry over the surface they share)
  fixes it.

**Ground-shaped draws per tile is now the number to watch.** The drape does ONE (plus a cached
bake); the shared ground was doing THREE - the ground fill, the paint, and the style's tile
background. The third is gone: a patternless background of exactly the ground colour is skipped,
because the ground pass has already painted it with the same displaced grid (emulator: background
draws 1234 -> 160 against 487 ground fills). Two remain wherever a paint is in the stack, and each
is a full grid draw whose vertex stage does ~20 elevation texture fetches. Folding the paint INTO
the ground pass would take it to one, which is exactly tangram's arrangement, but it moves the
hillshade to the bottom of the layer order - it needs a decision, not a patch.

**§10.4 The fills must stop being subdivided.** `TileLayer.cpp` decodes every fill subdivided to
`tileMeters / meshResolution` (`terrainSourceDensity = false`, always) because an un-subdivided fill
that is NOT draped sags below the surface. With no drape at all that reason is gone and tangram's
answer applies instead: source-density content plus the constant-clip `depth_shift`
(`debug.carto.linesourcedensity 1`, `debug.carto.depthshift 0.02`, §3.1), measured at 2.7× fewer
content indices. It was rejected as a default because contour lines showed through ridges — which
§4 (contours as a shader block on the terrain draw) is what actually fixes.

### 10.5 What this path still does not do

- **The sun works; cast shadows are wired but switched OFF.** The light and shadow block came out
  of the drape path into `MapRenderer::applyTerrainShadows`, which both paths now call with their
  own cover - so the light boxes, the caster pass and the map cache are shared code, and the shared
  ground gets the resolved stack lighting before it draws. Two real bugs were in the way and are
  fixed: `TileRenderer` only enabled terrain lighting `if (drapeFills)`, so with the drape off the
  ground AND the hillshade paint over it were unlit (and a shadow multiplies the LIT colour, so
  nothing could show); and the paint, which covers the ground it is drawn on, had no lighting of
  its own - it now takes the same sun and shadow the surface takes, from the geometric normal, not
  from the hillshade's own exaggerated slope.
  What is still wrong: with the caster pass enabled the map reads as scattered **shadow acne**
  instead of the drape's cast shadows - same scene, same map, same emulator, the drape path clean
  and this one not. So `applyTerrainShadows(..., castShadows = false, ...)` for the shared ground:
  half-working shadows are worse than none. Flip it to true to work on it, and diff against the
  drape path, which is the reference.
- **A cover leaf coarser than a render tile** (only when the split hits its 256-tile cap) makes that
  tile draw on its own surface, one tesselation finer than the ground it stands on.
- Device numbers, and the verdict on the two drape bugs in §9.6, are still to be taken.
