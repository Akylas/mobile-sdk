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
- **Arcs** are line strips. A circle about an axis covers the useful case exactly: the sun's path
  across a day is the circle of constant declination about the celestial pole, so an application
  gives an axis and one angle rather than sampling positions through the day. An explicit
  direction list covers the rest.
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
star drawn half a pixel wide would be unhittable otherwise, hence `CelestialSprite::ClickRadius`.

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

**The camera cannot tilt above the horizon**, and that is a property of the camera model, not a
setting: tilt is clamped to `Const::MIN_SUPPORTED_TILT_ANGLE` (0) and `Options::setTiltRange`
clamps to it too. Lowering it was tried and the camera flips through the vertical - the view comes
back inverted, ground below and sky at both edges - because the up vector is derived from a focus
point on the ground. So the visible sky runs from the horizon up to roughly half the field of view.
A body higher than that is drawn correctly and can be clicked, but cannot be brought into view
without a first-person camera, which is a much larger change than this option.

## What lives in the app, not here

The demo's `DemoCelestial` is worth reading as the worked example: the sun's direction comes from
the SDK's own solar position (`LightOptions::setSunPositionFromTime`), so the sprite sits exactly
where the light comes from — visibly, the sun disc lands **on** its own arc — while the moon's
position and the arc's declination are computed in the app. Astronomy is application code; the
layer only knows about directions, sizes and colors.
