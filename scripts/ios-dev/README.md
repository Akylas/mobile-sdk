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
| `DemoMap.h/.m` | `demo/DemoMap.java` | layer registry, tile sources, terrain, camera |
| `DemoViewController.m` | `ui/main/SecondFragment.java` | platform glue only |

## Status

The project, the build wiring and the config plumbing are complete. The demo itself is a **first
slice**: raster base map, hillshade, 3D terrain and the camera. The Android demo is ~7,500 lines,
and the rest is still to port — vector base map and styles (`DemoStyles`), the on-screen settings
panel (`DemoPanel`), sky/day-cycle (`DemoSky`), stars (`DemoStars`), and the one-shot routing /
search / GeoJSON actions (`DemoTests`).

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
