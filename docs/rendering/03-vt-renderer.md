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
- **A sharp join is still a round join.** Turns sharper than a right angle leave the bevel/fan
  branch for the split branch above, and closing that corner with the single triangle cuts it flat —
  the join reads as *square*. It only shows on geometry sparse enough for a turn to pass 90°, which
  is why simplifying a route surfaced it, and it comes and goes with zoom because which vertices
  survive simplification is decided per tile zoom. The same fan runs in both branches.
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

## Translucent layers: paint every pixel once

A style layer that is translucent must not blend a pixel twice, and no tesselation can guarantee
that: a line whose vertices sit closer together than it is wide folds at every join, a route doubles
back inside its own width, and a retained tile cross-fades over the tile that replaced it. Every
overlap blends again, which reads as darker knots along the line.

`renderGeometry2D` therefore runs a translucent layer as a **single-blend** pass: the top stencil
bit (`SINGLE_BLEND_STENCIL_BIT`) is cleared for the layer, `glStencilOp(KEEP, KEEP, GL_INVERT)`
marks each pixel as it is painted, and the `GL_EQUAL` test rejects the second fragment. One bit, one
masked clear per layer, no extra geometry and no extra pass.

- It engages **only where the artifact can exist**: layer opacity below 1, or any evaluated
  `colorFuncs[i]` alpha below 1. An opaque layer cannot show it, and there a later fragment
  legitimately covers an earlier one.
- A `comp-op` layer is excluded — it already composites once through its own buffer.
- The masks and the paint bit are **separate questions**. The tile masks are dropped in terrain mode
  and under a shared ground; single blend needs no masks, only a spare bit, so it reads the real
  stencil size (`maskStencilBits` vs `stencilBits`). Tying it to the masks is how it silently did
  nothing in exactly the configuration — 3D terrain — where it was wanted.
- The masks occupy the low bits, so it stands down past 128 target tiles in a frame.

**The trade-off is antialias seams.** The first fragment to reach a pixel owns it, and if that
fragment was a partial-coverage edge pixel the neighbour can no longer fill it in — faint lighter
hairlines along internal join boundaries. Where that is unacceptable the alternative is the layer's
own `opacity` + `comp-op`, which draws the layer opaque into the overlay buffer and composites it
once: no seams, but a full-screen pass per layer, and that buffer carries no depth, so in 3D terrain
the layer stops being occluded by ridges. Measured on an Adreno 610, the demo route (casing + fill,
translucent, scripted pan, two reps of 25 one-second samples): **37.7 fps single-blend against 26.5
fps through the overlay buffer**, `layers` 2.43 vs 3.62 ms. Single blend is the default for that
reason.

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
`lineFsh` therefore discards outside the tile, using `uTileUnitScale` / `uTileUnitOffset`
(vertex-frame units → TARGET tile units, set in `setupTerrainUniforms`; a **0 scale means no
elevation**, which disables the test) and a `vTileUnit` varying. No attachment, no extra draw.

> **The offset is what makes this survive a STAND-IN, and it was missing.** For content,
> `setupTerrainUniforms` is called with the *target* tile (whose elevation texture the content
> stands on) and the *source* tile's vertex frame (the vertices are source-tile-local). Source and
> target are the same tile normally — but not while a tile loads, when an ancestor stands in for it.
> With a scale and no offset, `vTileUnit = pos.xy * uTileUnitScale` put the source's unit square in
> [0, 2^dz], so the `> 1.0005` test discarded everything except the one quadrant that happened to
> land in [0, 1]. Since **every** visible tile becomes a stand-in for a second or two after an
> integer zoom step, all line content — contours, roads — vanished at every integer zoom in terrain
> mode, and only in terrain mode (the test is off without elevation). Measured at
> lat 45.210031 lon 5.730591 z14.99 tilt 26 over a scripted zoom, contour pixels per frame went
> 26k → **30** → 23 409; with the offset they decay smoothly and never collapse. Measuring the
> position from the target tile's own origin (`offset = (frame(i,3) − target(i,3)) / target(i,i)`,
> signed, not `abs()`) gives each target its own share of the ancestor, so the four together still
> paint the whole thing, each with the elevation mapping of the surface it stands on.
>
> The same scale also feeds the lattice edge-coarsening test in `commonVsh` ([terrain](04-terrain.md)),
> which is wrong for a stand-in by the same argument. It is deliberately **not** changed: adding the
> offset there moves settled contour positions (2.8 % of the frame) because it changes elevation
> interpolation, which needs judging on device rather than bundling into a clipping fix.
>
> What this was *not*, each ruled out by measurement before the clip was found: missing draw data,
> deep or empty stand-ins, the renderer not being fed, elevation-texture misses, render-tile blend
> (`blend` was 1.0 throughout), and the terrain coarsening floor `_terrainMinTileZoom` — that one
> does re-cull the whole far field at every integer zoom, but the blank is identical with
> `MaxTileZoomCoarsening` raised so the far field does not churn, so it only lengthens recovery.

**Width: tangram's model, capped at nominal.** The offset is extruded in model space and displaced
onto the terrain, exactly as tangram does (`polyline.vs` + `res/scenes/terrain-3d.yaml`), so a line
is a world quad through the projection and tapers with distance. Left unbounded it also *grows*
towards the camera until a near contour is a blob, so the projected offset is measured and **shrunk**
back to the nominal width when it exceeds it. The factor is ≤ 1 by construction — it can never
manufacture an oversized quad, which is what an unbounded screen-space fit does.

> **The capped vertex takes its DEPTH from the terrain, and its XY from the screen.** Both halves
> are load-bearing. Applying the shrunk offset to `centerClip.xy` and keeping `centerClip.z` — the
> obvious way — gives the outer edge of a wide line the *centreline's* depth; on a cross-slope that
> is below the ground on the uphill side, so the depth test against the terrain surface eats the
> line in a ragged, stair-stepped band. The widest layer hits the cap first, which is why a route's
> white casing broke up while its blue fill survived, and why it only showed at a tilt. Shrinking
> the **world** position instead fixes the depth but makes the projected width only approximately
> nominal, and it drifts vertex to vertex — segments of visibly different width at z14.38. So:
> screen-space xy for the width, `mix(centerPos, edgePos, shrink)` for the depth.
>
> Ruled out first, each by measurement, before the cap was suspected: line tesselation and joins,
> the route source's simplify tolerance (real but separate — it is applied per TILE ZOOM in
> `MBVTTileBuilder::simplifyAndCacheLayers`, so a coarse tile collapses hairpins into chords),
> `TerrainOptions::MeshResolution` (32/64/128, no effect) and `Options::TileLODFactor` (no effect).
> The two A/Bs that settled it: with the content depth test disabled the casing is complete, and
> with the cap disabled the casing is complete but every line is visibly fatter.

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
