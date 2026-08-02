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
/bin/sh /tmp/north.sh "baseline"      # recreate from §1 if /tmp was cleared
```
Expect ~6.8 fps, ~131 ms/frame, ~130 render tiles/frame. If it is not that, the camera or the layer
set is wrong — fix that before optimising anything.

**Move 1 — hillshade AND contours as shader blocks on the terrain draw (§4).** The one with real
leverage, and now also the fix for a correctness problem: the hillshade layer alone multiplies render
tiles ~5× (132 → 693 per interval), and contours drawn as geometry are what see through ridges when
any depth bias is applied (§3.1). Bind the shared DEM raster to the tile draw and let the composite
slot's shader compute shading and contour lines where the style places them, instead of giving the
slot its own `HillshadeRasterTileLayer` / contour source. Ordering survives (see §4); the duplicate
tile set, surface pass and stencil mask disappear, and contours painted onto the surface cannot show
through it at all.
*Acceptance:* render tiles per frame in the north pan drop toward the base-only count (~40) with the
hillshade still in its style position, and `/tmp/north.sh` improves materially. Read
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

Two decisions are Martin's, not the agent's: whether the tangram content model
(`debug.carto.linesourcedensity` + `debug.carto.depthshift`) is free of see-through at his cameras,
and whether the coarser LOD's label density is acceptable.

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

| script | what |
|---|---|
| `/tmp/ab.sh` | run one config, print `PROF` lines tagged with a label |
| `/tmp/abapk.sh` | install APK, then `ab.sh` — for interleaving two builds |
| `/tmp/abprop.sh` | same, but A/B by system property (one APK) |
| `/tmp/ab2.sh` | as `ab.sh` at a mountain camera |
| `/tmp/north.sh` | pan **north into the mountains**, full stack — the slow case |
| `/tmp/absum.py` | median summary, discards windows > 1600 ms (idle) |

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
