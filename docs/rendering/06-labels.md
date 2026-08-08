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

### Label size and the camera (fork-specific)

In a planar projection a label keeps a **constant on-screen size**: its world size comes from the
zoom alone, so the perspective divide would otherwise blow it up towards the camera on a tilted view
(and 3D terrain, which lifts anchors by a kilometre, made that obvious). `calculateTerrainScaleFactor`
cancels it by scaling the label with `viewDepth / focusDepth`.

`focusDepth` is the distance the zoom is calibrated at — the **camera-to-focus distance**, which
`TileRenderer` puts in `ViewState::focusDistance` for both the render and the cull pass. vt used to
guess it as the point where the view axis meets the z=0 plane; that is the same number only while
the focus sits ON the ground. Lift the viewpoint (free roam, a panorama from 2600 m) or aim at the
horizon and the guess runs away, shrinking every label on screen as the camera rises or flattens —
which is what "labels get smaller as I go up or pan" was. `focusDistance` 0 falls back to the old
guess, so a host that does not set it behaves as before.

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
- **The lift is in SCREEN PIXELS, and it stays there.** Two things make that true, and both had to
  be fixed before a row of names was a row:
  - *The conversion is read off the projection, not off the label's scale.* One pixel is
    `depth / (projection scale × half the screen height)` world units at the label's own depth
    (`Label::calculatePixelToWorld`). Converting with the label's scale instead — which comes from
    the zoom — makes the same stored lift mean a different number of pixels whenever the camera
    tilts, rises or zooms, so the labels slide up and down the screen between placement passes.
  - *The anchor moves; the label does not follow it.* Labels are re-anchored on the GL thread as
    elevation tiles arrive (`updateElevation`), and a tilt slides the anchor up or down the screen,
    while a placement pass only runs when the draw data changes. The culler therefore records the
    anchor's screen position along with the lift (`setCalloutPlacement`) and the draw path corrects
    by how far it has moved since (`calculateCalloutLift`), so the label holds its LINE rather than
    its distance from a summit that has meanwhile moved.
- **The leader line is one more quad in the label's own vertex stream**, textured from a 4×4 white
  cell loaded into the glyph atlas (`TileLabel::Style::calloutLineGlyph`). It is built per frame
  rather than cached with the text — its length is the offset, which changes with everything else
  on screen — and only once, after both text passes, since a halo copy would just draw it twice.
  Sampling the cell's interior matters: the outer texels blend into the atlas padding under linear
  filtering, which thins the line and fades its ends.
- **A callout has to be clamped to the screen.** Every other label is evidence of its own
  visibility; this one is drawn where it is not anchored, so the culler caps the lift a constant
  `SCREEN_EDGE_MARGIN` short of the top edge. The margin is a constant on purpose: it also caps a
  label the band placed correctly, and a margin proportional to the label's own height then pushes
  long names further down than short ones — the row stops being a row. A summit already high in the
  frame has no room left above it, and its name is the one worth keeping, so the lift is **pulled
  back down** to that cap rather than hidden (its leader line shortens to nothing with it).
- Rotation (`text-orientation`) stays with the CALLOUT placement instead of downgrading to POINT —
  angled names over a horizon is the whole look.

**Which point of the label is anchored.** Two style properties, both naming a point of the label's
own box — `center`, `left`, `right`, `top`, `bottom`, `top-left`, `top-right`, `bottom-left`,
`bottom-right` — rotated with the text, so on a name tilted 55° the `bottom-left` corner is where
its first letter starts:

```css
#mountain_peak {
  text-callout-line-anchor: bottom-left;  /* held over the summit; where the leader line ends */
  text-callout-align: top-right;          /* the point put on the band line */
}
```

`text-callout-line-anchor` **moves the label** so that point lands on the anchor's vertical
(`Label::calculateCalloutShift`, applied to the glyph offsets, the plate and the envelope alike) —
which is what keeps every leader line vertical while the text starts exactly above its summit.
`text-callout-align` only decides what the band line is measured against (`LabelCuller` reads that
point off the screen envelope by bilinear interpolation of its four corners). The pair is what the
two panorama looks are made of: names pinned to the top of the screen hang from their `top-right`
corner so the text stays under the edge, names in a band lower down line up on the same
`bottom-left` corner they are anchored by. Unset (the default) keeps the old behaviour: the text
laid out around its own anchor, the band measured against the bottom of the bounding box.

### A second run of text (fork-specific)

A summit name and its elevation are one label with two type sizes. `TextFormatter::Options` carries
an optional second run, so it is laid out **with** the first one — one baseline, one bounding box,
one background plate, one colour:

```css
#mountain_peak {
  text-name: [name];
  text-secondary-name: [ele]+'m';
  text-secondary-scale: 0.62;   /* of the main font size */
  text-secondary-dx: 3;         /* gap before it, pixels */
  text-secondary-dy: 0;         /* baseline shift, pixels, down positive */
}
```

`TextFormatter::appendSecondaryRun` lays the run out on its own (left aligned), scales its glyph
geometry, and rebases its `CR` pseudo-glyphs — which carry an **absolute** pen position, see
`Label::buildPointVertexData` — onto the end of the first run. Both runs then shift by the share of
the extra width the alignment asks for, so `text-horizontal-alignment` still applies to the pair.
The glyphs come from the same atlas raster as the main text, so a very small scale is a magnified
raster: this is for a suffix, not for a second paragraph.

### Ranking with the view (fork-specific)

`text-placement-priority` is evaluated once, at decode time, from the feature. A panorama also wants
to rank by something only the frame knows — how far the summit is — so a label style may carry a
**rank function**, evaluated by the culler once per label per placement pass and **added to the
priority**:

```css
#mountain_peak {
  text-placement-priority: [ele];              /* the higher summit claims the row ... */
  text-rank: 0 - [view::distance]/100;         /* ... and the nearer of two equals wins it */
}
```

`view::distance` is metres from the camera to the label being ranked. It is defined **only** in this
evaluation: `ViewState::labelDistance` is 0 everywhere else, because the renderer evaluates a style
function once per batch (`GLTileRenderer::renderLabelPass` packs colour and size into a 16-slot
parameter table) and a per-label value there would cost a batch per label. Ranking never changes how
a label looks — only which of two colliding labels keeps its slot.

Two notes for style authors:

- Prefer an expression that reads **only** the view state. It has no feature dependency, so
  `FloatFunctionProperty::getFunction` hands every feature the same function object and the label
  style is not rebuilt per feature.
- `0 - x`, not `-x`: CartoCSS's `literal` rule accepts `-` as a first character, so a leading minus
  in front of a field is read as the literal string `"-"` and the declaration fails to parse.

- **A callout keeps the row it holds.** The offset is carried across label rebuilds
  (`snapPlacement` — it belongs to the label, not to the tiles it was built from; without that, a
  rebuilt label drops onto its own anchor until the next placement pass, which is a whole screen
  of names jumping every time tiles stream in), and a label that was visible tries its previous
  row before any other. `text-callout-persist: <passes>` goes further: a name already on screen may
  fail placement that many consecutive passes before it is hidden, so it does not blink out and
  back in while the camera moves. Default 0 — hide on the first failure, as before. A held-over
  name may sit closer to its neighbours than `text-min-distance` allows, but never **on top** of
  one: a placement pass only runs when the draw data changes, so an overlap granted here would
  stay on screen until something else moved. It is held on the band's own line too, never at the
  lift it happened to have — a name kept off the row is what the row exists to avoid.
- **The step's sign is the stacking direction.** A negative `text-callout-step` stacks the rows
  DOWNWARDS, which is what a band pinned to the top of the screen needs: there is no room above it,
  so stepping up piles every row that loses its slot into the top edge — and since the ranking puts
  the nearest summits first, the effect reads as "the closer the label, the lower it sits".
- **The group's minimum distance is tested at every row.** `text-min-distance` is what thins a
  crowded ridge out, and testing it only after placement would put a label on a free row and then
  hide it for being too close to a neighbour — the one outcome the stacking exists to avoid.

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
