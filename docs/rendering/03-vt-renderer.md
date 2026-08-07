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

## Line joins

`TileLayerBuilder::tesselateLine` picks per vertex between a miter, a bevel, a round fan and a split,
from the dot product of consecutive binormals. Three things about it:

- **`line-miterlimit` is a ratio**, miter length over line width, and the stroke width must not enter
  it. It used to (`asin(min(width/limit, 1))` as the half-angle), which cut in two wrong directions at
  once: a 0.8-wide contour kept mitering into a needle five half-widths long, while any line wider than
  the limit never mitered at all. `dot = 2/limit² − 1` is the whole rule.
- **A sharp join must not overlap itself.** Two full-width quads meeting at a point overlap in a lens on
  the inside of the turn, and every pixel of that lens blends twice — which is what darkened a line with
  `line-opacity` at each hairpin. The inner corners are collapsed onto the centre line instead (mapbox's
  inner join) so the quads only touch; one triangle closes the outer gap. Offset lines keep the old
  overlapping split: their offset is `binormal × offset × side` in the shader, so a zero binormal would
  drop the offset entirely.
- **`line-join: round` builds tangram's 5-triangle fan** (`ROUND_JOIN_TRIANGLES`, their
  `JoinTypes::round`). Getting it right took three device rounds, all invisible in a syntax check:
  the fan vertices must sit **between** the two cross-sections (appended after them, the next segment
  links to fan vertices and the line comes apart); the hub must be on the **centre line**, not the miter
  point (that point is at zero alpha in the antialias ramp); and two extra triangles must close the
  sliver against each quad's end chord, which runs from its outer corner to the miter point and so does
  not pass through the hub. Winding mirrors with the turn direction — 2D geometry is drawn with back-face
  culling on, and a fan wound one way loses every join that turns the other way. Below ~10° the fan is
  skipped for a plain miter: tangram fans at every angle, but their line shader has no AA ramp and five
  near-degenerate slivers each carrying one is a visible seam.

## Lines over terrain

A line is a chain of quads whose width is an offset along a per-vertex binormal, and three things
about that offset are easy to get wrong. All three were, and the symptoms all looked like terrain or
depth bugs rather than line bugs.

**Clip every line fragment to its own tile.** Lines are built with a clip buffer of **an eighth of a
tile** (`TileLayerBuilder.h`, `_clipBox` = −0.125…1.125; polygons only 0.002), so every tile carries
a long stretch of its neighbours' roads and draws it — displaced with *its own* target tile's
elevation texture and lattice, which is a different DEM level than the tile that overflow actually
lies on. The same road is then painted twice at two different heights: from straight down the copies
coincide and it looks perfect, and the moment the camera tilts they separate. That tilt-only
signature is the tell. The stencil tile masks were what used to clip this, but they need a stencil
buffer and the shared-ground target has none (`GL_STENCIL_BITS` reads **0**), so they never run.
`lineFsh` therefore discards outside the tile, using `uTileUnitScale` (vertex-frame units → TARGET
tile units, set in `setupTerrainUniforms`; **0 means no elevation**, which disables the test) and a
`vTileUnit` varying. No attachment, no extra draw.

**Width: tangram's model, capped at nominal.** The offset is extruded in model space and displaced
onto the terrain, exactly as tangram does (`polyline.vs` + `res/scenes/terrain-3d.yaml`), so a line
is a world quad through the projection and tapers with distance. Left unbounded it also *grows*
towards the camera until a near contour is a blob, so the projected offset is measured and **shrunk**
back to the nominal width when it exceeds it. The factor is ≤ 1 by construction — it can never
manufacture an oversized quad, which is what an unbounded screen-space fit does.

**Antialias in device pixels, from a per-frame constant.** The ramp is one unit of the quad, and a
unit is not a pixel: widths are unscaled-DPI units, so at 2.6× density one unit is ~1.8 device px and
a 1 px contour is almost entirely ramp — that is what "blurry contours" was. `uAntialiasScale`
(screen height ÷ normalized resolution, set by `TileRenderer`) makes the ramp one device pixel;
measured on a contour at z17, the edge transition went 5 px → 2 px and the solid core 4 px → 8 px.
`GL_OES_standard_derivatives` is **not** exposed on this context, so an `fwidth`-based ramp is dead
code — check by forcing `a = 0` in that branch and seeing whether the line still draws.

> **Never measure a line's screen width per vertex against `roundedWidth`.** A `screenHalfWidth /
> roundedWidth` varying is in the *tile's* units, and the terrain LOD picks tiles up to
> `MaxTileZoomCoarsening` (default 3) levels below the camera zoom on a grazing slope. There the
> ratio is wrong by the zoom difference, the antialias ramp becomes a hard cut, and lines break into
> fat wedges and detached triangles. For the same reason, never divide the offset by
> `centerClip.w + deltaClip.w`: that is a second perspective divide by something that is not a
> position's w, and it explodes when the offset is large in world units.

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
