# Labels

Scope: text and icon labels from decoded tiles to screen. Contour labels specifically are in
[07-hillshade-contours.md](07-hillshade-contours.md#contour-labels-without-contour-geometry).

## The pipeline

```
tile decode          -> vt::TileLabel (text/icon, geometry, priority, group ids)
setVisibleTiles      -> buildLabelMaps: one vt::Label per globalId, geometries merged across tiles
VTLabelPlacementWorker -> LabelCuller::process per layer, sequentially, one culler per pass
GL thread startFrame -> opacity fades, elevation re-anchoring
GL thread renderLabels -> glyph quads rebuilt every frame, uploaded as batches
```

## Building the label set

On **every tile-set change**, `buildLabelMaps` recreates all `vt::Label` objects from the current
tiles. Labels with the same `globalId` from different tiles are merged (`mergeGeometries` — one
geometry copy per tile, identified by `(tileId, localId)`), and visibility, opacity and placement
are carried over from the previous object via `snapPlacement`.

**Placement stability invariant.** `snapPlacement` / `findSnappedPointPlacement` /
`findSnappedLinePlacement` prefer the geometry copy with the same `(tileId, localId)` as the previous
placement. Without that, re-snapping picks a winner by merged-list order — which changes with the
tile set — and a placement rebuilt from a differently clipped copy of a line can fail line fitting
(`buildLineVertexData`), after which the culler hides an already visible label. That is what
"labels jump and disappear while panning" looked like. Keep the invariant when touching `Label` or
`LabelCuller`.

Known remaining cost: `buildLabelMaps` reallocates every label on every tile-set change during
panning. Reusing unchanged labels is the next win here, and it is delicate — it relies on fresh
caches and `snapPlacement` semantics.

## Placement and culling

`VTLabelPlacementWorker` runs on `MapRenderer::vtLabelsChanged` (i.e. whenever draw data changes),
creates **one fresh `LabelCuller` per pass**, and calls `TileRenderer::cullLabels` for every vector
layer **sequentially**. The culler's screen grid accumulates across layers on purpose, so labels of
different layers collide with each other. `LabelCuller::process` therefore must **not** clear the
grid.

`LabelCuller::process`:

1. captures `wasVisible`;
2. `Label::updatePlacement` — only re-places a label when its envelope has fully left the frustum;
   resets opacity;
3. projects envelopes to screen space;
4. sorts by priority → `wasVisible` → layer index → size → opacity;
5. greedily inserts into a **16×32 screen grid** with SAT polygon-overlap tests.

A visible label keeps its slot unless a strictly higher-priority label overlaps it — that hysteresis
is what stops labels flickering when the camera moves slightly.

Fork-specific rules, all comparing the **placement's** `localId` (hence the stability invariant
above): `allowOverlapSameFeatureId`, `sameFeatureIdDependent`, and group ids with
`minimumGroupDistance`.

### Callout labels (fork-specific)

`LabelOrientation::CALLOUT` — style `text-placement: nuticallout` — is a point label **lifted away
from its anchor in screen space** and joined back to it by a leader line. It exists because a
panorama is the case the ordinary rules answer badly: hundreds of summits within a few degrees of
the horizon, all wanting the same band of pixels, and hiding all but a handful of them loses exactly
the information the view is for.

What changes, and only for this orientation:

- **`LabelCuller::placeCalloutLabel` replaces the hide.** The label is placed at its band
  (`text-callout-screen-anchor`, a fraction of the screen height from the top; below 0 it stacks
  from its own anchor instead), and while the grid says it is taken it moves up one
  `text-callout-step` at a time, for at most `text-callout-max-rows` rows. Everything else — the
  priority sort, the `wasVisible` hysteresis, the shared grid — is untouched, so callouts collide
  with ordinary labels and with each other in the usual way.
- **The offset is the culler's, and both the envelope and the vertex data read it**
  (`Label::setCalloutOffset`, in screen pixels along the camera up axis). One pixel is
  `scale / size` world units, because the glyph quads are in units of the font size.
- **The leader line is one more quad in the label's own vertex stream**, textured from a 4×4 white
  cell loaded into the glyph atlas (`TileLabel::Style::calloutLineGlyph`). It is built per frame
  rather than cached with the text — its length is the offset, which changes with everything else
  on screen — and only once, after both text passes, since a halo copy would just draw it twice.
  Sampling the cell's interior matters: the outer texels blend into the atlas padding under linear
  filtering, which thins the line and fades its ends.
- **A callout has to be clamped to the screen.** Every other label is evidence of its own
  visibility; this one is drawn where it is not anchored, so the culler caps the offset at the
  screen top and hides the label when even the first row does not fit.
- Rotation (`text-orientation`) stays with the CALLOUT placement instead of downgrading to POINT —
  angled names over a horizon is the whole look.

Picking is unchanged and needs nothing new: `GLTileRenderer::findLabelIntersections` tests the
placed geometry, so a callout is clicked where it is drawn, at the end of its leader line.

### Max distance (fork-specific)

A label glyph is screen-space: a street name 5 km away is drawn at the same size as one 50 m away,
so a tilted view fills its horizon band with labels nobody can read. Which labels a tile carries is
already decided by the style at the **tile's** zoom (`TileReader` sets `adjustedZoom = tileId.zoom +
bias`), and coarser far tiles — see [02-tiles.md](02-tiles.md) — thin them out a lot. The second
half of it is how far the labels that do exist may be seen:

```css
#transportation_name { text-max-distance: 2000; }   /* meters; 0 (default) = no limit */
#poi                 { marker-max-distance: 800; }
#shield              { shield-max-distance: 1500; }
```

It lives on the label **style** (`TileLabel::Style::maxDistance`), so it costs one comparison per
label per placement pass: `LabelCuller::process` measures the label's world anchor against
`ViewState::origin` and calls `setVisible(false)` beyond the limit. Hiding rather than skipping is
deliberate — the GL thread already animates opacity toward `isVisible()`, so the label **fades**
out when it passes the limit and fades back in when it returns, with no per-frame work.

Metres are converted with the mercator stretch at the view's own latitude
(`VTLabelPlacementWorker`), because 1/cos(45°) is a factor of 1.4 — too much to ignore in a number
a style author writes in metres. Tangram has no equivalent: their only control is the same per-tile
zoom filter.

## On the GL thread

- **Fades.** `updateLabel` moves opacity toward `visible ? 1 : 0`; invisible-but-fading labels stay
  rendered until they reach 0.
- **Terrain anchoring.** Label geometry is built flat at decode time, so a label must be re-anchored
  onto the terrain: once when it is new, and afterwards only when the elevation under one of its
  tiles changed. `invalidateLabelElevation(tileIds)` marks exactly those; the blanket version exists
  for whole-data-set changes. Anchoring costs one elevation sample per label vertex (~233 µs per
  label), and a whole screen of labels goes dirty at once while elevation streams in, so this loop
  measures 2.5–4.1 ms of a frame. It still has to run to completion: a label left dirty is drawn and
  culled at its old height, which reads as labels popping in at the wrong place and settling.
- **Vertex data.** Glyph quads are rebuilt from scratch for every visible label every frame and
  uploaded as one batch (`labelVertexBuildNs`, `labelBatchNs`). A GPU-billboard path would remove
  the per-frame world transform (`labelTransformNs`) — it is on the backlog, not implemented.

## Against tangram

Tangram's labels are built into the tile's styled mesh at tile build time and placed by their own
`LabelManager`; ours are re-merged and re-placed from the live tile set. The visible difference is
where the work happens: theirs is amortised into tile building, ours falls on tile-set changes during
panning. Their contour labels are generated from the elevation texture with no geometry at all,
which this fork now also does ([07-hillshade-contours.md](07-hillshade-contours.md)).
</content>
