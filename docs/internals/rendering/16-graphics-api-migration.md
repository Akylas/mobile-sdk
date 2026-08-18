---
title: Graphics API migration
description: Why the SDK stays on OpenGL ES and reaches Metal, D3D and Vulkan through ANGLE, and the phased plan to an ES 3.0 baseline.
sidebar_position: 16
---

# Graphics API migration

Scope: which graphics API the SDK targets, on which platform, and how it gets there. Covers the
move to an OpenGL ES 3.0 baseline (dropping ES 2.0), the Apple situation (Metal), and what
Windows/Linux/macOS need later. Does **not** cover what the renderer draws — that is the rest of
[this set](index.mdx).

Status as of 2026-08-18: investigation and plan. Nothing below Phase 0 has been executed.

## Where we are

Everything renders through **one** API surface: `GLES2/gl2.h` + `gl2ext.h`. Roughly 1730 GL call
sites over 114 distinct entry points.

| Where | Call sites |
|---|---|
| `all/native` | 898 |
| `libs-massif/vt` | 654 |
| `libs-massif/nml` | 177 |

Shaders are GLSL ES 1.00 (`#version 100`) throughout: 27 shader literals across 11 files in
`all/native/renderers/`, ~2000 lines in `libs-massif/vt/src/vt/GLTileRendererShaders.h`, plus
`nml/GLMaterial`.

An ES 3.0 **context** is already requested everywhere, with an ES 2.0 fallback:

| Platform | Context | Notes |
|---|---|---|
| Android | `MapView.java` queries `reqGlEsVersion`, asks ES3, falls back to 2 | minSdk 21; manifest still declares `glEsVersion 0x00020000` |
| iOS | `MapView.mm` asks ES3, falls back to 2 | EAGL by default; MetalANGLE behind `--use-metalangle` |
| Mac Catalyst | MetalANGLE only | `build-ios.py` refuses a Catalyst build without it |
| UWP | `EGLContextWrapper.cpp` hardcodes client version **2** | already ANGLE, on the D3D backend |

So ANGLE is already shipping on two of the platforms. `scripts/ios-dev` builds against MetalANGLE
too — the iOS simulator on Apple Silicon does not run Apple's GL, which is why the local bench
already goes through Metal.

`vt` already carries most of an ESSL 3.00 path: `GLTileRenderer::createShaderProgram` emits
`#version 300 es` plus `#define attribute in` / `varying out` / `texture2D texture`, fragment
shaders write `glFragColor` (a macro), and a failed 3.00 compile falls back per-program to 1.00
(`hasShaderVersionFallback()`). Today exactly one program uses it: the hardware-PCF shadow pass.
`all/native` has no equivalent.

## The decision — ANGLE, not a native backend

The forcing argument is a property of this codebase, not of Apple's deprecation notice.

**The shader system composes GLSL at runtime from app-supplied strings.** `buildShaderProgram`
splices `_fogShaderSource` into a placeholder, prepends a DEM prelude to the app's lighting shader,
and builds one program per (`flags` × `lightingMode` × `filterMode`) combination. Five public
setters feed application GLSL straight in:

| API | Module |
|---|---|
| `SkyOptions.ShaderSource` | `all/modules/components/SkyOptions.i` |
| `FogOptions.ShaderSource` | `all/modules/components/FogOptions.i` |
| `TerrainOptions.SurfaceShaderSource` | `all/modules/components/TerrainOptions.i` |
| `CustomRasterTileLayer.ShaderSource` | `all/modules/layers/CustomRasterTileLayer.i` |
| `PostProcessEffect` fragment source | `all/modules/renderers/PostProcessEffect.i` |

Those permutations cannot be precompiled offline — the app's contribution is a runtime string. A
native Metal/D3D/Vulkan backend would therefore have to **ship a GLSL front-end plus N dialect
back-ends inside the SDK binary** (glslang + SPIRV-Cross, several MB, and a permanent supply of
dialect bugs). That is what ANGLE already is. Writing it ourselves is rebuilding ANGLE worse.

### Cost

| | ANGLE | Native backends |
|---|---|---|
| Renderer source change | **none** of 1730 call sites | all of them, rewritten onto a device/encoder/pipeline API |
| Shaders | preamble macros (Phase 3) | one front-end, three dialects, **shipped and run at runtime** |
| Public GLSL API | survives unchanged | breaks, or needs that runtime translator anyway |
| Cost per new platform | one context/window file + a build script | a backend, a dialect, a windowing path |
| tangram-ng as reference | intact | **lost** — every render file forks from it |
| Binary | one static lib (measure it — see Phase 0) | a shader toolchain, minus the driver layer |

### Native workload, per platform, if we ever do it

| Platform | API | Backend | Dialect | Windowing |
|---|---|---|---|---|
| iOS | Metal | new | MSL | `CAMetalLayer` |
| macOS | Metal | shared with iOS | shared | `NSView` |
| Windows | D3D11/12 | new | HLSL | HWND + swapchain |
| Linux | Vulkan | new | SPIR-V | X11/Wayland |
| Android | GLES | keep | keep | keep |

Three backends, three dialects, a runtime cross-compiler, four windowing paths.

### ANGLE workload, per platform

| Platform | ANGLE backend | Work |
|---|---|---|
| iOS | Metal (ES 2.0 and 3.0 complete) | EGL bootstrap + `CAMetalLayer` view |
| macOS / Catalyst | Metal | same file, `NSView`. Catalyst is already ANGLE-only |
| Windows | D3D11 — ANGLE's oldest and most mature | EGL + HWND; the shape exists in `winphone/native/utils/EGLContextWrapper.cpp` |
| Linux | **none needed** — Mesa serves GLES 3.2 over EGL | EGL + X11/Wayland surface |
| Android | none — native GLES | nothing |

Linux is the quiet win: ANGLE is not a dependency there at all, and the same GLES 3.0 source runs.

## What the others did, and why it does not transfer

| Project | Approach | Cost |
|---|---|---|
| Mapbox | Native Metal. v10 added pluggable backends with 1:1 GL/Metal parity; v11 is **Metal-only** | Commercial team, full rendering rewrite; `mapbox-gl-native` archived Aug 2023 |
| MapLibre | Native Metal. Evaluated a MetalANGLE branch ("wasn't perfect but worked") and rejected it | ~5 engineers, over a year, **for Metal alone**, and only after a renderer modularization phase. Shipped iOS 6.0.0 in Jan 2024 |
| [tangram-ng](https://github.com/farfromrefug/tangram-ng) | Nothing. Pure GLES, EAGL on iOS | — |

Two of three went native, both with dedicated teams, both only after building a backend abstraction
this fork does not have. MapLibre's modularization later paid for Vulkan and WebGPU — that is the
real case for native, and it is a case for a team.

The reference implementation is a non-participant, which matters here more than elsewhere: a native
backend permanently ends the copy-from-tangram workflow that
[the rest of these pages](11-tangram-diff.md) depend on.

## ANGLE upstream, not MetalANGLE

`libs-external/angle-metal` is a prebuilt of `kakashidinho/metalangle` at commit `8ef9aba`,
originally vendored as `nutiteq/angle-metal`. That fork is dead by its author's own statement: most
of its ES 3.0 Metal implementation was merged into official ANGLE by June 2021, and the repo has
been inactive since he joined Google, with all development redirected upstream. Apple contributes
there too.

Upstream ANGLE's Metal backend is complete for **ES 2.0 and ES 3.0** on macOS and iOS (not
ES 3.1/3.2 — irrelevant here).

Porting cost is bounded and specific: the GL API surface is identical, so `all/native`, `vt` and
`nml` are untouched. What is **not** upstream is **MGLKit**, MetalANGLE's Objective-C lookalike for
Apple's deprecated EAGL/GLKit — which is exactly the `MSFGLContext` / `MSFGLKView` typedefs in
`ios/objc/ui/MapView.h` and the drawable-format block in `MapView.mm`. Those get rewritten onto
plain EGL + `CAMetalLayer`.

Two things to carry into the work:

- Apple's upstream contributions prioritised WebGL conformance over speed (rewriting index buffers
  on the fly for provoking-vertex rules, for one). Upstream may be **slower** than the fork on some
  paths. Bench it, do not assume it.
- Upstream drops the fork's ES2-downgrade patch and the armv7/i386 slices, both obsolete under the
  iOS 13.0 deployment floor.

## What ES 3.0 actually buys

Nothing lands from the version bump itself. The wins are the work it unblocks:

- **Instancing** — billboards, markers, celestial objects; one draw per batch instead of per quad.
- **Texture arrays** — shadow cascades are a `_size * _cascades` wide atlas today; an array removes
  the manual slice offsetting.
- **`glMapBufferRange` + orphaning** — label vertex streaming, currently the dominant 2D LINE cost.
- **Packed vertex attributes** (`GL_INT_2_10_10_10_REV`, normalized shorts) — vertex buffers roughly
  halve, and `MAX_VERTEXBUFFER_SIZE = 65535` with its 16-bit index cap can lift.
- **Uniform buffer objects** — the per-draw uniform storm in `GLTileRenderer`.
- **Core, no extension probe**: VAOs, `fwidth`, `sampler2DShadow`, `glInvalidateFramebuffer`,
  `glTexStorage2D`, MRT, `textureLod` in fragment shaders, guaranteed ETC2.

Caveat: tangram-ng is GLES-2-baseline, so unlike most of this documentation set, these have no
reference implementation to copy from.

## The plan

### Phase 0 — decision gates

Three measurements, no production code. Everything after this is conditional on them.

1. Build upstream ANGLE for `arm64-simulator`, run `scripts/ios-dev` against it. Confirms
   Metal-backend ES 3.0 in practice rather than from a support matrix.
2. Compile a `#version 300 es` shader containing `#define gl_FragColor TANGRAM_FragColor` through
   ANGLE's translator. See [the `gl_FragColor` disagreement](#the-gl_fragcolor-disagreement) below —
   this decides whether Phase 3 is a breaking change.
3. Frame-time A/B at a fixed bench camera: upstream ANGLE vs the vendored MetalANGLE vs EAGL.

**Gate**: if (1) fails or (3) is bad, this plan stops and the native question reopens.

### Phase 1 — ANGLE is the Apple graphics layer

- Re-vendor upstream ANGLE into `libs-external`. Slices: ios-arm64, ios-sim-arm64, catalyst
  arm64 + x86_64, macos arm64 + x86_64. Drop armv7 and i386.
- **One shared EGL bootstrap, parameterized by view class** — not `#ifdef`s inside `MapView.mm`.
  It will have four callers (UIKit, AppKit, Catalyst, later Win32), and MGLKit has to be replaced
  regardless; build it for that now.
- Delete: the EAGL path, `MSFGLContext`/`MSFGLKView`, `_MASSIF_USE_METALANGLE` and
  `--use-metalangle` (unconditional now), the `ios/glwrapper` OpenGLES fallbacks, and Xamarin
  (unmaintained; it is also the only binding that blocks an ANGLE-only iOS).
- `docs/maintenance/angle.md` with the exact build commands, versions and every fork patch.

**Done when**: `scripts/ios-dev` runs on device and simulator, Catalyst builds, and a screenshot at
a fixed camera matches the EAGL build.

### Phase 2 — ES 3.0 baseline, ES 2.0 dropped

Copy tangram's shape: one `Hardware::glVersion` parsed from `GL_VERSION`, every capability gated
off that number (`core/src/gl/hardware.cpp`).

- Android manifest to `0x00030000`; drop the `reqGlEsVersion` probe and `ConfigChooser`'s ES2
  branch. UWP client version 2 to 3.
- Collapse `GLContext`: NPOT, packed-depth-stencil and depth-texture become unconditional, the `ES3`
  flag disappears, `glDiscardFramebufferEXT` becomes `glInvalidateFramebuffer`.
- `vt/GLExtensions`: VAO and standard-derivatives probes become core calls;
  `GL_DEPTH_COMPONENT24_OES` becomes `GL_DEPTH_COMPONENT24`.

Devices lost: pre-2013 GPUs (Mali-400, Adreno 200/305, Tegra 3, PowerVR SGX). At minSdk 21 and an
iOS 13 floor — where every device is A7+ — that is a rounding error.

**Done when**: an Adreno 610 device and one iOS device show no regression at the bench cameras.

### Phase 3 — GLSL ES 3.00

The reference is already in the tree, and it is tangram, not mapbox-gl-js:
`core/src/gl/shaderSource.cpp` gates `#version 300 es` plus a vertex/fragment header on
`Hardware::glVersion >= 300`.

- Port that preamble into `all/native/renderers/utils/Shader.cpp`; flip `ESSL3_FLAG` on globally in
  `vt`; same for `nml`.
- Keep `vt`'s per-program 1.00 fallback for one release as the canary, then delete it.

**Done when**: every program compiles at `300 es` on both device families and
`hasShaderVersionFallback()` returns false.

#### The `gl_FragColor` disagreement

The two implementations contradict each other, and the answer decides whether this phase breaks the
public API.

- **tangram** does `#define gl_FragColor TANGRAM_FragColor` plus a `layout(location = 0) out`
  declaration. App shaders keep writing `gl_FragColor` and need no migration.
- **`vt`** renames instead, and its comment states the reason: a name starting with `gl_` cannot be
  `#define`d.

Tangram ships its form and it works on their device matrix — but tangram uses EAGL and has never run
under ANGLE, whose shader translator is the strict one. If ANGLE accepts the macro, the five public
GLSL setters need no migration and Phase 3 stops being a breaking change. That is Phase 0, item 2.

### Phase 4 — harvest

One measured PR each, recorded in [Performance log](../performance-log.md): instancing, then shadow
cascades as a texture array, then packed vertex attributes, then UBOs, then `glMapBufferRange` for
label streaming.

### Phase 5 — desktop

No renderer change. Per platform: one context/window file and one build script.

- **macOS native (AppKit)** — reuse the Phase 1 bootstrap, swap the view class.
- **Windows** — ANGLE on D3D11, reusing the UWP EGL wrapper shape.
- **Linux** — native EGL against Mesa's GLES 3.2. Pull in ANGLE-on-Vulkan only if a driver forces it.

## What this plan deliberately does not do

**No graphics abstraction layer.** MapLibre needed one because they were adding real backends; we
are not. Building one speculatively buys nothing until a native backend exists. What is worth
adopting is the cheaper discipline: new GL calls go through the `renderers/utils/` wrappers, not
into logic files. Retrofitting the 898 already-scattered calls in `all/native` is its own priced
job and is not part of this.

**No native Metal.** Reopen only if the Phase 0 bench shows ANGLE costing real frames at the city
camera — and price it against the table above, including the loss of tangram as a reference.

## Known gaps

- Every number in "Where we are" is from static analysis of the tree. Nothing in Phase 0 has been
  run; the ES 3.0 conformance of ANGLE's Metal backend is taken from its support matrix, not
  observed here.
- The binary-size delta of a linked upstream ANGLE is unmeasured. The vendored MetalANGLE static
  slice is 15.2 MB for arm64 before dead-stripping, which is not the shipped cost.
- Whether ANGLE adds measurable per-draw overhead versus EAGL is unknown, and it is the one result
  that could send this back to a native backend.
- Xamarin is assumed droppable. If it is not, it blocks an ANGLE-only iOS on its own.
- No decision on whether Windows should use ANGLE-on-D3D11 or ANGLE-on-Vulkan; D3D11 is assumed on
  maturity grounds only.
