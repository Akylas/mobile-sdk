# Terrain Phase D — painter-order depth model (drop the surface occluder)

Branch: `feature/terrain-painter-order`. Builds on the shipped regular-grid + lattice
work (main `6bca2c889`, libs-carto develop `7accc7f`).

Reference: tangram-ng (`/Volumes/dev/carto/tangram-ng`), which has no terrain artifacts
by construction. This plan ports its depth model.

## Why Phase D

Phases A–C gave us the shared regular-grid surface (no per-tile red-green tesselation)
and lattice-clamped vt geometry (near-zero slack). Two problems remain, and both trace to
one root cause — CARTO uses the **terrain surface as a depth pre-pass occluder** that all
draped content is depth-tested against:

1. **Zero geometry subdivision is unsafe** (Phase C had to be walked back). An
   un-subdivided fill is a few flat triangles that sag below the bulging grid surface over
   convex terrain and get depth-occluded → landcover holes. So draped vt geometry must
   still be subdivided to the grid — we don't get tangram's decode-time win.
2. **VectorLayer elements see through ridges** (route lines, every config, pre-existing).
   Elements follow the fine height field while the occluder is the coarse render-LOD grid;
   they deviate, so they need a distance-proportional forward slack, and that slack is
   exactly what lets a far element poke in front of a near ridge. Elements are not tiled,
   so they can't lattice-clamp to the per-tile render LOD — there is no static fix.

Both dissolve if the surface stops being a same-layer occluder. Tangram never depth-tests
content against the surface: the surface is just the **bottom painter layer**, and all
draped content (surface, fills, lines, elements) is separated by a fixed **per-layer
clip-space delta**. Heights come from the same elevation field, so nothing needs slack;
ridge occlusion comes from the true per-fragment depth of the drawn surface.

## Tangram model (verified in source)

Every draped vertex, in the vertex shader (`core/shaders/polygon.vs:107-154`,
`res/scenes/terrain-3d.yaml:47-56`):

```glsl
float proxy = u_proxy_depth;             // parent/child stand-in tiles
float depth_shift = -0.02 * u_proj[2][3];// larger delta near camera
#ifdef TANGRAM_RASTER_STYLE
    proxy *= 48.0;                       // push the raster (ground) surface well back
#endif
position.z += TANGRAM_TERRAIN_SCALE * getElevation();   // GPU draping, same field for all
...
gl_Position = u_proj * u_view * position;
gl_Position.z += (proxy - layer) * (TANGRAM_DEPTH_DELTA * gl_Position.w + depth_shift);
// TANGRAM_DEPTH_DELTA = 2^-19 (24-bit depth)  [shaderSource.cpp:74]
```

Key points:
- `layer` = draw order (raster style: `u_order`; feature styles: `a_position.w`).
- Depth test is ordinary `LEQUAL` with depth writes on; **no** pre-pass, **no** polygon
  offset, **no** slack. Coincident layers are separated purely by `(proxy - layer)*DELTA`.
- The ground raster surface writes depth pushed back by `proxy*48`, so live feature layers
  always win over it and over proxy (LOD stand-in) tiles.
- Ridge occlusion is automatic: the near tile's surface writes real depth densely; far
  content behind it fails LEQUAL.

## Current CARTO model (what we replace)

`GLTileRenderer.cpp`:
- Pre-pass (`renderGeometry`, ~441-472): per tile layer `glClear(GL_DEPTH_BUFFER_BIT)`,
  `renderTileSurfaceFill` writes the surface depth as the occluder.
- Per style layer (`renderGeometry2D`, ~1514-1560): `contentDepthWrite` content
  depth-tests + writes against the pre-pass; backgrounds/bitmaps bias `_terrainDepthBias`,
  geometry adds a forward slack (`_terrainDrawDepthClipUnits`, 2 in regular-grid, 12
  adaptive) via `applyDepthBias` (`uDepthBias*w + uDepthBiasClip`). Proxy (retained) tiles
  pushed back one `TERRAIN_LAYER_DEPTH_DELTA` (2^-19).
- CPU fallback: `glPolygonOffset`.
- `setTerrainSlackScale`, `uDepthBias`, `uDepthBiasClip` — the slack machinery.

`VectorLayer.cpp` (~173-224): element renderers get `elementDepthBias = 2*2^-19` +
distance-proportional `elementDepthBiasClip`; elements draped on CPU via
`TerrainProjectionSurface` (fine DEM).

## Target design

Gate behind a new opt-in so the shipped occluder model stays the default fallback:
`TerrainOptions.PainterOrderDepth` (working name), plumbed like `RegularGridEnabled`.
Painter-order **implies** regular-grid (needs a depth-writing ground surface); enabling it
forces the shared grid path.

### Phase D.1 — vt painter-order depth (libs-carto)

1. **One depth domain.** Stop clearing depth per tile layer for the occluder. Clear depth
   once at the start of the terrain 2D pass. All draped content shares it.
2. **Surface = bottom painter layer.** Draw the shared grid surface first, writing depth,
   pushed back by the raster-proxy analog (`proxy*48`-style constant). It is the ground +
   the ridge occluder, but as a **layer**, not a tested-against occluder.
3. **Per-layer delta in the shader.** Replace `applyDepthBias` with the tangram formula:
   `gl_Position.z += (proxy - layer) * (DELTA * gl_Position.w + depth_shift)`, with
   `DELTA = TERRAIN_LAYER_DEPTH_DELTA` (already 2^-19), `depth_shift = -k*proj(2,3)` (tune
   `k`, tangram uses 0.02). Feed `layer` (style-layer draw order, already available per
   `RenderTileLayer`) and `proxy` (retained/active + parent/child stand-in) as uniforms.
4. **Drop the slack.** Remove `uDepthBias`/`uDepthBiasClip` usage, `setTerrainSlackScale`,
   `_terrainDrawDepthClipUnits`, and the geometry forward slack. Content no longer sags
   below an occluder, so no slack is needed. Keep `applyTerrain`/lattice as-is (heights
   still identical across layers).
5. **No geometry subdivision (reclaim Phase C).** With no occluder, un-subdivided fills no
   longer get depth-occluded — set `divideThreshold = infinity` again in
   `TerrainTileTransformer` for painter-order mode. This is the decode-time perf win.
6. **Remove `glPolygonOffset`** from the VTF path; keep only for the true CPU fallback
   (non-terrain).
7. **Retire the color pre-pass / background-color fill** into the surface bottom layer.

### Phase D.2 — GPU-drape VectorLayer elements (all/native)

Elements must sample the same field and stack by the same delta, or they re-introduce the
slack. Because elements are **not tiled**, they need a regional elevation texture.

1. **Regional elevation texture.** Build one texture covering the visible extent by
   stitching the cached DEM tiles (reuse `ElevationTextureCache` machinery; either an atlas
   or a single re-projected texture with a world→uv transform). Rebuild when the view /
   elevation version changes.
2. **Shader draping in element renderers.** `LineRenderer`, `PointRenderer`,
   `PolygonRenderer`, `GeometryCollectionRenderer`: bind the regional elevation texture and
   add `pos.z += decode(sample(worldToUv(pos)))` in the vertex shader (mirror
   `applyTerrain`; bilinear, deterministic). Elements then follow the exact same field as
   vt geometry — no CPU `TerrainProjectionSurface` height baking needed for draping.
3. **Per-layer delta for elements.** Add the same `(proxy - layer) * (DELTA*w + shift)`
   term, with `layer` placing elements above vt tile content (they render after tiles).
   Replace `elementDepthBias`/`elementDepthBiasClip` (`VectorLayer.cpp:190-216`) with it.
4. **Keep CPU heights** for hit-testing / label anchors / camera (unchanged —
   `TerrainProjectionSurface` stays for those).

### Phase D.3 — labels, buildings, overlays

- Labels: still screen-space; occlusion via the async depth readback — verify it reads the
  new painter-order depth correctly (it should; the surface still writes depth).
- `polygon3D` buildings: already `applyTerrain(base)`; give them the same per-layer delta;
  base follows terrain, extrusion writes depth normally.
- Comp-op / overlay-buffer layers (no depth attachment today) — confirm they still composite
  correctly; they bypass terrain occlusion as before.

## Precision plan

24-bit depth, `DELTA = 2^-19`. Budget: N style layers + proxy levels must fit without
z-fighting and without far-leak. `depth_shift` gives near-camera layers extra separation
where precision is worst. Tangram runs this exact budget on mobile — start with its
constants (`DELTA = 2^-19`, `depth_shift = -0.02*proj(2,3)`, `proxy*48` on the ground
surface) and tune on device.

## Risks

- Rewrites the depth loop fought over ~56 rounds. Highest-regression area. **Gate it.**
- Ridge occlusion now depends on the surface writing depth as the bottom layer + correct
  proxy push; getting the ordering/proxy wrong = far content leaks through gaps.
- New precision minefield: many layers within 2^-19; z-fight vs far-leak trade-off.
- Regional element texture: memory/upload cost, seams at its edges, re-projection accuracy.
- Two depth models to maintain behind the flag (renderer complexity).
- Device-only validation; expect several iterations.

## Rollout (incremental, device checkpoint per step)

1. D.1 vt painter-order behind `PainterOrderDepth`, keep geometry subdivided first
   (isolate the depth-model change from the subdivision change). Validate: no see-through,
   no z-fight, ridge occlusion crisp, road colors not washed.
2. Flip `divideThreshold = infinity` (reclaim Phase C). Validate: no landcover holes.
3. D.2 element GPU-draping + delta. Validate: route line no longer sees through ridges;
   elements don't vanish; hit-testing still works.
4. D.3 buildings/labels/overlays pass. Remove dead slack code once stable.

## Validation cameras (Martin, planar, osm.zip)

- 45.245940 / 5.761129, z14.67 t35 (landcover holes regression check).
- 45.232494 / 5.770689, z13.80 t30 (route-line see-through check).
- 45.242440 / 45.244172, z11/t60 sweep (occlusion, mist, line, road colors).

## Definition of done

`PainterOrderDepth` on: zero geometry subdivision, no `uDepthBias`/`uDepthBiasClip`/
`glPolygonOffset`/`setTerrainSlackScale` in the VTF path, route lines and fills occlude
correctly with no see-through and no vanishing, at all test cameras — matching tangram.
