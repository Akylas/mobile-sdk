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
| `RenderStats` | draws, indices, render tiles, style layers, surfaces, label and prep timings, tile-surface builds | per one-second interval, deltas — **divide by the `PROF` frame count** of that interval, a faster build prints bigger counters |
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
- **`RenderStats` counters are sums over the one-second interval, not per-frame values.** A build
  that renders the same scene FASTER therefore prints MORE draws and MORE indices, because it got
  through more frames. Always divide by the frame count of the same interval — the `PROF` line
  right next to it starts with `%d frames in %.0f ms`. Comparing two arms on the raw
  `geomIndices=` cost a wrong conclusion in August 2026 (the faster arm looked like it was
  submitting more geometry).
- **A static camera never settles here.** With a rich style, a parked camera swings 9–24 fps for
  minutes (tile arrival, drape bakes, label placement, elevation fetches), so "leave it still and
  read the number" is not a measurement. Drive a scripted move — `--es anim pan` — for anything you
  intend to believe; it also makes the two arms traverse the same tiles.
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

### A city pans slower than a mountain, and it is fragments

Same build, same camera settings (z16.22, tilt 26), panning north — Grenoble against the
Saint-Eynard ridge, on a Crosscall:

| | fps | CPU frame | GPU total | GPU layers | draws/frame | indices/frame |
|---|---|---|---|---|---|---|
| city | 12.0–13.7 | 34 ms | 33 ms | **21 ms** | 48 | 1.65M |
| mountain | 18.2–19.6 | 31 ms | 18 ms | **9 ms** | 35–48 | 1.3–1.6M |

The CPU frame is the same and the draw/index counts match in the closest pair, so the 2.4× is
**per-fragment**: dense city content covers the whole screen where the mountain view is mostly
terrain surface. Of it, **contours are 45%** — `--es contour false` takes the city from 12.1 to
17.6 fps (repeated, interleaved), GPU layers 21.3 → 13.6 ms, render tiles 805 → 380 and draws
602 → 430 per interval, because the `#contour` slot is a second tile set drawn over the first.

## Starting up in terrain mode

Measured on a Crosscall at the demo's default camera (Grenoble, z16.22, terrain + contours, warm
caches), with temporary probes in `TileLayer::FetchTaskBase::run`, `PersistentCacheTileDataSource::
loadTile`, `ElevationManager::loadTileGrid` and `VectorTileLayer::FetchTask::loadTile`. The vector
tiles were never the cost: 66 decodes, 5–6 s of thread time. Elevation was, three times over.

| per startup | before | after |
|---|---|---|
| DEM HTTP requests | 79–94, of which **15 could only fail** | **0–1** |
| DEM grid decodes | 1525 loads of 167 distinct tiles (32 s) | 157 of 157 (3.1 s) |
| 90% of the content on screen | 6.4–7.6 s | **4.3–4.4 s** |

1. **The elevation grid LRU held 85 tiles and the view needed 167** — a byte budget behaving as a
   tile budget the DEM raster size decides. Fixed in [04-terrain.md](04-terrain.md#elevation-data).
2. **The on-disk tile cache defaults to 50 MB and one terrain view's DEM pyramid does not fit.**
   Two consecutive starts at the *same* camera missed on **different** tiles (their miss sets did
   not intersect): the cache was evicting exactly what the next start needed, so ~65 tiles were
   re-downloaded every time at 400–800 ms each. This is an app-side setting —
   `PersistentCacheTileDataSource::setCapacity`; the demo now asks for 600 MB for the DEM and
   200 MB per other source (`DemoConfig.DEM_PERSISTENT_CACHE_MB`, `--es demCacheMb`).
3. **The element elevation warm-up sampled the view envelope**, whose corners are off the DEM in
   terrain mode: 15 guaranteed 404s per startup. See
   [12-vector-elements.md](12-vector-elements.md#terrain-interaction).

What is left, in order: **contour tile generation** (44 child fetches, 6–22 s of thread time — the
`#contour` slot is a whole second tile set, which is what [07-hillshade-contours.md](07-hillshade-contours.md)
would remove by computing contours in the terrain fragment shader), the network itself, and the
decode pool (`Options::setTileThreadPoolSize`, default **1**; tangram runs 2). Raising the pool was
measured as no win *while* the network dominated — retake it now that it does not.

Unrelated but on the same path: the first `loadTile` on a persistent cache runs `loadTileInfo`, a
full-table scan of the cache DB, on the tile thread — **1.4 s** on the first (cold page cache) run
of a 52 MB DB.

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

A third round attacked what was then the biggest item, `buildLayerAttachment`, for a paired
**175.4 → 117.4 ms** on the same device and style project (three cold runs each, spread under
1.5 ms; the absolute numbers are higher than the round above because the bundled style has grown
since — only the pair means anything). Its inner loop runs **115,756** times for `transportation`
alone, and the counters said where:

| what the iteration did | share | what replaces it |
|---|---|---|
| "does this set already set this field?" | 71% | one bit test on a per-set field bitmask |
| filter intersect rejects the merge | 24% | one AND against a per-predicate "disjoint" mask |
| reaches the redundancy (cover) test | 5% | one AND against a per-predicate "contains" mask |

The field test is the interesting one: it used to scan the set's whole property list and compare
specificities. `compileLayer` sorts the properties by **decreasing specificity**, so a set that
already has the field got it from a property that outranks the current one — the answer is always
"skip", and one bit answers it. That order is now load-bearing; the comment at the sort says so.
Two smaller items: a merge that cannot succeed is rejected *before* the trial set is copied, and
`buildPropertyLists` evaluated each property expression once per attachment it appears in rather
than once per property.

Both masks are 256 bits with a fallback to the scans they replace (the biggest layer here has ~50
fields and ~80 predicates).

**How this was measured, and how to redo it:** `libs-carto/cartocss/test/CompileBench.cpp` builds
on the host (the command is in its header), loads a style project the way `CartoCSSMapLoader` does
and times `compileLayer` per layer. With `CSSBENCH_DUMP=<file>` it dumps every compiled property
set, so `diff` proves a compiler change kept the output identical — all five bundled projects
(osm/streets/outdoors/eink/ign) dump identically across this round. The host is ~9× faster than the
device but splits the same way per layer, so it is a valid *guide*; the ms in this page are always
device numbers.

Left on the table, per the host section timers after this round (all 23 layers, one run):
`buildLayerAttachment` **4.8 ms**, the stylesheet walk in `buildPropertyLists` **2.8 ms**, the
expression evaluation **0.7 ms** (2.6 before the memo), the per-zoom filter evaluation **0.9 ms**.
The walk is the structural one: it goes over the **whole** stylesheet once per layer (23 × 407
elements here) and only then discards what the layer predicate rules out. Sharing one
`FilteredPropertyState` across a project's layers would fix it, and that is an API change to the
compiler rather than a local one.

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

### The compiled map is cached

A compiled `mvt::Map` is read-only, and a decoder's parameter values live in its own store, so the
same map can serve several decoders. `MBVectorTileDecoder` keeps a small process-wide cache keyed by
**(asset package, style asset name, `cartoCSSLayerNamesIgnored`)** — weak references plus the last
two held strongly, so a day/night pair stays warm without pinning every style ever loaded. Measured
on the device, loading the same style a second time: **411 ms → 0.00 ms** (the symbolizer context is
already cached by asset package too, so the second load is free end to end).

The key is the asset **package object**, not its contents: two styles of one package (the day/night
case, `CompiledStyleSet(pack, "day")` / `(pack, "night")`) hit the cache, but re-creating the package
around the same files does not. Hashing the assets to do better would cost more than it saves for
the single-load case.

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

### Selection: the appearance half, without a decode

A selection is a parameter compared with a feature field — `[nuti::selected_id] = [osmid] + ''` —
which the classification above rejects, because the comparison can only be answered per feature. The
**appearance** half of it no longer needs a decode either, for a style that asks:

```json
"nutiparameters": { "selected_id": { "default": "", "selects": true } }
```

Opt-in on purpose. It only works for a style written a particular way, so inferring it would make
every other style pay a walk over its rules to be told no, and would leave an author whose style
just misses the conditions with no way to find out. `resolveSelectionParameter` returns before
touching anything when no parameter declares itself, and warns with the reason when a declared one
does not qualify.

A geometry already carries up to 16 style slots, whose colour and width are uniform arrays refilled
every frame, and every vertex names its slot in one byte of `aVertexAttribs`. So the decoder folds
the comparison BOTH ways: it builds the symbolizer twice, once with the parameter forced to the
feature's own value and once to a value it cannot equal, and both answers land as two slots of the
same geometry. Nothing else about the feature changes, so it is tesselated once.

- `mvt::resolveSelectionParameter` verifies the declared parameter at style load and marks the
  properties it may fold (`Property::setSelectionFoldable`). A folded property reads no parameter, so
  it collapses to a constant — which is what makes it a slot, and what lets every unselected feature
  share one.
- `ExpressionContext::setNutiParameterOverride` is how the fold is forced; `TileReader::createSelectionFeatureProcessor`
  runs the branch that is not drawn over an EMPTY feature collection, so it registers its slot without
  laying down vertices.
- Each feature keeps a 64-bit `hashValue` of what it is compared with, next to the vertex run
  (`vt::TileGeometry::FeatureStyleRange`). `MBVectorTileDecoder` publishes the hash of the parameter
  in a shared atomic; `GLTileRenderer` compares the two in `buildCompiledTileGeometry`, rewrites the
  style byte of the runs that changed and re-uploads exactly those bytes with `glBufferSubData`.

Deciding it on the render thread rather than walking the tile cache is what keeps it free of locks:
the vertex data is only ever touched where it is uploaded, and a tile decoded later picks the state
up on its own.

Conservative, because a fold that got the tesselation wrong could not be undone by a repaint. The
parameter has to be read only by the `stroke`, `stroke-opacity` and `stroke-width` of line
symbolizers — the three that end up as slots and touch no vertex — always inside an `=` against the
same field expression, never in a rule filter, never beside another parameter in one property. A
dashed line whose width is selected is refused as well: the dash raster is sized by the width, so the
two branches would not share their vertices.

Measured with the demo's selection bench (`--es routeSelect true --es routeSelectCycle 2500`,
12 routes, z12.5, `--es tilt 90`), with a temporary `decodeTile` probe. `value` mode goes from 6
`decodeTile` calls per selection to **zero**, on the emulator and on the Crosscall alike, and
`setStyleParameter` from 2.2-3.6 ms to **0.32-0.81 ms** on the device (0.08-0.39 ms on the emulator).
`filter` mode still decodes its 6 tiles per selection, as it must, and logs `it is read by a rule
filter, which decides whether the geometry exists at all`. The selected route changes colour and
width in the next frame in both, and the 23-layer base-map style is unaffected - it declares no
selecting parameter, so its rules are never walked.

**What is still a decode: the structural half.** `when ([nuti::selected_id] = [osmid] + '')::selected`
decides whether the casing geometry exists at all, and no repaint can build geometry. A style that
wants a free selection has to express the casing as appearance — a width and a colour that fold —
rather than as a rule. The durable answer for the general case is maplibre's `feature-state` model:
the selected id becomes a **uniform** compared per vertex against a feature-id attribute, which needs
a feature-id vertex attribute in `vt` and shader support — not done.

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
| CartoCSS compile: field and predicate bitmasks on the property set, evaluate each property once | compile 175.4 → 117.4 ms paired (same style, later and bigger than the rows above) |
| live style parameters (a colour-only parameter swaps values and redraws) | a parameter change went from *visible tiles × ~130 ms* of decode to **zero decodes** |
| compiled-map cache keyed by asset package + style name | loading the same style again: 411 ms → **0.00 ms** |
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
