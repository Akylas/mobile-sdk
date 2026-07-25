# Terrain "landcover holes": grey background replacing green fills

Status: **partially fixed, root mechanism identified, one residual case open.**

## Symptom

In 3D terrain mode, polygon fills (landcover) are replaced by the flat style background
colour. Appears while zooming/panning; the persistent form is fixed, a transient form
remains and settles when the map stops.

## Mechanism (established, not inferred)

`renderTileBackground` paints the style `background-color` onto the **full terrain grid
surface** and **writes depth** (`glDepthMask(GL_TRUE)`, GLTileRenderer.cpp:1581), before the
polygons of the same tile. Polygons then draw with `glDepthMask(GL_FALSE)` and **zero**
forward bias under `GL_LEQUAL` (GLTileRenderer.cpp:1660,1665).

So any fill whose computed height ends up below the background surface fails the depth test
and the already-painted grey stays. It is a colour *replacement*, not overdraw — which is why
`setDebugWireframe` looks perfect (the surface mesh is fine; the *fill* sags).

The fill's clearance shrinks hard with zoom: `slackScale ∝ tileSize²`, further multiplied by
`_terrainSlackScale = (32/meshResolution)²` — 0.0625 at meshResolution 128.

## Fixed: source-density vs draping disagreement

`TileLayer` set source-density (skip fill subdivision) from `isDrapeFillsEnabled()`, decided
**globally at decode**. But `GLTileRenderer` drapes only tiles where
`sourceTileId == targetTileId` — decided **per tile at render**. Overzoomed and proxy tiles
therefore rendered as 3D geometry carrying un-subdivided source-density fills: a few large
flat triangles chorded across the terrain, sagging below the surface, occluded.

Fix: `terrainSourceDensity = false` in TileLayer.cpp — a tile cannot know at decode time
whether it will be draped. Costs the perf win (fills subdivide again; those VBOs upload on
the render thread). The better long-term fix is to make the two decisions agree by draping
overzoomed tiles too, which is what the parked `feat/terrain-rtt-draping` branch does.

## Ruled out (each by measurement, not reasoning)

- Depth precision — resolves when motion stops; 24-bit depth confirmed.
- The terrain mesh — `setDebugWireframe` clean.
- Cross-layer interaction — reproduces with a single layer, hillshade removed.
- `sourceTileId` vs `targetTileId` terrain uniforms — tested on master, no change.
  (Still a real latent inconsistency: the surface uses `renderTile.targetTileId` while content
  uses `renderLayer->targetTileId`. Kept on branch `test/terrain-uniform-target-tile` in
  libs-carto.)
- Empty drape textures — magenta bake-clear showed them fully painted.
- Missing elevation at decode — logged: `no-elev 0` over 1500+ decodes.
- Insufficient clearance from `meshResolution` — tested at 32, artifact persists.

## Open: transient case, prime suspect

`CompositeVectorTileLayer` with `setSinglePassRenderingEnabled(true)` (the demo's
configuration). Composite layers drive one `GLTileRenderer` over disjoint style-layer ranges
via `setRendererLayerIndexRange`. If the background and the landcover are drawn in different
invocations, the per-tile-layer terrain pre-pass `glClear(GL_DEPTH_BUFFER_BIT)` and the
depth-writing background may not establish the reference the polygon expects.

One-line test: `compositeLayer.setSinglePassRenderingEnabled(false)`.

## Method note

Six causes were proposed before the mechanism was found; five were wrong. Every real advance
came from a cheap decisive probe, not from reading: the wireframe check, the green/grey
detail, the single-layer test, the magenta bake-clear, and above all the
`TerrainTileTransformer` counter that showed `divideThreshold inf` on every decode. Prefer
instrumenting the actual program state over another plausible patch — and make probes
unconditional, so "no output" cannot be confused with "no problem".

## Transient form: cause found

`CompositeVectorTileLayer` renders **one VT pass per segment** — each group/external child is a
separate `TileLayer` with its own `GLTileRenderer`, each running the terrain pre-pass with
`glClear(GL_DEPTH_BUFFER_BIT)`.

- The style `TileBackground` is included **only in invocation 0** (`includeBackground` is true
  for group 0 alone, CompositeVectorTileLayer.cpp:305). Landcover can land in a later
  invocation, split by layer *name* regex (`setRendererLayerFilter`).
- Invocation >=1's depth clear destroys invocation 0's background depth, and the background is
  not redrawn. The grey *colour* stays in the colour buffer. The polygon's depth test is
  therefore resolved inside its own invocation, against a surface it never visually replaced;
  when it fails, the pixel keeps invocation 0's grey.
- Each renderer has its own drape cache and its own `DRAPE_BAKE_BUDGET_PER_FRAME = 4`. During
  motion the background renderer and the landcover renderer disagree about whether a tile is
  draped: background draped (grey baked into the surface at true depth) while landcover is
  still undraped geometry with zero bias, which loses the test. When motion stops both caches
  fill and they agree again.

A single plain `VectorTileLayer` does not show this: background and landcover share one
invocation and one drape texture, so a depth failure hides both together.

Workaround: `setDrapeFillsEnabled(false)` — with source-density fixed, all renderers then take
the subdivided-geometry path consistently. Real fixes: make the drape decision consistent
across composite invocations, or drape overzoomed tiles too (the parked RTT branch).

Note: `setSinglePassRenderingEnabled` is a **no-op placeholder** (CompositeVectorTileLayer.h:109)
— toggling it changes nothing. `setRendererLayerIndexRange` has no callers. `_terrainRenderOrder`
is stored and never read.
