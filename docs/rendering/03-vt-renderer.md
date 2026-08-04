# The GL vector-tile renderer

Scope: `libs-carto/vt/src/vt/GLTileRenderer.*` and its bridge `all/native/renderers/TileRenderer.*`.
Terrain specifics are in [04-terrain.md](04-terrain.md), depth in [05-depth-model.md](05-depth-model.md),
labels in [06-labels.md](06-labels.md).

## The bridge

`TileRenderer` (all/native) owns a `vt::GLTileRenderer` and, once per frame, pushes the state the
renderer cannot know by itself: view state, interaction mode, blending speeds, layer filters, and
the whole terrain block — elevation texture provider, regular-grid mode, painter order, depth shift,
slack scale, lighting, fog, shadow map, ground tiles and layer ordinals.

That "terrain state block" is also where an elevation version change is turned into work:
`invalidateTileSurfaces` / `invalidateLabelElevation` for the changed tiles, or a full
`resetTileSurfaces` (debounced by `SURFACE_RESET_DELAY`). Measured cost of the block during a pan:
**0.04 ms** — it is not the pan hang it was once believed to be.

## Per-frame phases

```
startFrame(dt)      blend render tiles, re-anchor labels whose elevation changed, blend labels
renderGeometry()    the 2D pass, then the 3D pass
renderLabels()      glyph quads, built fresh every frame, uploaded as batches
endFrame()          sweep compiled resources whose owners expired
```

`renderGeometry2D` (GLTileRenderer.cpp:2588) is the heart of it:

1. Bucket every visible render tile's layers by **style layer index** into `renderLayerMap`.
2. In terrain mode, sort each style layer's tiles **near to far** (content writes depth, so near
   tiles must occlude far ones). The distance is computed **once per tile** into a small map — doing
   it inside the comparator made the sort 21% of the render thread, because on the terrain
   transformer `calculateTileBBox` samples the elevation manager and transforms in double precision.
3. For each style layer, in order: resolve the layer's opacity/comp-op, set the per-layer depth
   ordinal, and draw each tile's geometry (`renderTileGeometry`).

### Render tiles

A *render tile* is a target tile id plus the source tile actually available for it (possibly an
ancestor), one `RenderTileLayer` per style layer, with `active` telling whether it is the live tile
or one retained for the cross-fade. `buildRenderTiles` merges the new set with what was on screen so
a tile can blend out rather than pop.

### What a draw costs

Measured on an Adreno 610: a 6-style-layer and a 21-style-layer style submit the same ~300k indices,
but 59 draws against 500, and cost **3 ms against 20**. The per-frame cost tracks the **draw count**,
not the triangle count. `RenderStats` therefore counts draws, indices, style layers, render tiles and
surface draws separately (`geomDraws=…` lines).

The counters split a draw into `geomProgramNs` (program selection + fog uniforms), `geomTerrainNs`
(MVP, depth bias, terrain/shadow uniforms), `geomStyleNs` (style parameter uploads) and
`geomStyleEvalNs` (the colour/width functions themselves), so a regression can be attributed without
guessing.

## Shaders

Programs are built on demand from source in `GLTileRendererShaders.h` and cached by a key made of
the shader kind plus a bitmask of feature flags (terrain, VTF, lighting, shadow, derivatives, fog,
paint surface, …). Consequences worth knowing:

- Every new flag combination is a **new program compile** on first use, on the render thread.
- A uniform the compiler drops has location `-1` from `glGetUniformLocation`, but
  `Shader::getUniformLoc` returns **0** — a valid location that then clobbers uniform 0. Always use
  `glGetUniformLocation` with a `>= 0` guard (`SkyRenderer` is the pattern to copy).

## Style evaluation

CartoCSS values may be functions of view state (zoom, nuti parameters), so colours, widths and
opacities are evaluated per frame per style layer through small caches keyed by the function plus
view state (`_colorFuncCache` and friends, with `styleFuncLookups`/`styleFuncMisses` counters).

## Interaction with the rest of the frame

- The renderer holds one mutex covering its tile/label state. The label placement worker holds it
  for the whole of `buildLabelMaps`, which is the only place the GL thread has been observed
  waiting on it (`mutexWaitNs`); during pans it has measured **0.00 ms**.
- `endFrame` sweeps every compiled-resource map (bitmaps, surfaces, geometries, label batches)
  looking for expired owners — cost tracked as `endFrameNs` / `endFrameSwept`.
- Client-side vertex arrays and bound VBOs are a cross-renderer hazard: the terrain paint pass once
  left `GL_ARRAY_BUFFER` bound and `SkyRenderer`, which draws from a client array, turned its quad
  into an offset into that buffer — the sky went black. **Unbind after every draw loop.**
</content>
