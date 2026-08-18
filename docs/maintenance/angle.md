---
title: Rebuilding the ANGLE slices
description: How to rebuild the vendored MetalANGLE static libraries for iOS, the simulator and Mac Catalyst, and the fork patches that must survive.
---

# Rebuilding the ANGLE slices

`libs-external/angle-metal` is a **nested submodule** (`massif-maps/angle-metal`) that contains
prebuilt binaries only — one `libangle.a` per architecture plus the headers. There is no ANGLE
source in it, so "updating ANGLE" means building the `.a` files elsewhere and dropping them in.

Why MetalANGLE rather than upstream `google/angle`, and what it costs, is in
[Graphics API migration](../internals/rendering/16-graphics-api-migration.md). The short version:
MetalANGLE ships an Xcode project, so this page is `xcodebuild` and nothing else — upstream would
need depot_tools, gclient and a ~10 GB sync.

Verified 2026-08-18 with Xcode 26.5 (17F42), `iPhoneSimulator26.5.sdk`, on macOS 15 / arm64,
building `kakashidinho/metalangle` master `ec925142e`.

## Layout

| Path | Contents |
|---|---|
| `libs-external/angle-metal/<arch>/libangle.a` | the static slice; `<arch>` is `arm64`, `arm64-simulator`, `arm64-maccatalyst`, `x86_64`, `x86_64-maccatalyst` |
| `libs-external/angle-metal/include/` | `EGL/ GLES/ GLES2/ GLES3/ KHR/`, `angle_gl.h`, `export.h`, and the `MGL*.h` MGLKit headers |

`scripts/ios-dev/bootstrap.sh` symlinks `.angle` at one of those arch directories and
`project.yml` links `$(SRCROOT)/.angle/libangle.a`, so switching slice is a symlink swap and a
relink — no reconfigure.

## Build

```sh
git clone https://github.com/kakashidinho/metalangle.git
cd metalangle/ios/xcode
./fetchDependencies.sh
```

`fetchDependencies.sh` clones four pinned revisions (glslang, SPIRV-Cross, jsoncpp and its source)
from the chromium.googlesource.com mirrors into `third_party/`. It only clones; nothing is executed.

Then one `xcodebuild` per slice. The settings after `build` are **not optional** — they are what
turns a 446 MB archive into the ~14 MB slice that ships, and what makes the symbols link from a
static library into dependent projects:

```sh
xcodebuild -project OpenGLES.xcodeproj -target MetalANGLE_static \
  -configuration Release -arch arm64 -sdk iphonesimulator \
  IPHONEOS_DEPLOYMENT_TARGET=13.0 \
  ONLY_ACTIVE_ARCH=YES \
  DEPLOYMENT_POSTPROCESSING=YES STRIP_INSTALLED_PRODUCT=YES STRIP_STYLE=non-global \
  GENERATE_MASTER_OBJECT_FILE=YES KEEP_PRIVATE_EXTERNS=NO \
  GCC_SYMBOLS_PRIVATE_EXTERN=NO GCC_INLINES_ARE_PRIVATE_EXTERN=YES \
  GCC_PREPROCESSOR_DEFINITIONS='$(inherited) ANGLE_PLATFORM_EXPORT= ANGLE_EXPORT= ANGLE_UTIL_EXPORT=' \
  GCC_ENABLE_CPP_EXCEPTIONS=YES GCC_ENABLE_CPP_RTTI=YES GCC_ENABLE_OBJC_EXCEPTIONS=YES \
  build
```

Output: `build/Release-iphonesimulator/libMetalANGLE_static.a` → copy to
`libs-external/angle-metal/arm64-simulator/libangle.a` (note the rename).

Per slice, change `-sdk` and `-arch`:

| Slice | `-sdk` | `-arch` | Output dir |
|---|---|---|---|
| `arm64-simulator` | `iphonesimulator` | `arm64` | `Release-iphonesimulator` |
| `arm64` | `iphoneos` | `arm64` | `Release-iphoneos` |
| `x86_64` | `iphonesimulator` | `x86_64` | `Release-iphonesimulator` |

Mac Catalyst uses the scheme rather than the target, and is then split with `lipo`:

```sh
xcodebuild -project OpenGLES.xcodeproj -scheme MetalANGLE_static -sdk macosx \
  -configuration Release -destination 'platform=macOS,variant=Mac Catalyst' \
  build SUPPORTS_MACCATALYST=YES
lipo <output>/libMetalANGLE_static.a -extract arm64  -output libangle.a   # -> arm64-maccatalyst/
lipo <output>/libMetalANGLE_static.a -extract x86_64 -output libangle.a   # -> x86_64-maccatalyst/
```

See [Mac Catalyst](mac-catalyst.md) for why those slices behave like a macOS build at link time.

`armv7` and `i386` slices still exist in the submodule and are dead — the deployment floor is
iOS 13.0. Do not rebuild them.

### Why each strip setting is there

Without them the archive keeps every object's debug info and every private symbol:
**446 MB unstripped vs 14.2 MB with them**, for the same code. `GENERATE_MASTER_OBJECT_FILE`
(single-object prelink) plus `STRIP_STYLE=non-global` is the pair that does most of it.
`GCC_SYMBOLS_PRIVATE_EXTERN=NO` must stay `NO` or the SDK cannot see the GL entry points.
The three empty `*_EXPORT=` macros neutralise ANGLE's dllexport attributes, which are meaningless
in a static library and produce visibility warnings otherwise.

C++/ObjC exceptions must be **on**: they are off by default for this project, and a build with them
off breaks exception propagation in *dependent* projects, not in ANGLE itself.

## Fork patches that must survive an update

Only one, and it is in the headers rather than the source:

- **`include/MGLContext.h`** — the `#include "EGL/egl.h"` is commented out and `eglDisplay` is typed
  `void*` instead of `EGLDisplay`, so that including `MGLContext.h` does not drag EGL's headers into
  the SDK's Objective-C surface. Re-apply after copying headers from a new MetalANGLE checkout; the
  rest of `include/` is byte-identical to the upstream tree.

The `angle-metal` README also documents an ES2-downgrade patch (`kEAGLRenderingAPIOpenGLES3` →
`...ES2` in `DisplayEAGL.mm`) for 32-bit devices. **Do not re-apply it** — it caps the context at
ES 2.0, which is the opposite of where the SDK is going, and the devices it was for are below the
deployment floor.

## Checking the result

Launch anything that creates a map and read the two startup lines:

```
GLContext::LoadExtensions: OpenGL ES 3.0.0 (ANGLE 2.1.0.ec925142edeb), depth texture 1, shadow samplers 0
MapRenderer::onSurfaceCreated: renderer 'ANGLE (Metal Renderer: Apple iOS simulator GPU)', depth bits 24, stencil bits 8
```

The **commit suffix on the ANGLE version is how you tell slices apart** — the 2021 vendored build
reports a bare `ANGLE 2.1.0.` with nothing after the last dot. If you swapped the `.a` and the
suffix did not change, the link picked up the old one.

`scripts/ios-dev` is the fastest way to exercise it:

```sh
cd scripts/ios-dev && ./bootstrap.sh
xcodebuild -project MassifDemo.xcodeproj -scheme MassifDemo -sdk iphonesimulator \
  -destination 'id=<simulator-udid>' build
```

`-destination` needs the UDID from `xcrun simctl list devices available`; the by-name form fails
with *"no available devices matched the request"* even when the name is right.

## Known gaps

- Only `arm64-simulator` has actually been rebuilt at master. The device, x86_64 and Catalyst slices
  in the submodule are still the 2021 `8ef9aba` builds.
- No device run: ES 3.0 on the Metal backend is confirmed on the simulator only.
- MetalANGLE's own README grades its ES 3.0 at 90% — primitive restart and last-provoking-vertex
  flat shading are unimplemented. Not yet checked against what this renderer draws.
