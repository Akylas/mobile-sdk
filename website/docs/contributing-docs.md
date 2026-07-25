---
title: Building the Docs
sidebar_position: 6
slug: /contributing-docs
---

# Building the documentation

This documentation site lives in [`website/`](https://github.com/Akylas/mobile-sdk/tree/master/website)
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
[`scripts/docs/convert-guides.py`](https://github.com/Akylas/mobile-sdk/blob/master/scripts/docs/convert-guides.py)
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

Feature pages reference images under `website/static/img/features/`. Placeholders ship in the repo;
replace them with real captures using the Android demo app:

```bash
# Boot an emulator / connect a device, then:
scripts/docs/capture-screenshots.sh
```

The script builds the `scripts/android-dev` demo (`assembleDebug`), installs it, and pulls
screenshots via `adb`. Because feature captures (terrain, contours, hillshade) need map data
(`osm.zip` style + a DEM) present on the device, review the script header and set the paths for
your data before running. Drop the resulting PNGs/MP4s into `website/static/img/features/`.

## Deployment

A GitHub Actions workflow
([`.github/workflows/docs.yml`](https://github.com/Akylas/mobile-sdk/blob/master/.github/workflows/docs.yml))
builds the site + API reference and deploys to GitHub Pages on every push to `master` and on every
published release. See [Release workflow](/docs/release-workflow).
