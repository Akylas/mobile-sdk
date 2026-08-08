# Post-processing: full-screen effects and the layers that sit above them

Scope: `MapRenderer::setPostProcessEffect`, what a fragment shader gets to work with, how the
terrain depth reaches it, and how a layer opts out of being stylized. The relief (peak-finder) look
is the worked example; the shaded terrain surface it draws over is in
[04-terrain.md](04-terrain.md#the-surface-shader).

## The pipeline

With an effect attached, the frame is redirected (`MapRenderer::onDrawFrame`):

1. `clearAndBindScreenFBO` — sky, background and all layers render into an offscreen colour
   texture with a real depth buffer, instead of the screen.
2. `applyPostProcessEffect` — optionally renders the **terrain depth texture** first, then draws
   one full-screen quad with the effect's fragment shader.
3. With no opted-out layer, that quad goes straight to the screen and the frame is done.
   With one, it goes to the framebuffer's **secondary colour texture** (`FrameBuffer::
   attachSecondaryColorTex`) — same FBO, same depth attachment — the overlay layers are drawn on
   top of it by `drawOverlayLayers`, and `blendAndUnbindScreenFBO` blits the result out.

Step 3 is the only reason for the second texture: GL cannot read and write one texture in a pass,
and re-rendering the terrain depth into the default framebuffer to get overlays depth-tested would
cost a second terrain pass (~20 ms). Swapping the colour attachment keeps the depth buffer the
scene was drawn with, so an overlay is still occluded by the ridge in front of it.

Nothing above runs when no effect is set: the split in `drawLayers` is behind the
`postProcessing` flag, and the secondary texture is allocated on first use.

## What a shader gets

`PostProcessEffect(name, fragmentShader)` takes GLSL ES 1.00 source. Uniforms the renderer sets
when the shader declares them (queried with `glGetUniformLocation` + a `>= 0` guard — see
[03-vt-renderer.md](03-vt-renderer.md)):

| Uniform | Meaning |
|---|---|
| `sampler2D uColorTex` | the rendered frame, premultiplied alpha |
| `sampler2D uTerrainDepthTex` | packed terrain depth, only with `setTerrainDepthRequired(true)` |
| `vec2 uInvScreenSize` | 1/width, 1/height; screen uv is `gl_FragCoord.xy * uInvScreenSize` |
| `float uNear`, `uFar` | frustum distances, internal units |
| `vec2 uProjInvScale` | `tan(fovy/2)·aspect, tan(fovy/2)` |
| `float uTime` | seconds since the effect was attached |
| float / colour parameters | every `setFloatParameter` / `setColorParameter`, by name |

The depth texture is RGB = 24-bit linear eye depth relative to the far plane
(`dot(rgb, vec3(1, 1/255, 1/65025))`), A = terrain coverage (0 = sky). Eye position of a pixel:

```glsl
vec3 eyePos = vec3((uv * 2.0 - 1.0) * uProjInvScale, -1.0) * depth * uFar;
```

It is rendered by `TerrainRenderer::renderDepthTexture` at **half resolution**
(`BUFFER_DOWNSCALE = 2`) with nearest filtering, and — for the effect path only — at the terrain's
**full mesh resolution**. The occlusion read-back keeps the cheap 32-cell cap because it samples
points; an effect that draws *lines* from this depth would otherwise draw the depth mesh's own
triangulation, which is what the first attempt did (bright facets all over the near field).

## The relief outline effect

`PostProcessEffect::CreateReliefOutlineEffect()` — ink lines on paper over the shaded surface.
Three findings from making it match the reference (PeakFinder, and farfromrefug/geo-three):

- **Sample at least one depth texel apart.** With a step below `BUFFER_DOWNSCALE` pixels the four
  neighbour samples land on the same texel, the tangent vectors come out zero, and
  `normalize(vec3(0))` is undefined — it painted the entire near field flat grey. `uDepthTexelSize`
  is the floor.
- **A silhouette belongs to the nearer side.** Testing `abs(neighbour - depth)` draws every ridge
  twice, once on each side, and at the horizon the pairs merge into a black band. Only a neighbour
  *further away* counts.
- **Do not widen terrain-against-terrain lines with distance.** The obvious reading of "the horizon
  is bolder" smears the far ranges solid: up there ridges are a pixel apart, so a 4 px line covers
  everything. What is bold in a panorama is the **sky silhouette**, so only that test uses the wide
  radius (`uHorizonBoost`); the ridge and crease lines keep one width everywhere.

Ridge/valley lines come from the two tangent directions away from a pixel: opposite on a flat
surface (`dot = -1`), folded together over a crest. Computed on eye positions, not on depth, so a
merely oblique slope — which is most of a panorama — does not read as a fold.

## Layers above the effect

`Layer::setPostProcessed(false)` holds a layer back from the stylized pass. It is drawn after the
effect, into the same depth buffer, so annotations and sky-anchored objects
([13-celestial.md](13-celestial.md)) keep their own appearance while still going behind ridges.
Such a layer takes no part in the terrain prelude (depth-write assignment, cover, draping) — it is
an overlay, not a layer that paints the ground.

## Known limits

- The depth texture is half resolution, so lines are quantised at 2 px and slopes show occasional
  dotted artefacts. A full-resolution depth pass would fix it and doubles the depth pass cost.
- The effect resolves once per frame over the whole screen; layer-level effects do not exist.
- Verified on the emulator (Grenoble panorama, z13.2 tilt 25). Line quality on a device at high DPI
  has not been measured.
