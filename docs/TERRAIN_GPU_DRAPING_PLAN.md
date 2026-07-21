# Terrain v2: GPU draping (tangram-ng translation plan)

Reference implementation: https://github.com/styluslabs/tangram-ng (local clone: `/Volumes/dev/carto/tangram-ng`).
Goal: replace the CPU-displacement + depth-bias terrain architecture on `feature/3d-terrain`
with tangram's GPU-draping architecture, which renders terrain without artifacts.

## Why tangram has no artifacts (verified in source)

1. **One source of truth, on the GPU.** Elevation tiles are uploaded as textures
   (terrarium RGBA8 or float). Every draped vertex — the terrain grid, polygons,
   lines — runs `position.z += decode(texture2D(u_rasters[N], uv))` in the vertex
   shader (`core/shaders/polygon.vs`, `polyline.vs`, injected `position` block; see
   `res/scenes/terrain-3d.yaml`). All layers therefore agree on heights *bit-exactly*.
   There is no depth pre-pass for rendering, no polygon offsets, no slope-scaled
   push/pull, no geometry clamps — the entire class of bugs we fought in rounds 3–17
   does not exist by construction.
2. **Layer separation is explicit, not geometric.** Coincident draped layers are
   separated by a fixed clip-space delta per draw order:
   `gl_Position.z += (proxy - layer) * (TANGRAM_DEPTH_DELTA * gl_Position.w + depth_shift)`
   with `TANGRAM_DEPTH_DELTA = 2^-19` (24-bit depth) — `shaderSource.cpp:74`,
   `polygon.vs:154`. Proxy (parent/child stand-in) tiles get pushed behind live tiles
   the same way (`proxy *= 48.0` for the terrain surface style).
3. **Crack-free LOD by construction.** When a tile is finer than the elevation data,
   the shader samples the *parent* elevation texture with scaled/offset UVs
   (`style.cpp:261-269`). Neighbours sharing a parent sample one continuous texture,
   so edges match exactly. No skirts, no edge stitching, no per-tile grid resampling.
4. **Terrain surface is trivial.** A fixed 64×64 grid per raster tile
   (`rasterStyle.cpp:58-90`), displaced by the same shader block. Hillshading normals
   are computed in the *fragment* shader from the elevation texture (3×3 taps).
5. **No terrain subdivision of vector geometry.** Features keep their source vertex
   density; between vertices geometry interpolates linearly and can deviate slightly
   from the 64-grid surface. In practice invisible because heights come from the same
   smooth texture and the per-layer delta breaks ties.
6. **Labels are screen-space overlays.** The anchor gets elevation on the CPU
   (bilinear from the elevation texture data, `elevationManager.cpp:81-105`), is
   projected once, and glyphs are laid out in screen pixels (SDF, ¼-px quantization)
   — always sharp, size never affected by terrain. Occlusion + terrain picking +
   camera terrain-following use an async ¼-resolution terrain depth pass rendered to
   an offscreen R32UI buffer and read back with one frame of lag
   (`elevationManager.cpp:191-283`, `view.cpp:548-575`).
7. **GL requirement: vertex texture fetch** (GLES3-class). Tangram has no ES2
   fallback; terrain is simply a scene option.

Smoothness (independent of terrain, for later phases): geometry is built once on
worker threads with zoom-derivatives baked into vertex attributes (widths scale in
the shader — zero rebuilds while zooming); proxy tiles from an LRU cache fill every
hole; LOD selection is by projected screen area (terrain-aware); uploads are lazy.

## Translation to carto mobile-sdk / libs-carto vt

### Phase 0 — GL capability gate
- Android: request an ES3 context (ES2 API remains valid on it); iOS MetalANGLE
  supports ES3. At runtime require `MAX_VERTEX_TEXTURE_IMAGE_UNITS > 0` for terrain
  mode; otherwise terrain falls back to flat (current non-terrain path).
- Decision: drop the CPU-displacement path once GPU draping works (it is the source
  of the artifact class, and maintaining both doubles the surface).

### Phase 1 — GPU draping core (libs-carto vt + terrain glue)
1. **Elevation textures.** ElevationManager (or TileRenderer glue) keeps a GL texture
   cache keyed by DEM tile id. Upload decoded grids re-encoded as terrarium RGBA8
   (decode is linear in R,G,B, so bilinear filtering commutes with decoding — safe on
   all hardware, no float-texture extensions needed).
2. **Per-tile UV transform.** For each drawn tile, resolve the best available DEM
   texture (exact tile or ancestor — same logic as ElevationManager::getTileGrid) and
   compute `(uvOffset, uvScale)` exactly like tangram's overzoom path.
3. **Shader changes** (GLTileRendererShaders.h): in terrain mode all 2D vertex
   shaders (background, bitmap, line, point, polygon) add
   `pos.z += uTerrainScale * decodeTerrarium(texture2D(uElevationTex, uv))` where
   `uv = tilePos * uElevationUVScale + uElevationUVOffset`. Tile-local position is
   already available in these shaders. Validate with scripts/validate-vt-shaders.py.
4. **Flat tile surfaces + grid.** TileSurfaceBuilder keeps a fixed N×N subdivision in
   terrain mode (transformer subdivision without height sampling — heights move to
   the GPU). TerrainTileTransformer's calculatePoint returns z=0 again; its
   tesselation thresholds stay (they define the grid).
5. **Depth model.** Remove: terrain depth pre-pass usage for rendering, designated
   depth-write layer, all glPolygonOffset calls, uDepthBias, the surface-fan clamp.
   Add: per-layer constant clip-space delta as in tangram (layer index is already
   available per TileLayer; proxy equivalent = carto's non-active blend tiles).
   Opaque draped content depth-tests and the bottom layer writes depth as before —
   but now every layer's heights are identical by construction.
6. **Keep CPU heights** (ElevationManager grids) for: hit testing, camera,
   getElevation API, vector element (VectorLayer) placement via
   TerrainProjectionSurface, label anchor heights, min/max for culling. Unchanged.
7. **Labels**: keep current CPU anchor elevation + the new view-depth scale
   correction; occlusion can stay ray-march initially, later switch to tangram-style
   async depth readback (also enables camera terrain clearance).

### Phase 2 — LOD/prefetch polish
- Blend/retained tiles get the tangram `proxy`-style depth push instead of stencil
  ordering heuristics; verify LOD transitions.
- Optional: screen-area-based tile selection for terrain (carto currently uses
  w-distance of flat tile centers).

### Phase 3 — smoothness & sharpness (separate track, needs profiling first)
- Compare carto's per-frame CPU work (style function evaluation, label transforms)
  against tangram's; consider zoom-derivative attributes if line-width evaluation
  shows up in profiles.
- Text sharpness: compare carto SDF glyph rasterization sizes/filtering vs alfons
  (3 fixed SDF sizes, 6px radius, two-pass stroke+fill, ¼-px screen quantization).

## Risks / open questions
- MetalANGLE VTF path on iOS must be verified on device early (phase 0 spike).
- Terrarium re-encode adds one copy per DEM tile upload (cheap; grids are already
  decoded once).
- Buildings/3D polygons: tangram displaces them by the same texture (base follows
  terrain); carto POLYGON3D pass needs the same treatment.
- Raster (hillshade) layers: their surfaces get the same shader displacement — the
  hillshade bitmap remains a plain raster draped like everything else.
