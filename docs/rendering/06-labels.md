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

### Anchored shields: the name takes a free side (fork-specific)

A shield is ONE label whose glyph run is `[icon glyphs] CR [text glyphs …]`: the icon comes before
the first line break and the text after it, and `buildPointVertexData` resets the pen at that break.
Everything below rests on that split — the icon stays on the feature, only the text moves.

```css
#poi {
  shield-name: [name];
  shield-file: url(shields/place.svg);   /* a bitmap icon, as before */
  shield-icon-name: '<PUA char>';        /* AND/OR a font icon: one glyph of an icon face */
  shield-icon-face-name: 'osm';
  shield-icon-size: 15; shield-icon-fill: #b5651d;
  shield-anchors: 'right,left,top,bottom';  /* sides, in preference order */
  shield-text-optional: true;               /* no side free -> draw the icon alone */
  shield-text-dx: 2;                        /* gap from the icon, MIRRORED per side */
}
```

- **The sides are precomputed, the choice is per pass.** `TileLayerBuilder` measures the text once
  and stores one `TileLabel::Variant` per anchor — a `vec2` shift of the text pen and a `drawText`
  flag. No extra glyph run, no extra formatting: the block is placed against the icon's edge, so the
  style's own `horizontal-alignment` does not have to be mirrored, and `dx`/`dy` are re-applied as a
  gap along the anchor direction (a name pushed 2 px right of the icon is pushed 2 px LEFT on the
  left side). A style with no `shield-anchors` builds no variants and takes exactly the old path.
- **`LabelCuller::placeAnchoredLabel`** tries the side the label already holds first and then the
  style's order, taking the first free one — tangram's
  `do { … } while (isOccluded() && nextAnchor())` (`labelManager.cpp`), with their anchor set and
  order. Keeping the current side is what stops a name swapping sides under a moving camera; the
  exception is the icon-only variant, which is smaller than every other one and therefore always
  fits, so a label that fell back to it once would keep it for good and its name would never come
  back. It is never the preferred side.
- Along the side's own axis the text is placed against the icon's **edge**, and `dx`/`dy` become a
  gap pushed away from the icon. Across that axis it is **centred on the anchor** — a name above the
  icon has to sit over it, and the formatter's own alignment is derived from the sign of `dx`, which
  means nothing once `dx` is a gap.
- `shield-text-optional` appends a last variant that draws the icon alone. That is mapbox's
  `text-optional`; here it costs one more variant, not a second label.
- The side is carried across rebuilds by `snapPlacement`, so a label recreated by a tile-set change
  does not start at side 0 for one frame before the next pass moves it back.
- **Placement is re-run when the camera zooms** (`MapRenderer::viewChanged`, ¼ of a zoom level). A
  pass is otherwise only asked for when the TILE SET changes, and a label's envelope is screen-space:
  zooming in makes room nothing notices. The pass is **postponed** rather than queued
  (`VTLabelPlacementWorker::postpone`), so a zoom gesture places once when it settles instead of
  re-deciding at every step — placing mid-gesture is what made labels fade in and straight back out.

**Which labels keep their text** is decided by the same greedy insertion as everything else: the
culler sorts by priority → `wasVisible` → layer index → size → opacity and inserts in that order, so
a label sorted earlier claims its preferred side while later ones find less room and fall back to
`text-optional`. `shield-placement-priority` is therefore the knob for "these names matter more than
those", and a bigger `shield-size` also sorts earlier among equal priorities.

### Justifying a wrapped name (fork-specific)

`shield-text-horizontal-alignment: 'left' | 'middle' | 'right' | 'auto'` justifies the LINES of a
wrapped name inside its block. `auto` follows the side the culler chose — flush left when the name
is to the right of its icon, flush right when it is to the left — and an explicit value is mirrored
the same way. Unset keeps every line centred, which is what the formatter always did.

It costs no second glyph run: the formatter still centres each line, and `Label` measures each
line's ink extent once (`measureTextLines`) and shifts the pen per line at draw time
(`calculateLineShift`). Single-line labels — nearly all of them — shift by zero.

This is also what fixes "the gap looks bigger on one side": for a single-line name the gap is
symmetric by construction (the text is placed against the icon's edge, and it measures the same
17 px either way on screen), but a centred short line inside a wider block sits away from the icon,
and only justification can close that.

**Cost of the sides.** All sides share the placement, the scale and the label's screen axes, so
`Label::calculateVariantEnvelopes` builds all of their envelopes in one call and the culler only
repeats the cheap part (project + grid test) per side. Measured on the emulator with a deliberately
extreme style (every POI anchored, ~2300 live labels, 4 sides + icon-only): **20.8 ms per culler
pass against 16.3 ms with the property unset**, and the frame time moves by under 1 ms — which is
the extra labels `text-optional` lets through, not the placement. Building one envelope per side
instead cost 27.8 ms per pass, so the shared setup is worth keeping. A style that does not use
`shield-anchors` measures identical to before. The pass runs on the placement worker; no frame
section shows it, which is why `RenderStats::cullerNs` (`cullMs=` in the `RenderStats:` line) exists.

### Font icons

`shield-icon-name` is a run of glyphs from an icon face, drawn before the text with its own colour
(`shield-icon-fill`, a third style slot next to the halo and the second text run) and its own size.
It is not a bitmap: the glyphs are SDF like the text, so they stay sharp at any zoom and cost one
atlas cell each.

The face has to be reached **through the label font** — `getFont(labelFont->getName(), iconFace)` —
because `FontManagerFont::shapeGlyphs` rasterizes a fallback's glyphs into the atlas of the font it
was called on, and one label can only be drawn from one atlas. Resolving the icon face on its own
and shaping with it gives glyphs in a different atlas and the label renders nothing. The face is
also re-requested at the render size the ICON is drawn at, not the text's, so a large icon next to
small text is not a magnified small raster.

A shield may carry both: the bitmap (`shield-file`) is the first prefix glyph and the icon run
follows it, so they sit side by side rather than on top of each other.

### Plates behind the text and behind the icon (fork-specific)

A label may carry two plates — one behind the text, one behind the icon run — each a rounded
rectangle with its own colour, corner radius, padding and border:

```css
#road_label {
  text-background-fill: #ffffff;  text-background-opacity: 0.85;   /* also on a shield: */
  text-background-radius: 3;                                        /*   shield-background-*   */
  text-background-padding-x: 4; text-background-padding-y: 2;       /*   shield-icon-background-* */
  text-background-border-fill: #444444; text-background-border-width: 1;
}
```

- A plate is **3-sliced from one atlas cell** (`buildRoundedRectBitmap`, cached by radius, the glyph
  map dedupes by pointer): left cap, stretched middle, right cap — which is what keeps the corners
  round at any name width.
- The **border is a second plate behind the fill**, one border width larger on every side and with
  its corner radius grown to match. That is why it needs no shader and no second texture: it is the
  same cell drawn bigger in another colour.
- Both plates are part of what the label covers, so `calculatePlatedBBox` grows the envelope by
  their padding and border — the culler tests what is actually drawn, and a callout's leader line
  ends outside the plate rather than inside it.
- Each colour is one slot in the label batch, like the halo (`LabelBatchParameters::MAX_PARAMETERS`
  is 16; a style using both plates with borders takes 4 of them plus text, halo, secondary and icon).

**Cost**, measured on the emulator with a plate behind the name AND behind the icon on ~2100 labels:
vertex build **4.6 → 5.2 µs per label per frame** (+13%) and batch time **12.8 → 17.8 µs per label**
(+39%), i.e. about **+1 ms per frame at 2100 plated labels**, and no change in draw-call count
(≈60/s either way). It is proportional to the plated labels on screen: each plate is 3 quads built
in the same loop as the glyph quads, so a screen of road shields — tens, not thousands — is far
below what the frame-time noise on this emulator can even resolve.

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
`LabelManager`; ours are re-merged and re-placed from the live tile set. Their icon and its name are
**two labels** linked by `setRelative`, with `optional` deciding whether the parent survives the
child being occluded; a shield here is one label with several text layouts, so the anchor retry loop
is theirs but the object model is not (see
[11-tangram-diff.md](11-tangram-diff.md)). The visible difference is
where the work happens: theirs is amortised into tile building, ours falls on tile-set changes during
panning. Their contour labels are generated from the elevation texture with no geometry at all,
which this fork now also does ([07-hillshade-contours.md](07-hillshade-contours.md)).
</content>
