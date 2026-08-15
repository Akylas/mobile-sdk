# Migrating to Massif Maps

> Published version: https://massif-maps.github.io/MassifMaps/migration

The SDK was renamed from **CARTO Mobile SDK** to **Massif Maps** — CARTO no longer maintains it,
and the fork had carried a dead brand in every namespace, package and style token.

Every rename below is a **hard break** except the CartoCSS ones, which keep parsing the old
spelling and log a deprecation warning once per stylesheet.

## C++

| Before | After |
|---|---|
| `carto::` | `massif::` |
| `_MASSIF_*_SUPPORT` build defines | `_MASSIF_*_SUPPORT` |
| `CARTO_VT_RENDER_STATS`, `CARTO_FRAME_PROFILER` | `MASSIF_VT_RENDER_STATS`, `MASSIF_FRAME_PROFILER` |
| `mvt::NutiParameter*` | `mvt::StyleParameter*` |
| `CartoGeocodingProxy` | `MassifGeocodingProxy` |

## Bindings

| Platform | Before | After |
|---|---|---|
| Java | `com.carto.*` | `com.massifmaps.*` |
| Java (routing-lib) | `com.akylas.routing.*` | `com.massifmaps.routing.*` |
| ObjC | `NTMapView`, `NTVectorTileLayer`, … | `MSFMapView`, `MSFVectorTileLayer`, … |
| .NET | `Carto.Ui`, `Carto.Layers`, … | `Massif.Ui`, `Massif.Layers`, … |
| Native library | `carto_mobile_sdk` (`libcarto_mobile_sdk.so`) | `massif` (`libmassif.so`) |
| Gradle artifact | `com.carto:carto-mobile-sdk` | `com.massifmaps:massif` |

On JitPack the resolvable coordinate is `com.github.massif-maps:MassifMaps:<tag>` — JitPack overrides
the declared `groupId`, so the package name and the coordinate differ by design.

## CartoCSS — old spelling still accepted

Both spellings parse. `CartoCSSMapLoader` logs one warning per deprecated token per stylesheet;
the old spelling will be removed in a later release.

| Before | After | What it means |
|---|---|---|
| `[nuti::x]` | `[param::x]` | app-supplied runtime parameter, joining `mapnik::` and `view::` |
| `"nutiparameters"` (project.json) | `"styleparameters"` | the block declaring those parameters |
| `text-placement: nutibillboard` | `billboard` | fully screen-aligned |
| `text-placement: nutibillboardline` | `billboard-line` | screen-aligned, laid along a line |
| `text-placement: nutipoint` | `flat` | flat in the placement plane, no camera facing |
| `text-placement: nuticallout` | `callout` | screen-aligned with a leader line |

`point` (upright on the ground normal, swivelling to face the camera) and `line` are unchanged.

## Deliberately NOT renamed

These name data or upstream work, not this SDK:

- **On-disk formats.** `NUTi` is the 4-byte magic of the compressed bitmap format; `nutikeysha1`
  is a row in the `metadata` table of downloaded offline packages; `.nutigraph` / `.nutigeodb`
  are package file extensions; `__Nuti_pkgmgr_` is a local filename prefix. Renaming any of them
  would orphan data users already have on disk.
- **`cartodb_id`**, an MVT feature field.
- **CartoCSS** — the style language, MapBox and CARTO's, which this SDK implements rather than owns.
- **The CartoDB copyright headers and LICENSE attribution.**
- **`tiles.akylas.fr` URLs, the HTTP user agent and CI addresses** — infrastructure, not branding.

## Debug knobs

| Before | After |
|---|---|
| logcat tag `carto-mobile-sdk` | `massif` |
| `adb shell setprop debug.massif.*` | `debug.massif.*` |
| demo app `com.massifmaps.test` | `com.massifmaps.test` |
| demo `--es style nuti` | `--es style project` (`--es demo nuti` still accepted) |
| demo `--es nutiInterval` | `--es paramInterval` |
