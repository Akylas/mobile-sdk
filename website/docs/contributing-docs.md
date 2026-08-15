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
npm start          # dev server with hot reload at http://localhost:3000/MassifMaps/
npm run build      # production build into website/build
```

## Standalone pages and their data {#pages}

The non-doc pages (`/platforms`, `/roadmap`, `/sponsors`, `/community`, `/integrations`) are React
pages under `website/src/pages/`. Their content is **not** in the JSX — each reads a plain data
module in `website/src/data/`, so editing a page usually means editing one array:

| Page | Data file | Edit it to… |
|---|---|---|
| `/platforms` | `src/data/platforms.js` | change a platform's status (`supported` / `planned` / `legacy`) or a row of the feature matrix |
| `/sponsors` | `src/data/sponsors.js` | change tier prices, the contact address, or add a sponsor (logo in `static/img/sponsors/`) |
| `/community` | `src/data/community.js` | change the issue/discussion entry points and the repo list |
| `/integrations` | `src/data/integrations.js` | add a framework plugin |

Backticked spans inside those strings render as `<code>` via `src/components/Ticked.js` — no other
markdown is interpreted.

## Roadmap page (GitHub issues) {#roadmap}

`/roadmap` has no content of its own: `website/plugins/roadmap-issues/` fetches the issues labelled
**`roadmap`** in `massif-maps/MassifMaps` **at build time** and exposes them as plugin global data.
Each card takes the issue title, the first image in the body (markdown `![]()` or a raw `<img>`)
and a teaser of the remaining text.

Columns come from extra labels on the same issue, most-advanced first:

| Issue labels | Column |
|---|---|
| `roadmap` + `status:in-progress` | In progress |
| `roadmap` + `status:next` | Next up |
| `roadmap` alone | Exploring |
| `roadmap`, issue closed | Shipped |

Because the fetch happens at build time, the page only refreshes when the site is rebuilt — the
`schedule:` cron in `docs.yml` rebuilds it nightly. CI passes `GITHUB_TOKEN` to lift the anonymous
60 req/h rate limit; a local `npm run build` works without one. When the API cannot be reached the
build does **not** fail: it falls back to `src/data/roadmap-fallback.json` (refreshed on every
successful build) and the page shows a "may be out of date" banner.

Repo, label and column mapping are plugin options — override them in `docusaurus.config.js`:

```js
['./plugins/roadmap-issues', {owner: 'massif-maps', repo: 'MassifMaps', label: 'roadmap'}],
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
