# Objects in the sky

Scope: `all/native/celestial/`, `all/native/layers/CelestialLayer.*`,
`all/native/renderers/CelestialRenderer.*`. The sky *itself* — gradient, sun disc, fog — is
[08-lighting-sky-fog.md](08-lighting-sky-fog.md); this page is about objects placed **in** it.

Everything the map draws is anchored to a position on the ground. A sun, a moon, a star, an
aircraft overhead are not: they belong to a direction, or to a point in the air. `CelestialLayer`
is the layer for those, and it is deliberately generic — **the SDK has no notion of a sun or a
moon**. The demo's `DemoCelestial` builds both out of two sprites and an arc, and the same API
carries a satellite track or a star catalogue with no SDK change.

## Anchors

An object is placed one of two ways (`CelestialObject`):

- **By direction** — azimuth and altitude in degrees, plus a distance. Distance **0 means
  infinitely far**: the object keeps its direction whatever the camera does, so it never
  parallaxes when the map pans. That is what the sun, the moon and the stars need.
  A finite distance gives real parallax, for something that is merely high up.
- **By geographic position + altitude** — for an object that belongs to a place on the map but is
  above it: an aircraft, a satellite pass.

Directions use the same frame as `LightOptions::getSunDirection` — x east, y north, z up, azimuth
clockwise from north — so an application can hand a computed sun direction straight over. The
renderer converts through `ProjectionSurface::calculateVector` at the focus point, so it is correct
on a sphere as well as on a plane.

## Drawing

`CelestialRenderer` draws with the layer, in layer order. **Add the layer first** and the map and
the terrain then draw over it, which is what a body in the sky wants: a ridge in front of the sun
hides it, for free, with no extra work.

- **Sprites** are camera-facing quads expanded from the view matrix's right/up, sized either by
  **angle** (a real body: the sun and the moon are both about 0.5°) or in **screen pixels** (an
  icon). Batched **per bitmap**, so any number of objects sharing one bitmap — or none — is a
  single draw call. With no bitmap the fragment shader draws a soft disc, which costs no texture
  at all and is enough for a sun, a moon or a star.
- **Arcs** are line strips. A circle about an axis covers the useful case exactly: the daily path
  of a distant body is the circle of constant declination about the rotation axis, so an
  application gives an axis and one angle rather than sampling positions through the day. An
  explicit direction list covers the rest, and `setSegments` reads that list as **disjoint pairs**
  instead of a path — a figure drawn between fixed directions (the demo's constellation lines) is
  then ONE object: one draw call, one clickable thing, one name.
- **Depth**: depth-TESTED, never depth-WRITING. An infinitely distant object is parked just inside
  the far plane, so everything the map draws is nearer and covers it, and it never occludes
  anything itself.
- Objects are sorted **far to near** before drawing so overlapping discs blend in the expected
  order.

## Clicking, and the gap it exposed

Objects are hit-tested against the touch ray through the standard layer path
(`calculateRayIntersectedElements` / `processClick`), so they sort against every other layer's
content and a click on terrain in front of the sun does not report the sun. The test is **angular**
— how far the ray is from the direction the object sits in — which is the natural measure here; a
sprite drawn half a pixel wide would be unhittable otherwise, hence `CelestialSprite::ClickRadius`.
Arcs are hit the same way, against the nearest point of the curve (`CelestialArc::ClickRadius`,
0 = not clickable). Every arc sits at the same distance, so the reported hit is pushed out by the
angle it was missed by — otherwise two overlapping curves would be picked between by list order,
and a sprite drawn on a curve would lose to it.

This needed a fix in `TouchHandler::handleClick`, and it applies to any sky content, not just this
layer: a touch was **dropped entirely** unless its ray met the ground (`isValidScreenPosition`),
because the click path was built around a map position. A tap aimed at the sky has no map position
but still has a ray, so the layers are now asked with the ray alone in that case (`MapRenderer`
gained the matching overload). Only the map-position callback is skipped — there is nothing to
report.

## Seeing them: free roam

Sky content is normally off the top of the screen, because the map camera points at the ground.
`Options::FreeRoam` changes what a one-finger drag does: sideways turns the heading, up and down
changes the tilt, and panning moves to a two-finger drag. Pinch still zooms; the two-finger paths
are untouched.

## Looking above the horizon: a negative tilt

The tilt may now go **below 0**, and that is what "look up" is. It is opt-in: `MIN_SUPPORTED_TILT_ANGLE`
is -90 but the default tilt range is still `(0, 90)`, so a map only gets there if it asks —
`setTiltRange(MapRange(-90, 90))`.

The model matters, because the obvious version does not work. Tilting between 90 and 0 rotates the
camera **about the focus**; carrying that on below 0 puts the camera under the ground and the view
comes back inverted (ground below, sky at both edges), because the up vector is derived from a
focus point on the ground. That was tried, and it is why the ceiling stood.

What a negative tilt does instead: the camera **stays exactly where the tilt geometry left it** and
only the view direction pitches up, about the camera (`ViewState::calculateLookatMat`, and
`getGroundTilt()` — the tilt floored at 0 — is what positions the camera). `dist(camera, focus)` is
untouched, so zoom, the visible tile set and the near/far budget all still mean what they meant.
`CameraTiltEvent` spends only the part of the tilt at or above the horizon on moving the camera.

Two consequences had to be handled:

- **All-sky frames have no ground.** `calculateViewDistances` walks rays to the ground for near and
  far; with none of them hitting, it used to leave `far == near` and collapse the depth range onto
  the near plane — taking with it everything drawn into the sky, which parks just inside the far
  plane. It now falls back to the view distance the ground would have been drawn to.
- **The terrain clearance bound goes unsatisfiable.** The camera's height above the focus is
  `dist * sin(tilt)`, so approaching the horizon it rises by almost nothing as it zooms out and
  `ViewState::getTerrainMaxZoom` runs off to minus infinity. Clamping to that threw the map from
  z16 to its minimum zoom in a few frames (and, with the per-frame correction in `MapRenderer`,
  kept walking it out to z-33). Both now drop the bound when the camera is at or below the focus
  height, or when the bound lands below the zoom range: no zoom clears the terrain at that tilt
  anyway, and keeping the camera the user asked for beats emptying the world.

At tilt ≤ 0 the camera sits at the height of its focus, i.e. on the ground — which is exactly right
for looking at the sky, and means the terrain is seen edge-on at the horizon.

## What lives in the app, not here

The demo is worth reading as the worked example, and it is where all the astronomy lives:
`DemoAstro` (sun, moon and its phase, planets from JPL's Keplerian elements), `DemoStarCatalogue`
(bright stars + constellation figures), `DemoCelestial` (sun and moon with their real paths for the
day, sampled from the same ephemeris — visibly, each disc lands **on** its own arc, which is a free
check on both since they are computed independently) and `DemoStars` (one sprite per star, one
segmented arc per figure, planets). The layer only knows about directions, sizes and colors.

The demo's star sky mode is also the answer to "draw nothing but the sky": the map layers leave the
layer list entirely (not hidden — never built), the terrain is disabled and the clear colour goes
fully transparent, so with a translucent surface whatever is behind the view shows through.
