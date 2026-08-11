# Measuring and improving performance

Scope: how to get a number you can trust, what the frame currently costs, and what is known **not**
to be worth optimising. The dated history and the raw numbers live in
[../render-performance.md](../render-performance.md); this page is the method and the current state.

## Build

```sh
cd scripts/android-dev && ./gradlew :app:assembleDebug -x lint -PprofileRender
adb install -r -t app/build/outputs/apk/debug/app-debug.apk
```

- The native SDK is compiled **optimised by default** (`RelWithDebInfo`), from the command line and
  from Android Studio alike. `-PnativeOpt=false` goes back to `-O0` for stepping through native code.
  This matters more than any single optimisation so far: the same code at `-O0` measured **8.6 fps**
  against **12.8** at `-O2` on the north pan.
- `-PprofileRender` compiles in `FrameProfiler` (`PROF` lines) and `vt/RenderStats.h` (`RenderStats`
  lines). Neither exists in the binary otherwise.
- Install from `app/build/outputs/apk/debug/`. `app/build/intermediates/apk/debug/` also holds an
  `app-debug.apk` and it is **stale** — *except* when you pass
  `-Pandroid.injected.build.abi=<abi>` to build a single ABI, which inverts it: AGP then writes the
  fresh APK to `intermediates/` and leaves `outputs/` untouched. Check the mtimes, not the path.
- **`RelWithDebInfo` is not what ships.** It is a plain `-O2 -g` build: no LTO, and none of the
  per-subproject `-O2`/`-Oz` split, which is gated on `CMAKE_BUILD_TYPE MATCHES Release`. The
  shipped Release build compiles the SDK at `-Oz -flto=thin`. Bench the shipped configuration with
  `-PnativeConfig=Release` (Release strips, so simpleperf loses its symbols — use it for `PROF`
  numbers, not for profiles).

## The three instruments

| instrument | what it gives | gotchas |
|---|---|---|
| `PROF` | CPU ms per frame section: `sky prelude prepare cover drape layers layers3D billboards` | `sky` is mostly the swap wait, not work. Not comparable across apps. |
| `PROF GPU` | the same sections on the GPU (`GL_EXT_disjoint_timer_query`) | Android only; off with `setprop debug.carto.gputimer 0` |
| `RenderStats` | draws, indices, render tiles, style layers, surfaces, label and prep timings, tile-surface builds | per one-second interval, deltas |
| `simpleperf` | an actual CPU profile of the render thread | see below — this is what finds things the timers cannot |

### Profiling the render thread

```sh
adb shell simpleperf record --app com.akylas.cartotest -g -f 500 --duration 12 -o /data/local/tmp/perf.data
adb pull /data/local/tmp/perf.data /tmp/perf.data
# symbols: the UNSTRIPPED .so, in a tree mirroring the device path
D='/tmp/symfs/data/app/~~<hash>==/com.akylas.cartotest-<hash>==/lib/arm64'; mkdir -p "$D"
cp scripts/android-dev/carto_mobile_sdk/build/intermediates/cxx/*/*/obj/arm64-v8a/libcarto_mobile_sdk.so "$D/"
$NDK/simpleperf/bin/darwin/x86_64/simpleperf report -i /tmp/perf.data --symfs /tmp/symfs \
  --tids <gl-thread-tid> --children --sort symbol -n
```

Two traps: the report is per **process** by default and the tile decode threads are as busy as the
render thread, so always pass `--tids`; and several threads are called `GLThread` — the render thread
is the one whose call graph starts at `MapRenderer::onDrawFrame`.

## Getting a trustworthy number

- **Device numbers drift.** The same build has measured 8.4, 11.2 and 16.1 fps in one evening.
  Only **interleaved** A/B means anything: alternate two APKs, ≥3 repeats, report the median and the
  spread over one-second windows. A comparison against a number taken earlier is worthless.
- **Emulator fps is meaningless.** Emulator runs are for *counters* (draws, indices, render tiles) and
  for functional checks.
- **The camera decides what you measure.** The slow case is panning **north into the mountains**
  (`--es animLatDelta 0.06`) with contours and hillshade. Panning east over the valley is cheap.
  Tilt matters as much: at tilt 85 most of the screen is sky. Tilt 90 is straight down, so a
  "tilted" camera for occlusion work is t=20, not t=55.
- **`bench/ab2.sh` passes `--es base plain`, and the `#hillshade` slot only exists in the COMPOSITE
  base** — a hillshade run started from it silently measures a frame with no hillshade in it. Add
  `--es base composite`.
- Helper scripts: `scripts/android-dev/bench/` (`ab.sh`, `ab2.sh`, `north.sh`, `abapk.sh`,
  `abprop.sh`, `absum.py`).

## Reset the debug props before measuring

`debug.carto.*` properties survive until the device reboots, and a session that leaves them set
measures a crippled build for weeks. A run in August 2026 found `drapebudget 0` and `drapemip 0`
(the drape memory budget and its mipmaps, i.e. the whole win of the round that added them) still
set from the session that introduced them, along with `paintdetail 0` and `skyclip 0`. Clear every
one of them before a baseline:

```sh
for p in areasourcedensity areathreshold asyncdepth asyncdepthms background demtaps depthshift \
         drapebudget drapemip groundpaint linesourcedensity paintdetail skyclip terrainpaint \
         tilebg tilemasks; do adb shell setprop debug.carto.$p '""'; done
```

## Where the frame goes today

At `-O2`, on the north pan with content, the render thread has **no dominant leaf**: draw submission
(`renderTileGeometry` ~28% inclusive, ~156 draws/frame), `renderGeometry2D` ~33%,
`TileRenderer::prepareFrameUnsafe` ~10%, `GLTileRenderer::startFrame` ~9%,
`ElevationTextureCache::getTexture` + `resolveEntry` ~9%, hillshade layer ~6%.

The GPU is not the limit: `PROF GPU` with content puts the layers at 29–43 ms and the total at
38–53 ms against a CPU frame of 120–175 ms at `-O0`. **We are CPU-bound, on draw submission.**

So the lever is **fewer draws and fewer layers**, which is [07-hillshade-contours.md](07-hillshade-contours.md)
and [09-composite-layer.md](09-composite-layer.md), not micro-optimisation.

## Style load and tile decode (off the render thread, but in front of the user)

Measured on a Crosscall HLTE556N with the demo's bundled style project (`--es style assets`:
`osm.json`, 23 layers, 67 styles, 461 nutiparameters, 9 `.less`/`.mss` files, 74 KB), with temporary
timers in `CartoCSSMapLoader`, `TileReader` and `MBVectorTileDecoder`. Device clocks move the
absolute numbers by up to 40% between runs — compare a change against a run whose *style load* time
matches, or pair the runs.

**Loading a style: ~0.5–0.7 s**, split roughly 7 / 75 / 20 between parse, compile and everything else:

| section | ms |
|---|---|
| `CartoCSSParser::parse`, all 9 files (boost::spirit) | 34 |
| `CartoCSSMapLoader::buildMap` | 362 |
| — `CartoCSSCompiler::compileLayer`, all layers | 271 |
| — translate to mapnik rules | 71 |
| — `Style::optimizeRules` | 9 |
| fonts, asset scan, symbolizer context | ~110 |

The compile is three layers: `transportation` **115 ms** (2230 rules, 23 attachments), `route` 61 ms,
`poi` 61 ms; the other 20 together ≈ 35 ms.

Inside `compileLayer` it was three near-equal thirds, and each answered to a constant factor rather
than to the algorithm (per-section ms, `transportation`, cold runs on the Crosscall):

| section | before | after |
|---|---|---|
| `buildPropertyLists` | 37.8 | 22.8 |
| per-zoom filter evaluation | 33.4 | 12.1 |
| `buildLayerAttachment` | 39.6 | 31.4 |
| list comparison | 2.2 | 2.0 |

What the three fixes were, in the order they pay: **evaluate each distinct predicate once per zoom**
(a layer has ~77 predicates and ~640 properties, and every property re-evaluated its own filters —
this is a memo, not a semantic change); **bucket `insertProperty` by field** (two properties can only
be equal if they set the same field, and comparing them is a deep expression comparison, so the scan
went over every property inserted so far); **intern the field strings and hand out references** in
`buildLayerAttachment` (its innermost operation was a string compare, and each property visit copied
a `shared_ptr`). Summed over all 23 layers: **264 → 157 ms**, `buildMap` 362 → 261 ms.

A second round took the same three sections further, for **157 → 129 ms** (`buildMap` 220 ms):

- **A zoom whose predicates evaluate exactly as the previous one is skipped whole.** The optimized
  property lists are a pure function of (property lists, predicate results), so equal results mean
  equal lists and equal attachments. Comparing ~80 bytes replaces rebuilding every property list and
  deep-comparing it. A layer resolves to 1–14 distinct ranges out of the 25 zooms evaluated.
  `boost::tribool` cannot be compared as a block — two indeterminate values do not test equal — so
  the results are kept as a three-state byte.
- `buildLayerAttachment` built a fresh property set (two vectors) per (property, property set) pair
  considered and dropped it on the common path; one reused object keeps the buffers.

Cumulative: **compile 264 → 129 ms**, `buildMap` 362 → 220 ms on the same style and device.

Left on the table: `buildPropertyLists` still walks the **whole** stylesheet once per layer (23 × 407
elements here) — worth ~10–15 ms total, so low priority — and `buildLayerAttachment` remains the
biggest single item (~31 ms of `transportation`'s 55). Its cost is O(properties × property sets²) per
distinct zoom range, which is the algorithm, not a constant factor.

**`setPixelScale` used to reload the style.** It rebuilt the symbolizer context by calling
`updateCurrentStyleSet`, i.e. a full parse + compile, and `VectorTileLayer` calls it when the layer
joins a map — so every startup paid the ~0.5 s twice. Split into `updateSymbolizerContext()`
(fonts, bitmap/stroke/glyph maps, settings), the second pass is **~100–130 ms**. `addFallbackFont`
took the same path. `setCartoCSSLayerNamesIgnored` genuinely changes compilation and still reloads.

**Decoding a tile: 120–150 ms mean, ~0.5 s worst** at a z16 city camera. Section split (probe
overhead ~30%, so read the shares, not the absolutes):

| section | share |
|---|---|
| symbolizer → geometry/label build (tesselation) | 35% |
| loop glue: `shared_ptr`-keyed caches, 67 layer builders per tile, batching | 22% |
| `TileLayerBuilder::buildTileLayer` | 13% |
| filter predicate evaluation | 12% |
| feature tag decode | 7% |
| protobuf geometry decode + clip | 5% |
| symbolizer property evaluation | 4% |
| rule prefilter + field gathering | 2% |

So the CartoCSS/expression machinery is **~18% of a decode** — geometry building is the cost.

### Live style parameters

`setStyleParameter` used to invalidate every tile ([TileLayer.cpp](../../all/native/layers/TileLayer.cpp)
`updateTiles`), so changing one `nuti::` colour cost *visible tiles × ~130 ms* of decode CPU. A
parameter that **only** feeds properties the renderer evaluates per frame does not need any of that:

- the values live in a `mvt::NutiParameterStore` that decoded tiles hold a pointer to, so replacing
  them is visible to already-decoded tiles;
- a colour/width property whose expression reads parameters (and at most the view state) becomes a
  `vt::ColorFunction`/`FloatFunction` instead of being folded at decode — `Property::isLiveCapable`;
- `mvt::resolveLiveNutiParameters` classifies each parameter at load, and `MBVectorTileDecoder`
  takes the cheap path only when **every** parameter in the call is live: swap the values, ask for a
  redraw (`onDecoderRefreshed`), decode nothing.

Conservative by construction. A parameter is **not** live when it appears in a rule filter (it
decides what the tile contains), when it feeds a property that is also read at decode time — glyph
raster size, generated marker bitmap, stroke pattern (`Property::isBakedAtDecode`) — when the
expression also reads a feature field or the zoom, or when it drives `_geometryscale`, `_fontscale`
or `_zoomlevelbias`. Anything unclassified stays on the re-decode path.

Measured on the device with the demo's in-memory nuti style (`--es style nuti`): flipping a colour
parameter every 3 s produced **zero `decodeTile` calls** and the water polygons changed between the
two colours in the next frame; flipping the boolean the style uses in a filter still re-decodes, as
it must. Worth knowing: the bundled 23-layer style has **no** live parameter — its 461 parameters all
sit in filters, text or marker sizes — so this pays only for styles written with colour parameters,
which is the point of the feature.

Classification costs ~37 ms once per style load on that style (a walk over every rule and property).

## Measured NOT to matter — do not re-run these

| hypothesis | result |
|---|---|
| geometry volume (area subdivision off) | indices 37.3M → 7.5M, **+6.5%, inside the noise** |
| per-vertex DEM taps 16 → 1 | 5.69 vs 5.86 fps with content — nothing (worth ~20% terrain-only) |
| tile LOD granularity (`--es maxTileZoomOffset -1`) | 11.46 → 11.16 fps |
| paint-as-ground (`debug.carto.groundpaint 1`) | nothing, twice |
| the far plane (tangram's formula) | never binds at the cameras tested |
| tile decode pool size 1 → 4 | no change warm or cold |
| shadows off / sky shader off | ~0 |
| half display resolution (¼ the pixels) | +13% — not fill-bound |
| lattice clamp on surfaces (16 taps → 4) | correct but unmeasurable |
| the per-tile CPU surface rebuild ("the pan hang") | **does not happen at all** in grid mode: `surfBuilt=0 surfInval=0`, the block costs 0.04 ms |
| DEM border patching instead of full re-encode | 93% fewer re-encodes, **no fps change** at any camera — the encode was never on the render thread |
| the DEM encode path in a warm pan | **zero encodes** — there is nothing there to optimise |
| skipping the layer builder for styles the prefilter empties (67 per tile) | 3 paired cold runs each: decode mean 148 vs 147 ms — inside the noise |

## Things that did pay

| change | effect |
|---|---|
| native `-O2` instead of `-O0` | 8.6 → 12.8 fps |
| sort key computed once per tile instead of per comparison | part of 7.3 → 8.7 fps |
| elevation texture bitmap built on the encode worker | same pair; `layers` 49.5 → 26.0 ms, fps p25 3.9 → 8.1 |
| shared ground (no per-layer pre-pass, no stencil masks) | mask draws 4209 → 0, surface draws −39% |
| hillshade as a terrain paint | +29% fps in a background-only style |
| mesh resolution 128 → 64 (tangram's value) | 8.5 → 15.2 fps |
| occlusion depth read-back on its own thread | 13.7 → 14.9 fps |
| contour lines as a shader block | render tiles 494 → 216 |
| contour label stubs + shader lines (device) | 14.5 → 16.6 fps, `layers` 8.7 → 7.0 ms |
| label mutex taken per 32 labels instead of per label | culler pass 19.4 → 15.4 ms (device, ~1960 labels) |
| label terrain re-anchor: DEM tile loads no longer read as a scale-only change, plus a grid and a latitude-scale memo | full stack over terrain, interleaved ×3: **1.00 → 1.55 fps**, `prepare` 658 → 219 ms |
| an off-screen, already-anchored label defers its re-anchor | 1.60 → 1.70 fps — small, most dirty labels do hold a placement |
| label lines tesselated for reading, not for painting (no lattice split, surface-cell step) | **1.75 → 2.10 fps**, `prepare` 157 → 72 ms |
| `setPixelScale` rebuilds only the symbolizer context, not the compiled map | startup style cost 2 × 0.5–0.7 s → one load plus a ~0.1 s context rebuild |
| CartoCSS compile: per-zoom predicate memo, field-bucketed property insert, interned field ids | compile 264 → 157 ms, `buildMap` 362 → 261 ms (23-layer style, device) |
| CartoCSS compile: skip a zoom whose predicate results repeat, reuse the trial property set | compile 157 → 129 ms, `buildMap` → 220 ms |
| live style parameters (a colour-only parameter swaps values and redraws) | a parameter change went from *visible tiles × ~130 ms* of decode to **zero decodes** |
| render and tile paths at `-O2` in Release instead of `-Oz` | device 39.09 → 37.82 ms/frame (3.2%), CPU work minus the swap wait 14.39 → 13.51 ms (6.1%), `prepare` 2.65 → 2.22, `prelude` 0.91 → 0.70; +614 KB on arm64 |

The `-Oz` → `-O2` A/B is a warning about the emulator as much as a result. Three interleaved cycles
on the emulator put the mean 4.8% apart but reversed the sign on one cycle out of three — no
conclusion. The same six runs on the device (Adreno 610) favoured `-O2` in **3 of 3 paired runs**.
Anything worth a few percent needs the device and needs pairing; a single emulator run will happily
report 10%.

### The label culler, measured on the device

A pass is `cullMs / cullPasses` from the `RenderStats:` line. At a POI-dense camera with ~1960 live
labels it was **19.4 ms**, and splitting it showed **44% of that was waiting on `labelMutex`** — the
loop took and released it once per label, ~1900 times, against a GL thread that holds it to build
label vertices. `BatchLock` (32 labels per acquisition, handed back around the sort) is most of the
win. The rest: two screen-aligned envelopes whose bounds intersect *do* overlap, so the
separating-axis test is skipped for them (`CullRecord::axisAligned` — exact, not an approximation,
and it covers every billboard label), and the two per-label heap allocations in the collection loop
became reused buffers.

Ruled out on the way: SAT was **not** the bottleneck — the fast path alone barely moved a style with
no anchors. And the batch size does not trade against frame time the way it looks like it should:
batch 8 and batch 32 measure the same worst frame (~46 ms against ~40 before). The worst-frame cost
is the culler doing the same work in a denser burst, not the mutex being held longer.

## Runtime switches (no rebuild)

`adb shell setprop debug.carto.<name> <value>` — `demtaps`, `groundpaint`, `tilebg`,
`areathreshold`, `areasourcedensity`, `linesourcedensity`, `depthshift`, `terrainpaint`,
`paintdetail`, `asyncdepthms`, `gputimer`. They are read **once per process**, so restart the app
after setting one, and **reset them when you are done** — they survive until reboot.
</content>
