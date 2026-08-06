# The depth model

Scope: how the ground, the content on it, and the vector elements above it relate in depth. This is
the single most failure-prone part of the renderer — rounds 45 to 56 of the terrain work were almost
entirely about it — so it is documented as a model plus a catalogue of what breaks.

**It is tangram's model, ported whole.** Read `res/scenes/terrain-3d.yaml` in tangram-ng, not just
their shaders: `polygon.vs` sets `depth_shift = 0.0` "to allow blocks to modify", and the terrain
scene is the block that modifies it.

## The model

```glsl
// tangram core/shaders/polygon.vs
gl_Position.z += (proxy - layer) * (TANGRAM_DEPTH_DELTA * gl_Position.w + depth_shift);

// tangram res/scenes/terrain-3d.yaml
depth_shift = -0.02 * u_proj[2][3];   // [2][3] is -1 with glm::perspective => a FLAT 0.02
#ifdef TANGRAM_RASTER_STYLE
proxy *= 48.0;                        // "prevent terrain poking through the level above"
#endif
```

with `TANGRAM_DEPTH_DELTA = 2⁻¹⁹` on a 24-bit depth buffer, `layer` the style layer's order, and —
`core/src/style/style.cpp` — opaque *and* translucent geometry drawing with depth test **and depth
write**.

Ours, matching it:

| piece | ours |
|---|---|
| the ground | drawn at true depth, writes depth, pushed back 48 units per **proxy** level |
| content (fills, lines, buildings) | writes depth, carries `(proxy − layer)` with the layer ordinal |
| `depth_shift` | flat, not scaled by the projection |
| ordinals | dense across the whole stack, one range per layer (see below) |
| near plane | camera height / 50, as tangram ([04-terrain.md](04-terrain.md#near-and-far-planes)) |

### Why the two terms differ

`TANGRAM_DEPTH_DELTA · w` is a **constant-NDC** bias: its eye-space tolerance grows as
distance²/near, which is how a bias of a few units becomes hundreds of metres at range. That is the
mechanism behind every see-through this project has had.

`depth_shift` is a **constant clip-space** offset: its NDC effect is `0.02/w` — strong near the
camera, where an un-subdivided fill chords furthest below the surface, and vanishing at range, where
it would leak. That is why tangram can afford it and why it is the term that pays for not
subdividing content.

### Ordinals, and the budget

Tangram numbers its style layers in one global list (`res/osm-bright.yaml` uses 1..93) and spends
`93 × 0.02 ≈ 1.86` clip units across the scene. Our stack is several renderers — a composite layer's
children included — so the ordinals are handed out in **draw order, one dense range per layer**:
`MapRenderer::drawLayers` gives each `TileLayer` an ordinal base and advances it by that layer's
style layer count.

The quantity that matters is **constant × span**, i.e. the total depth budget the stack gets, not the
constant. A style with nine style layers at their 0.02 would spend a tenth of their budget. So the
shift is derived from the budget instead: `TERRAIN_TANGRAM_DEPTH_BUDGET / span`, which gives back
exactly 0.02 for a 93-layer style.

Measured on device (45.244172/5.760595 z13.2 t26, 9 ordinals): **0.2 is the largest total with no
see-through**; 0.3 opens pale wedges through ridges, 0.5 sees straight through Saint-Eynard. Their
budget *is* the leak threshold. Do not raise it.

Numbering starts at **1**, not 0: the ground is a numbered draw at the bottom of the same list
(tangram draws the terrain raster at `order: global.earth_order`), so content at ordinal 0 would
share the ground's term exactly and have no clearance over ground it chords across.

### Proxy depth

Tangram, `core/src/tile/tileManager.cpp`:

```cpp
setProxyDepth(m_proxyCounter > 0 ? std::max(maxVisS - tileId.s, 1) : 0);
```

The `m_proxyCounter > 0` is a **guard**, and it is the whole point: proxy depth applies only to a
tile drawn in place of one that has no data. A legitimately coarse tile in a mixed-LOD cover is a
live tile and takes **zero**, however far away it is. Giving it a depth pushes most of a tilted
view's far field back by clip units and takes the hillshade paint (which carries the same push)
behind the ground it shades — far hillshade missing, blinking as the cover's deepest level moves.

The measure is in **levels** (how many levels coarser than the deepest level on screen), not a flat
1, and the raster/ground multiplier is **48**.

## Four ways to get this wrong, all of them tried

| attempt | what it produced |
|---|---|
| masks dropped, content not writing depth | the previous zoom's roads painting through every gap in the new tile's content, blinking as the blend ran |
| content writing depth, no per-style-layer ordinal | fills shredded into stripes, washed road casings |
| no subdivision **and** no `depth_shift` | all content sunk into the terrain — contours and roads swallowed |
| ground stand-in walking to an ancestor, content not | roads snapping to straight lines over ground that IS displaced |

Two more from choosing constants instead of copying them: `depth_shift` scaled by `|m22|` (did
nothing), then by `|m23|` (dragged a landcover fill in front of the mountain); and an ordinal stride
of 32 per renderer, which reached the leak range with five layers.

## Rules to keep

1. **Never push the reference surface back.** Slack belongs on the content, forward and test-only. A
   backward push only works for opaque depth-writing content; translucent content double-blends and
   non-writing geometry leaks in the band.
2. **Content that writes depth must discard transparent fragments** (`alpha < 0.004`), or the
   transparent corners of sprite and dash quads pollute the depth buffer — white boxes and dark
   roads.
3. **Keep the ordinal out of the clip-space slack.** Ordinal-scaled clip slack was itself a leak;
   ordinal-scaled NDC pull washed out road casings and fills.
4. **Sort near to far inside a style layer** ([03-vt-renderer.md](03-vt-renderer.md)), because
   content writes depth.
5. **An emulator pass is not a device pass** for anything in this file. Every regression here was
   found on a device after the emulator was clean.

## Known open artifact

Landcover fills break into triangular shards over the hillshade **while the LOD is changing**, and
heal when tiles settle. Measured *not* to be the cause: fill subdivision (1 vs 2 cells identical) and
the depth-shift magnitude (0.02 / budget / 0.5 identical at a settled camera). The remaining suspect
is the proxy path — the ground stand-in and the content of the outgoing zoom disagreeing during the
swap. Not yet reproduced on an emulator.
</content>
