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
  `app-debug.apk` and it is **stale**.

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

## Where the frame goes today

At `-O2`, on the north pan with content, the render thread has **no dominant leaf**: draw submission
(`renderTileGeometry` ~28% inclusive, ~156 draws/frame), `renderGeometry2D` ~33%,
`TileRenderer::prepareFrameUnsafe` ~10%, `GLTileRenderer::startFrame` ~9%,
`ElevationTextureCache::getTexture` + `resolveEntry` ~9%, hillshade layer ~6%.

The GPU is not the limit: `PROF GPU` with content puts the layers at 29–43 ms and the total at
38–53 ms against a CPU frame of 120–175 ms at `-O0`. **We are CPU-bound, on draw submission.**

So the lever is **fewer draws and fewer layers**, which is [07-hillshade-contours.md](07-hillshade-contours.md)
and [09-composite-layer.md](09-composite-layer.md), not micro-optimisation.

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

## Runtime switches (no rebuild)

`adb shell setprop debug.carto.<name> <value>` — `demtaps`, `groundpaint`, `tilebg`,
`areathreshold`, `areasourcedensity`, `linesourcedensity`, `depthshift`, `terrainpaint`,
`paintdetail`, `asyncdepthms`, `gputimer`. They are read **once per process**, so restart the app
after setting one, and **reset them when you are done** — they survive until reboot.
</content>
