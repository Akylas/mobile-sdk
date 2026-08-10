# iOS development environment

`scripts/ios-dev` is the iOS counterpart of [`scripts/android-dev`](../android-dev): one demo app
that exercises the SDK, with the SDK itself built as a dependency of the app rather than consumed
as a prebuilt framework. Touch a `.cpp`, hit build, and only that file recompiles — the same loop
gradle gives on Android.

## Setup

```sh
./bootstrap.sh              # arm64 simulator (the usual dev target)
./bootstrap.sh device       # arm64 device
PROFILE=lite ./bootstrap.sh # a different feature profile
```

That regenerates the Objective-C bindings, configures the SDK with CMake's Xcode generator, and
writes `CartoDemo.xcodeproj` with [XcodeGen](https://github.com/yonaskolb/XcodeGen) (`brew install
xcodegen`). Re-run it only when the profile or the platform changes; day to day just build:

```sh
xcodebuild -project CartoDemo.xcodeproj -scheme CartoDemo -sdk iphonesimulator build
```

`CartoDemo.xcodeproj`, `Info.plist`, `.sdkproj` and `.angle` are all generated and gitignored.
`project.yml` is the source of truth.

## Configuring a run

Android takes its knobs as intent extras; iOS takes them as launch arguments, which UIKit folds
into `NSUserDefaults`. The key names are deliberately identical, so a camera or a layer set reads
the same for both demos:

```sh
adb shell am start -n com.akylas.cartotest/.MainActivity --es zoom 14 --es hillshade true
xcrun simctl launch <device> com.akylas.CartoDemo -zoom 14 -hillshade true
```

Supported today: `base`, `satellite`, `hillshade`, `terrain`, `lon`, `lat`, `zoom`, `tilt`,
`rotation`, `exaggeration`, `meshResolution`, `rasterUrl`, `demUrl`, `demEncoding`.

## Structure

Mirrors the Android demo file for file, so the two can be compared knob by knob:

| iOS | Android | Role |
|---|---|---|
| `DemoConfig.h/.m` | `demo/DemoConfig.java` | every default + the launch-argument key map |
| `DemoCfg.h/.m` | `demo/DemoCfg.java` | typed readers for the overrides |
| `DemoMap.h/.m` | `demo/DemoMap.java` | layer registry, tile sources, terrain, sky/light, camera |
| `DemoStyles.h/.m` | `demo/DemoStyles.java` | generated CartoCSS + the style decoders |
| `DemoPanel.h/.m` | `demo/DemoPanel.java` | the on-screen settings panel |
| `DemoTests.h/.m` | `demo/DemoTests.java` | one-shot actions (route, search, clear) |
| `DemoViewController.m` | `ui/main/SecondFragment.java` | platform glue only |

One deliberate difference: `DemoConfig.java` is ~230 static fields, each hand-mapped to an intent
extra. Here the values live in a dictionary keyed by the **same names**, so the override pass is
automatic and adding a knob is one line instead of four. `DemoPanel` is likewise table-driven
rather than a hand-built layout - the Java panel is 1200 lines mostly because every control is
written out.

## Adding a source file

`./regen.sh` — it strips the legacy elements CMake re-emits and re-runs XcodeGen. Needed because
`xcodegen generate` on its own fails after any CMake reconfigure, and fails *silently* in the
sense that the project keeps its old file list.

## Status

Covered: vector and raster base maps (composite or plain), the generated inline CartoCSS with its
label / road-width / 3D-building / landcover knobs, the composite hillshade and satellite slots,
the stand-alone hillshade layer, contours both on-the-fly and pre-baked, 3D terrain, sun and sky,
the camera, the settings panel and the route/search/clear actions.

Not ported yet: the day cycle and celestial objects (`DemoSky`, `DemoStars`), free-roam and
peak-finder modes, the maneuver-arrow gallery, the GeoJSON benchmarks, and the vector-tile search
service behind the search action (which currently only reports the query point).

## Gotchas

- **Positions must go through the map's own projection.** `[[mapView getOptions] getBaseProjection]`
  is EPSG3857; converting with `NTEPSG4326` instead compiles, looks right, and silently feeds
  lon/lat to the map as metres, which puts the camera in the ocean off 0,0.
- `bootstrap.sh` strips `PBXBuildStyle` from the CMake-generated project. CMake still emits those
  Xcode 2 vestiges and XcodeGen's parser refuses to read a project containing them.
- The app links `libc++` and MetalANGLE explicitly: its own sources are all Objective-C, so nothing
  would otherwise pull in the C++ runtime, and a project reference does not carry CMake's
  `target_link_libraries` through to the app.
- One architecture at a time — `bootstrap.sh` configures the SDK for a single arch and the app is
  pinned to match.
