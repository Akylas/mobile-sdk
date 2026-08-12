# Hillshade, contours, hypsometric tint

Scope: everything painted *from* the elevation data rather than from vector geometry.

The design principle is tangram's: `res/scenes/hillshade.yaml` computes hillshade, contour lines and
the hypsometric tint in the `color:` block of the terrain raster draw the renderer is already doing —
**zero extra tiles, zero extra draws**. Ours converges on that.

## Why it matters

Every layer with its own tile set multiplies the frame. Measured on the north pan, render tiles per
one-second interval:

| configuration | render tiles | surface draws |
|---|---|---|
| base only | 132 | 265 |
| base + hillshade (as its own tile layer) | 693 | 926 |
| base + hillshade + contours | 492 | 618 |

Adding the hillshade as a tile layer multiplied render tiles ~5×.

## The terrain paint

A `HillshadeRasterTileLayer` in **paint mode** holds no tiles at all. It shades the elevation texture
the terrain has already bound, as one quad (or, under the shared ground, one grid draw) per terrain
tile, at its own position in the layer order. What disappears: the DEM tile set with its cull, fetch,
decode, normal-map build and upload, its stencil mask and its share of the render tiles.

- `vt`: `GLTileRenderer::setTerrainPaint`, `renderTerrainPaintSurfaces`, the `terrainPaint*` shaders.
- `all/native`: `HillshadeRasterTileLayer` decides paint mode, `TileRenderer::setTerrainPaint`
  carries it, `TileLayer::drapeStackSignature` watches its appearance.

**The lighting is the same code**: the normal-map lighting shader (built-in or a custom one) is
injected over a prelude that reads the terrain DEM and is handed a normal rebuilt from the DEM
gradient. All hillshade methods, colours and custom `getElevation()` shaders work unchanged.

### When it engages

3D terrain, and the layer's data source is the terrain's own. Anything else keeps the normal-map tile
path: a different DEM must not be silently replaced by the terrain's.

**It is not pixel-identical, by construction.** The sampling is the terrain's elevation grid, so the
layer's own zoom bias no longer reaches it, and the gradient is recomputed per drape/ground texel in
floats instead of interpolating an 8-bit-packed 256² normal map — crisper, and blockier where the DEM
grid is coarse. A custom shader reading `getRawColor()` sees the terrain's re-encoded DEM texel;
`getElevation()` is the portable one.

Device measurement with a background-only style (`--es minimal true`, north pan, interleaved):
**22.0 fps against 17.0 for the normal-map path**, frame 38.7 vs 49.9 ms. With a full style the two
are a wash, because the base map's own geometry is the frame; the hillshade's cost there is tile
loading, which the frame timer does not see.

Two things the port had to get right:

- **The relief boost follows sampling density, not a tile id.** The low-zoom boost was keyed off tile
  zoom × bitmap resolution; the terrain's grids are 514² at z11 (38.2 m/texel, the density of an old
  z12 256² tile), so keying off the grid's own zoom made the paint ~1.5× too strong. It now derives
  the zoom from metres per texel.
- **A paint has no per-tile fingerprint.** Its appearance rides `TileLayer::drapeStackSignature` and
  it reports no tiles at all. Reporting the previous frame's cover instead made every tile that had
  just entered it look incomplete (surface draws up 12%).

### The DEM level

The terrain caps the elevation grid at what the **mesh** can express, which drops two zoom levels.
Shading is per fragment and resolves far more than that, so on the paint the cap is visible as blur
from z15 up. `getFullDetailDataTile` + `ElevationTextureCache::setFullDetail` lift it — and it stays
**off by default**, because the texture pipeline cannot pay for it: 2.5 fps against 6.7 on device,
with the working set jumping ~16× past the 96-texture cache. Fixing that is the elevation-texture
port described in [04-terrain.md](04-terrain.md#the-elevation-texture).

## Contour lines

Drawn as a fragment block on the terrain draw, from the same DEM: distance to the nearest contour in
metres divided by the per-pixel elevation change (`fwidth`), giving a screen-width anti-aliased line
(`u_contourIntervals`, `u_contourHalfWidths`, `u_contourColors`, `u_contourClassCount` — one class
per elevation divisor, the coarsest match winning, which is what a `#contour [div=N]` style says).

### NEVER bake a line into the drape — a contour or a road

The block is compiled into passes that draw **screen fragments** — the paint's surface pass
(`PAINT_SURFACE`) and the ground/drape surface draw (`CONTOUR_BANDS`) — and never into the drape
bake, and that is deliberate. The drape is one texture per tile at a fixed resolution, mapped onto
the surface afterwards, so anything baked into it is resampled: magnified into a soft band where the
tile is close and large on screen, minified into aliasing (or mipmap mush) at a grazing angle. A fill
survives that; a hairline does not. It is the same reason `TerrainOptions.DrapeLinesEnabled` is off
by default — fills are draped, lines are not. If a contour or a road ever looks stretched or blurry
on a slope, this is the first thing to check.

The consequence is that contours have to be drawn by a surface pass even when the fills are draped —
not folded into the bake with them.

**Measurement warning.** The old "contours measured free: 7.25 fps without against 7.57 with" line
was taken with the drape ON, where this block is not compiled at all: it compared two frames that
both had no contours in them. Any number for per-fragment contours has to come from a run whose
screenshot shows the lines.

Turning them on for a layer that would otherwise fall back to its own DEM tile set moved render
tiles 494 → 216 and `layers` 18.4 → 16.1 ms.

**In a composite base the settings come from the style**, not from the `HillshadeRasterTileLayer`
setters — those only reach a stand-alone layer. The style properties are
`hillshade-contour-interval`, `hillshade-contour-width`, `hillshade-contour-color`
([09-composite-layer.md](09-composite-layer.md)).

### Contours from the style, painted per fragment (opt-in, and not yet worth it)

`mvt::resolveContourStyle` reads the `#contour` layer's LINE rules and answers with one class per
elevation divisor — `{divisor, colour × opacity, width}` — evaluated at the current view zoom and
nuti parameter state, or with `shaderCapable = false` and a reason. `CompositeVectorTileLayer::
applyContourPaint` then either paints those classes (`ContourClass`, `TileLayer::
setTerrainContourPaint`, source switched to `setLabelStubsEnabled` so the labels stay real
features) or leaves the traced geometry alone. Everything the shader cannot reproduce falls back:
a dash, an offset, an arrow, casing, a filter on a field other than `div`/`stub`, a line property
that reads the feature.

Four things this got wrong first, all worth remembering:

- **A `#contour` layer carries more than lines.** Its `ContourConfigSymbolizer` configures the
  source and draws nothing, and its text rules are the labels — both have to be skipped, not
  treated as "something the shader cannot draw".
- **`Rule::getReferencedSymbolizerFields` is per RULE.** A contour rule usually carries the text
  symbolizer beside the line one, so it reports `ele` from `text-name: [ele]+' m'` and rejects a
  line that reads nothing. Ask the line symbolizer's own properties, and ignore `view::zoom`,
  `nuti::` and `zoom`, which the resolver evaluates itself.
- **"The last matching rule wins" is wrong.** A style whose base rule is `line-width: 0` with the
  real widths in nested `[div>=N]` blocks resolves to width 0 for every class that way. Every
  matching rule paints in order and a zero-width one paints nothing, so the class is the last match
  that would actually draw a line.
- **`setLabelStubsEnabled` notified tiles-changed even when the value was unchanged.** Called once
  a frame by this decision, that threw away every contour tile *and* every elevation grid
  (`ElevationManager` listens on the same source) every frame.

**Where the bands are drawn decides whether this pays at all.** Crosscall, city camera, interleaved
pairs, `adb shell setprop debug.carto.shadercontours 1`:

| | fps | GPU total | GPU layers | GPU drape | geom draws | geom indices |
|---|---|---|---|---|---|---|
| traced geometry | 12.2 / 12.1 | 33 ms | 21 ms | 4 ms | 636 | 22.6M |
| bands in a paint PASS of their own | 9.2 / 9.1 | 68 ms | 56 ms | 5 ms | 272 | 10.1M |
| bands in the GROUND draw | **13.1 / 13.3** | 39 ms | 15 ms | 19 ms | 384 | 14.7M |

A pass of its own is a third SLOWER than the geometry it replaces, with half the geometry: it is an
extra full-cover surface pass whose fragment shader reconstructs the DEM (a nine-tap stencil) and
runs the hillshade lighting before it gets to the bands. Composited into the ground/drape surface
pass that already runs — tangram's arrangement — the same bands are **+8%** instead, because the
band costs `fract()` and `fwidth()` on a height the vertex stage already computed and no texture
fetch at all. Note where the cost moved: the layer pass drops 21 → 15 ms with the contour geometry
gone, the surface pass rises 4 → 19 ms, and the frame still wins.

**Where the remaining cost is, and how to remove it.** Split measured with the block compiled but
the class count at zero: the varying and the derivatives are **~1 ms**, everything else is the
per-class work — 2.2 ms per class per frame originally, **1.6 ms** after the three optimisations
(count compiled in so the loop unrolls, `1/interval` as a uniform because a per-fragment divide is
several times a multiply, coverage in `mediump` with the branch as a `mix`). It is still **linear in
the number of classes**, so a style with more of them pays more.

**The level LUT removes the count from the cost** (`CONTOUR_LUT`, `GLTileRenderer::
buildContourLut`). Every divisor in play is a multiple of the finest one, so the lines of *every*
class sit at multiples of that base interval, and one index — the nearest base level — names the
class the fragment belongs to:

```
level    = floor(e * u_contourLutParams.x + 0.5);              // e = vTerrainMeters, x = 1/base
distPx   = abs(e - level * u_contourLutParams.y) * pxPerMetre; // y = base
u        = fract(level * u_contourLutParams.z) + 0.5 * u_contourLutParams.z;  // z = 1/size
vec4 cls = texture2D(u_contourLut, vec2(u, 0.25));  // rgb + opacity
vec4 row = texture2D(u_contourLut, vec2(u, 0.75));  // r = half-width, g = interval
```

`fract(level / size)` is the level index modulo the table width, so the width never has to be a
uniform. The table is `lcm(intervals) / base` texels wide (20 for the demo's
`50·100·200·250·500·1000` at z13.6, 100 when a 10 m class is in play, cap 256), `NEAREST`, two
rows, and is rebuilt only when the resolved classes actually change — the layer re-applies them
every frame, so `setContourBands` compares before rebuilding or it re-uploads the texture 60 times
a second. Which class owns a level is decided on the CPU while building it (the coarsest divisor
that divides the level wins), which is where the "coarsest match wins" rule stops costing anything
per fragment. The unrolled loop stays as the fallback for a style whose intervals are not multiples
of the finest one (`20·50` alone has no common base under the cap).

Measured on the Crosscall, mountain camera 45.244172/5.760595 z13.6 t35, six classes, two
interleaved pairs — the same binary with `buildContourLut` forced to bail is the other arm:

| | GPU surface pass | fps |
|---|---|---|
| unrolled loop, 6 classes | 18.9 / 18.6 ms | 18.4 / 18.5 |
| level LUT | **13.3 / 10.1 ms** | **20.0 / 20.0** |

The two LUT arms differ by 3 ms of device drift and the loop arms by 0.3, so read the direction and
the size, not the third digit: the six-class cost collapses to roughly what one class used to cost,
and a style with twelve classes would now measure the same.

Two things the bands got wrong against the traced lines, both fixed with them:

- **A style width is in unscaled-DPI units and reaches device pixels through three steps**, so
  taking it as pixels drew the same style 3.2× too thin on a 288-dpi 1648-high screen. The steps,
  in order, are the width table of `renderTileGeometry` (`0.25 · normalizedResolution · width /
  tileSize`), the half-unit AA margin `lineVsh` puts on every line (`(units - 1) · 0.5 + 1`), and
  `lineFsh`'s `uAntialiasScale` (`screenHeight / normalizedResolution`). `applyContourPaint` walks
  the same three. The band's coverage ramp is `clamp(halfWidth - distPx, 0, 1)` for the same
  reason — that is `lineFsh`'s ramp, one device pixel wide.
- **One class set covers the whole screen**, where the traced path gets its distance LOD for free
  from the divisor ladder picking a coarser set a zoom down. Without that, the far half of a tilted
  view is a solid wash of merged lines. Each class now fades out where its own spacing drops below
  a few pixels (`smoothstep(2, 5, spacingPx)`, the interval carried in the LUT's second row) — the
  thresholds are chosen, not ported: tangram has no equivalent, its contours are per raster tile
  and take the tile's zoom.

Still opt-in. What is left before it can be the default: the index lines are lighter than the
traced ones close up, and the cost has only been measured at one camera.

## Which contours a traced tile carries, and which of them are drawn

Two separate decisions, and confusing them empties the map.

Both are app settings now, not constants: `setIntervalMultiplier(maxZoom, multiplier)` and
`setResolutionForZoom(maxZoom, resolution)`, each a table of `(maxZoom, value)` rungs with `-1` for
"everything above". Defaults: interval `(9, 50) (11, 10) (13, 5) (any, 1)`, resolution table **empty**.

`getIntervalForZoom` decides what the tile **carries**. It is a cost rule:
a low-zoom tile covers a huge area and its DEM is sampled far too coarsely to place a 10 m line
meaningfully. Two constraints on the rungs:

- **They must nest** — every interval a multiple of the finer one. 200 m and 500 m share no
  elevation, so a z10 tile's 600 m line had nothing to meet in the z9 tile beside it and stopped
  dead at the border. `10 | 50 | 100` nests.
- **They must stay usable when zoomed out.** The original ladder went to 50× the base (500 m) at
  z ≤ 9, which is two or three lines on a mountain: zoomed out, and across the whole far half of any
  tilted frame (those tiles are z6–z9), the map read as *no contours at all* while the hillshade
  drawn from the same DEM stayed fully detailed. That was reported as a rendering bug; it was the
  ladder. Now 50×/10×/5×/1× over z9 / z11 / z13 / above — measured on a mid-range phone at ~10% less
  tile-generation CPU than a uniform fine ladder, for the same picture under a `div`-filtered style.

The **grid** is a different matter and must NOT scale with zoom. A tile is drawn at roughly the same
screen size whatever its zoom, so the grid is what fixes the shape on screen: at z9 a 48-sample grid
puts contour vertices 1.6 km apart and the far half of a tilted view — which is made of exactly those
tiles — reads as long straight chords. Uniform 128 measured both **faster and smoother** than the
DEM's own 512 (12.0 vs 14.7 CPU-seconds over 25 s, and contours complete at t=4 s instead of blank).

What is **drawn** is the style's decision, per camera zoom, keyed on `div` (the largest nice divisor
of the elevation — 1500 → 500, 250 → 250, 130 → 10), exactly as the pre-baked tileset is filtered.
It has to be a **width (or opacity) ramp**, not a filter: a CartoCSS filter is evaluated per tile at
decode time and cannot see the camera, while `linear([view::zoom], …)` is evaluated per frame. A
width of 0 draws nothing — the quad is degenerate — so the pattern is

```
#contour {
  line-width: 0;
  [div>=10]  { line-width: linear([view::zoom], (13, 0), (13.5, 0.8)); }
  [div>=50]  { line-width: linear([view::zoom], (11, 0), (11.5, 1.0)); }
  [div>=100] { line-width: linear([view::zoom], (8.5, 0), (9, 1.2)); }
  [div>=500] { line-width: 1.6; }
}
```

**Diagnosis trap:** two frames at different zooms that look *identical* are not proof the ramp is
dead — below z9 only the `div>=500` rank has a width, so every frame from z6 to z8 legitimately
shows the same lines. Take the positive control at z13.8, where the finest rank fades in.

## Contour labels without contour geometry

Contour *lines* are free, but labels need something to lay text along. Tangram generates that from
the elevation texture (`core/src/style/contourTextStyle.cpp`) and carries no contour geometry, no
contour source and no contour tiles at all. That generator is ported into
`ContourTileDataSource` as **label stubs** (`setLabelStubsEnabled`):

- a 4×4 grid of seeds per tile, aligned across zoom levels so a label does not jump when a finer
  tile replaces the one it came from;
- each seed walks **down the elevation gradient** onto `round(elev/interval)·interval` — at most 12
  iterations, interpolating straight onto the level once it is bracketed, to a position error of
  `0.25/256` of a tile;
- then along the contour **tangent** in steps of `2/256` until the stub is `1.25 × 32/256` long —
  about 20 points, exactly enough to carry the text.

The features keep the layer name and the `ele`/`div` attributes, so existing `#contour` **text**
rules style them unchanged, and they carry `stub` (1 for a stub, 0 for traced geometry) so a style
keeps its **line** rules with a `[stub=0]` filter. Both modes set the attribute: an undefined
attribute does not compare equal to 0, so a one-sided property would silently drop the traced lines.

Emulator counters at the ridge camera: geometry draws 2035 → 1217, indices 51.6M → 29.4M, render
tiles 1197 → 712. Device A/B still to take.

**The trap:** the stub levels must be the levels the shader draws, or labels sit between the lines.
Set `LabelInterval` to the layer's contour interval (tangram carries the same warning in their
source).

## Hypsometric tint

Currently a `CustomRasterTileLayer` with a shader over the DEM source (the demo's
`DemoStyles.hypsometricShader()`). It has **not** been converted to a paint kind; doing so is the
same quad and the same prelude as the hillshade paint, and is the obvious next step for it.
</content>
