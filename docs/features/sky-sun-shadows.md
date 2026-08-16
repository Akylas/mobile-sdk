---
title: Sky, Sun & Shadows
sidebar_position: 7
---

# Sky, Sun and Shadows

One directional light and one shader sky, shared by the ground, the terrain, the buildings and the
fog — so the map, the horizon and the haze all agree on where the sun is.


<figure class="docs-figure">

![3D terrain lit by a low sun, under the shader sky](/img/features/sun-lighting.jpg)

<figcaption>Terrain lit by a sun 10° above the horizon (azimuth 100°), under the built-in sky gradient. Captured from the <code>scripts/android-dev</code> demo.</figcaption>

</figure>

## The sun

```kotlin
import com.massifmaps.components.LightOptions

val light = LightOptions().apply {
    sunAzimuth = 315f          // degrees clockwise from north; default 315 (NW)
    sunAltitude = 45f          // degrees above the horizon; default 45
    sunIntensity = 1.0f
    ambientIntensity = 0.35f
    isTerrainLightingEnabled = true   // shade the terrain surface with N·L
}
mapView.options.lightOptions = light
```

Instead of setting the angles, ask for a real sun position — NOAA's low-accuracy solar formula,
good to about 0.1°:

```kotlin
light.setSunPositionFromTime(2026, 8, 14, 7, 30, 45.188, 5.719)   // UTC + lat/lon
```

| Property | Default | Notes |
|---|---|---|
| `SunAzimuth` | `315` | Degrees clockwise from north. The classic cartographic light is NW. |
| `SunAltitude` | `45` | Degrees above the horizon, clamped `-90..90`. |
| `SunColor` / `SunIntensity` | white / `1.0` | Direct light. |
| `AmbientIntensity` | `0.35` | Light in the shadow. |
| `TerrainLightingEnabled` | `false` | Shade the terrain surface from its geometric normal. |
| `ShadowStrength` | `0.0` | `0` = no shadows. |
| `ShadowMapSize` / `ShadowCascades` | `1024` / `3` | Cascaded shadow map, up to 4 cascades. |
| `ShadowBias` / `ShadowSoftness` / `ShadowDistance` | `0.25` / `1.0` / `0` | `ShadowDistance` 0 = derived from the view. |
| `ShadowCasterMargin` | `3` | Ring of off-screen tiles that may still cast into the view. |

:::caution Terrain cast shadows are wired but off
The cascaded shadow map, the caster pass and the light boxes all exist, but casting onto the shared
ground is currently disabled in `MapRenderer::applyTerrainShadows` — with the pass on, the map reads
as shadow acne instead of shadows. `ShadowStrength` therefore affects 3D objects (buildings), not the
terrain surface. Sun *lighting* of the terrain (`TerrainLightingEnabled`) is unaffected.
:::

Two details worth knowing about the shadow model: the shadow pass floors the sun altitude at 15°
(a 9° sun throws a 4.4 km shadow off a 700 m hill and the cascade ladder goes useless), while N·L
lighting keeps the true sun; and the shadow map is snapped and cached, so a still camera re-renders
nothing.

## The sky

`SkyOptions` draws a single full-screen pass before everything else — one quad whatever the camera
does.

```kotlin
import com.massifmaps.components.SkyOptions
import com.massifmaps.graphics.Color

val sky = SkyOptions().apply {
    isEnabled = true
    skyColor = Color(0xFF3A74C4.toInt())      // zenith
    horizonColor = Color(0xFFABCEEC.toInt())
    horizonBlend = 12f                        // degrees of gradient around the horizon
    isSunDiscEnabled = true
}
mapView.options.skyOptions = sky
```

| Property | Default | Notes |
|---|---|---|
| `Enabled` | `true` | Off → the legacy sky bitmap band (`Options.setSkyColor`) is drawn instead. |
| `SkyColor` / `HorizonColor` | blue / pale blue | Zenith and horizon of the built-in gradient. |
| `GroundColor` | horizon color | Only shows in the wedge between the drawn map and the mathematical horizon. Transparent leaves the clear color. |
| `HorizonBlend` | `12` | Degrees of blend between horizon and sky. |
| `FogBlend` / `FogHorizon` | `12` / `-1` | How the fog band meets the sky; `-1` = derived from the terrain in view. |
| `SunDiscEnabled` | `true` | Draw the sun disc and its glow. |
| `ShaderSource` | — | Replace the whole appearance (below). |

### A custom sky shader

`setShaderSource` takes GLSL ES 1.00 that defines one function:

```glsl
vec4 skyColor(vec3 rayDir) {          // rayDir: world-space view ray, x east, y north, z up
    float t = clamp(rayDir.z, 0.0, 1.0);
    vec4 c = mix(u_horizonColor, u_skyColor, t);
    return mix(c, vec4(u_fogColor.rgb, 1.0), fogAmount(rayDir));
}
```

The wrapper already declares the uniforms and the `fogAmount(rayDir)` helper — **redeclaring any of
them is a compile error and the renderer silently falls back to the built-in sky**, which is the
usual reason a custom sky "does nothing".

| Uniform | Meaning |
|---|---|
| `vec3 u_sunDir` | unit vector towards the sun, world space |
| `vec4 u_sunColor`, `float u_sunIntensity` | from `LightOptions` |
| `vec4 u_skyColor`, `u_horizonColor`, `u_groundColor` | the configured colours |
| `float u_horizonBlend`, `u_sunDisc` | the configured blend / sun-disc switch |
| `vec4 u_fogColor`, `float u_fogBlend`, `u_fogHorizon` | the resolved fog, already lit by the sun |
| `float u_time`, `u_zoom`, `u_cameraHeight` | seconds since the view was created, fractional zoom, metres |
| `vec2 u_resolution` | viewport size in pixels |

## Fog

Fog is not a separate subsystem: it is resolved once per frame from `TerrainOptions` and the style's
`Map` block, **lit by the same sun** (dark at night, warm at a low sun), and handed to the ground
shaders, the background plane and the sky. That single resolution point is why the ground and the
horizon match.

```kotlin
terrain.fogColor = Color(0xFFB8C6D0.toInt())   // alpha = how opaque the fog gets; transparent = no fog
terrain.fogStartDistance = 8000f               // metres from the camera; nothing nearer is fogged
terrain.fogDistance = 40000f                   // metres at which it reaches full strength; 0 = off
terrain.viewDistanceFactor = 1.0f              // where the ground ends — pair it with fog
```

A style can set the same three from its `Map` block (`fog-color`, `fog-start-distance`,
`fog-distance`); the style wins where it has an opinion.

`ViewDistanceFactor` ends the ground (tangram's rule: `2 × camera height / cos(pitch + fovy/2)`,
capped at 127 tile widths; `1` = their rule verbatim). Without fog it ends on a hard edge.

## See also

- [3D Terrain](/docs/features/3d-terrain) — the surface all of this lights.
- [Objects in the Sky](/docs/features/celestial-objects) — a sun disc, a moon, stars, an aircraft.
- [Post-processing Effects](/docs/features/post-processing) — full-screen shaders over the lit scene.
