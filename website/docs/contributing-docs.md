---
title: Building the Docs
sidebar_position: 7
slug: /contributing-docs
---

# Building the documentation

This documentation site lives in [`website/`](https://github.com/massif-maps/MassifMaps/tree/master/website)
and is built with [Docusaurus](https://docusaurus.io/). Everything needed to build it — content,
API-reference generation and screenshot capture — is in the repo.

## Run the site locally

```bash
cd website
npm install
npm start          # dev server with hot reload at http://localhost:3000/mobile-sdk/
npm run build      # production build into website/build
```

## Migrated guides

The conceptual guides under **Guides** are converted from the original CARTO Jekyll docs by
[`scripts/docs/convert-guides.py`](https://github.com/massif-maps/MassifMaps/blob/master/scripts/docs/convert-guides.py)
(Liquid → CommonMark). Re-run it if the source `docs/guides/*.md` change:

```bash
python3 scripts/docs/convert-guides.py
```

## API reference (Javadoc + Jazzy) {#api}

The per-language API reference is generated from the SWIG bindings, then dropped into
`website/static/api/{android,ios}` so it is served under `/api/...`:

```bash
# Android Javadoc  ->  website/static/api/android
scripts/docs/gen-api-android.sh

# iOS Jazzy        ->  website/static/api/ios
scripts/docs/gen-api-ios.sh
```

Both scripts first run the SWIG proxy generators (`swigpp-java.py` / `swigpp-objc.py`) to produce
the language sources, then run `javadoc` / `jazzy`. They need the SWIG fork and (for iOS) a macOS
host with `jazzy` installed — the CI workflow sets these up automatically. See each script's header
for prerequisites.

## Screenshots & videos {#screenshots}

Feature pages reference images under `website/static/img/features/`. The terrain / hillshade /
contour shots and the pan video there were captured from the `scripts/android-dev` demo:

```bash
# Boot an emulator / connect a device, then:
scripts/docs/capture-screenshots.sh terrain-hero      # a still
RECORD=1 scripts/docs/capture-screenshots.sh terrain  # still + ~14s video
```

The demo streams its terrain data from public online tiles (a terrarium DEM +
an OpenFreeMap vector basemap), so the emulator only needs internet — no map data is pushed to
the device. The native libraries are prebuilt under `scripts/android-dev/massif/`, so the
app builds in seconds. The script builds (`assembleDebug --offline`), installs, launches, grabs a
screenshot (and optionally a screen recording), then uses `ffmpeg` to crop the Android status/nav
bars and encode a web-friendly JPEG/MP4. Drop the results into `website/static/img/features/`.

For distinct shots (top-down hillshade, close-up contours, a low-angle 3D view), edit the demo's
`SecondFragment` camera (`setFocusPos` / `setZoom` / `setTilt` — Massif tilt is `90` = top-down,
low = horizon) and comment out `addTerrainControls` to hide the debug UI, then restore it.

## Deployment

A GitHub Actions workflow
([`.github/workflows/docs.yml`](https://github.com/massif-maps/MassifMaps/blob/master/.github/workflows/docs.yml))
builds the site + API reference and deploys to GitHub Pages on every push to `master` and on every
published release. See [Release workflow](/docs/release-workflow).
