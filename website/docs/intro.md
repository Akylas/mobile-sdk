---
title: Introduction
sidebar_position: 1
slug: /intro
---

# CARTO Mobile SDK

A **maintained fork** of the original CARTO Mobile SDK ([CartoDB/mobile-sdk](https://github.com/CartoDB/mobile-sdk)), kept alive and extended by [Akylas](https://github.com/Akylas/mobile-sdk).

CARTO Mobile SDK is an open, multi-platform framework for visualizing maps and providing
location-based services on mobile devices — smartphones and tablets. It ships a high-performance,
flexible vector-tile renderer, multiple built-in routing engines (street and indoor), plus
built-in geocoding and reverse geocoding.

![CARTO Mobile SDK](/img/carto-mobile-sdk-animated.gif)

## Why this fork?

CARTO stopped maintaining the original SDK. This fork continues it and adds many features on
top of the original 4.x API, while keeping the same `com.carto.*` public API namespace so
existing code keeps working. Highlights added by the fork:

- ⛰️ **[3D Terrain](/docs/features/3d-terrain)** — real elevation with render-to-texture fill draping and correct depth occlusion.
- 〰️ **[On-the-fly contour lines](/docs/features/contours)** — generated directly from RGB elevation tiles, plus GPU shader contours.
- 🎚️ **[Composite Vector Tile Layer](/docs/features/composite-vector-tile-layer)** — weave external raster / hillshade / vector sources into one CartoCSS style.
- 🌄 **[Advanced hillshade](/docs/features/hillshade)** and **[custom raster shaders](/docs/features/custom-raster-shaders)**.
- 🧩 **[PMTiles](/docs/features/pmtiles)** support (local and HTTP).
- 🌅 **[Sky, sun and shadows](/docs/features/sky-sun-shadows)** — one directional light and a shader sky shared by ground, terrain and fog.
- 🌙 **[Objects in the sky](/docs/features/celestial-objects)** — sun, moon, stars or an aircraft, plus a free-roam camera that can look up at them.
- 🖌️ **[Post-processing effects](/docs/features/post-processing)** — full-screen shaders with access to the terrain depth (the peak-finder look).
- 🏷️ **[Shields, font icons and callout labels](/docs/features/label-styling)** — names that take the free side of their icon, SDF icon glyphs, plates, panorama callouts.
- 🎨 **[Live style parameters](/docs/features/style-parameters)** — change a colour, or the selected feature, without re-decoding a single tile.
- 📐 **[GeoJSON vector tiling](/docs/features/geojson-vector-tiles)** — app data tiled through a geojson-vt pyramid and styled with CartoCSS.
- ➡️ **[Navigation maneuver arrows](/docs/features/maneuver-arrows)** — cut from the route, drawn as a line with an arrow head.
- 🧱 **[MapLibre Tiles (MLT)](/docs/features/maplibre-tiles)** — the columnar vector-tile format, read by the same decoder and styled the same way.
- 🧭 Embedded **Valhalla** routing, exposed as a standalone routing library too.

## What's in the box

- Supports **Android, iOS and UWP** from a single C++ core.
- Multiple languages: **Java / Kotlin / C#** on Android, **Objective-C / Swift / C#** on iOS, **C#** on UWP.
- Open GIS formats: **GeoJSON, Mapbox Vector Tiles, MBTiles, PMTiles, TMS**.
- High-level styling via **[CartoCSS](https://carto.com/developers/styling/cartocss/)**.
- **Globe** and **planar** map modes, plus 2.5D tilted views.
- Routing and geocoding connectors for internal and third-party services.
- **Offline** packages for maps, routing and geocoding.

## Where to next

| I want to… | Go to |
|---|---|
| Add the SDK to my app | **[Installation](/docs/getting-started/installation)** |
| Show my first map | **[Your first map](/docs/getting-started/your-first-map)** |
| Learn the core concepts | **[Guides](/docs/guides/map-view)** |
| Use a new fork feature | **[Features](/docs/features/3d-terrain)** |
| Browse the classes | **[API Reference](/docs/api-reference)** |

:::info Original documentation
This site mirrors the structure of the [original CARTO Mobile SDK docs](https://cartodb.github.io/developers/mobile-sdk/).
Conceptual guides carried over from there still apply — only installation and the fork-specific
features differ.
:::
