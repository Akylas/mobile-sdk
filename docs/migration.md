---
title: Migrating from the CARTO Mobile SDK
description: Every namespace, package, class prefix and style token renamed when the CARTO Mobile SDK became Massif Maps — and the ones deliberately left alone.
sidebar_position: 8
slug: /migration
---

# Migrating from the CARTO Mobile SDK

Massif Maps **is** the CARTO Mobile SDK, forked and kept alive after CARTO stopped maintaining it.
The 4.x concepts, class shapes and CartoCSS styles all still apply — what changed in 5.0 is the
name, in every namespace, package and debug token.

Every rename below is a **hard break** except the CartoCSS ones, which keep parsing the old spelling
and log a deprecation warning once per stylesheet.

:::tip Most apps are a find-and-replace
An Android app that only used `com.carto.*` classes migrates with one search-and-replace and a
dependency line. The part with no automatic path is the CARTO **online services**, which the fork
removed entirely — see [What was removed](#what-was-removed).
:::

## Do this first

**1 · Change the dependency.**

```gradle
// before
implementation 'com.carto:carto-mobile-sdk:4.4.+'
// after — JitPack overrides the declared groupId, so the coordinate is the GitHub path
implementation 'com.github.massif-maps:MassifMaps-android-aar:5.0.0'
```

On iOS the CocoaPod is replaced by a Swift package:
`https://github.com/massif-maps/MassifMaps-ios-swift`.

**2 · Rename the imports.**

```bash
# Android / Java / Kotlin
grep -rl 'com\.carto\.' src/ | xargs sed -i '' 's/com\.carto\./com.massifmaps./g'

# iOS / Obj-C / Swift — class prefix NT -> MSF
grep -rl 'NT[A-Z]' Sources/ | xargs sed -i '' -E 's/\bNT([A-Z][A-Za-z0-9]*)/MSF\1/g'
```

**3 · Replace anything that talked to CARTO's servers** — see
[What was removed](#what-was-removed).

**4 · Styles need no change.** `nuti::` still parses. Rename it when convenient.

## Bindings

| Platform | Before | After |
|---|---|---|
| Java / Kotlin | `com.carto.*` | `com.massifmaps.*` |
| Java (routing-lib) | `com.akylas.routing.*` | `com.massifmaps.routing.*` |
| Objective-C / Swift | `NTMapView`, `NTVectorTileLayer`, … | `MSFMapView`, `MSFVectorTileLayer`, … |
| .NET | `Carto.Ui`, `Carto.Layers`, … | `Massif.Ui`, `Massif.Layers`, … |
| Native library | `carto_mobile_sdk` (`libcarto_mobile_sdk.so`) | `massif` (`libmassif.so`) |
| Gradle artifact | `com.carto:carto-mobile-sdk` | `com.massifmaps:massif` |

On JitPack the resolvable coordinate is `com.github.massif-maps:MassifMaps-android-aar:<tag>` — JitPack
overrides the declared `groupId`, so the package name and the coordinate differ by design.

## C++ (only if you build from source or embed the core)

| Before | After |
|---|---|
| `carto::` | `massif::` |
| `_CARTO_*_SUPPORT` build defines | `_MASSIF_*_SUPPORT` |
| `CARTO_VT_RENDER_STATS`, `CARTO_FRAME_PROFILER` | `MASSIF_VT_RENDER_STATS`, `MASSIF_FRAME_PROFILER` |
| `mvt::NutiParameter*` | `mvt::StyleParameter*` |
| `CartoGeocodingProxy` | `MassifGeocodingProxy` |

## CartoCSS — the old spelling still works

Both spellings parse. `CartoCSSMapLoader` logs one warning per deprecated token per stylesheet; the
old spelling will be removed in a later release.

| Before | After | What it means |
|---|---|---|
| `[nuti::x]` | `[param::x]` | app-supplied runtime parameter, joining `mapnik::` and `view::` |
| `"nutiparameters"` (project.json) | `"styleparameters"` | the block declaring those parameters |
| `text-placement: nutibillboard` | `billboard` | fully screen-aligned |
| `text-placement: nutibillboardline` | `billboard-line` | screen-aligned, laid along a line |
| `text-placement: nutipoint` | `flat` | flat in the placement plane, no camera facing |
| `text-placement: nuticallout` | `callout` | screen-aligned with a leader line |

`point` (upright on the ground normal, swivelling to face the camera) and `line` are unchanged.
See [Live style parameters](/docs/features/style-parameters) for what `param::` can do now.

## Debug knobs

| Before | After |
|---|---|
| logcat tag `carto-mobile-sdk` | `massif` |
| `adb shell setprop debug.carto.*` | `debug.massif.*` |
| demo app `com.akylas.cartotest` | `com.massifmaps.MassifDemo` |
| demo `--es style nuti` | `--es style project` (`--es demo nuti` still accepted) |
| demo `--es nutiInterval` | `--es paramInterval` |

## What was removed {#what-was-removed}

The fork dropped everything that depended on CARTO's hosted services — they were switched off with
the product. There is **no renamed equivalent**; bring your own source.

| Gone | Replace with |
|---|---|
| `CartoOnlineVectorTileLayer` and the hosted basemap | any `TileDataSource` + a CartoCSS style — see [Layers & data sources](/docs/guides/layers-and-data-sources) |
| CARTO offline map packages and their `PackageManager` endpoints | your own package server, [MBTiles](/docs/guides/offline-maps) or [PMTiles](/docs/features/pmtiles) |
| CARTO hosted routing and geocoding endpoints | the embedded Valhalla / SGRE engines, or any HTTP service |
| CARTO API keys and the mobile app registration flow | nothing — there is no key to set |

Guides carried over from the CARTO documentation that still use these classes carry a warning
banner at the top.

## Deliberately NOT renamed

These name data or upstream work, not this SDK:

- **On-disk formats.** `NUTi` is the 4-byte magic of the compressed bitmap format; `nutikeysha1` is
  a row in the `metadata` table of downloaded offline packages; `.nutigraph` / `.nutigeodb` are
  package file extensions; `__Nuti_pkgmgr_` is a local filename prefix. Renaming any of them would
  orphan data users already have on disk.
- **`cartodb_id`**, an MVT feature field.
- **CartoCSS** — the style language, MapBox's and CARTO's, which this SDK implements rather than
  owns.
- **The CartoDB copyright headers and LICENSE attribution.**
- **`tiles.akylas.fr` URLs, the HTTP user agent and CI addresses** — infrastructure, not branding.
