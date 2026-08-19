#include "StyleEnvironment.h"
#include "components/TerrainOptions.h"
#include "components/LightOptions.h"
#include "components/FogOptions.h"
#include "utils/Const.h"

#include <algorithm>
#include <cmath>

namespace massif {

    void StyleEnvironment::mergeMissing(const StyleEnvironment& other) {
        auto take = [](auto& value, const auto& otherValue) {
            if (!value) {
                value = otherValue;
            }
        };
        take(sunAzimuth, other.sunAzimuth);
        take(sunAltitude, other.sunAltitude);
        take(sunColor, other.sunColor);
        take(sunIntensity, other.sunIntensity);
        take(ambientIntensity, other.ambientIntensity);
        take(ambientColor, other.ambientColor);
        take(buildingLightIntensity, other.buildingLightIntensity);
        take(buildingAmbient, other.buildingAmbient);
        take(buildingVerticalGradient, other.buildingVerticalGradient);
        take(buildingRoofShade, other.buildingRoofShade);
        take(buildingAoIntensity, other.buildingAoIntensity);
        take(buildingAoGroundAttenuation, other.buildingAoGroundAttenuation);
        take(terrainLightingEnabled, other.terrainLightingEnabled);
        take(shadowStrength, other.shadowStrength);
        take(shadowBias, other.shadowBias);
        take(shadowSoftness, other.shadowSoftness);
        take(shadowDistance, other.shadowDistance);
        take(shadowMapSize, other.shadowMapSize);
        take(shadowCascades, other.shadowCascades);
        take(shadowCasterMargin, other.shadowCasterMargin);
        take(fogEnabled, other.fogEnabled);
        take(fogColor, other.fogColor);
        take(fogRangeStart, other.fogRangeStart);
        take(fogRangeEnd, other.fogRangeEnd);
        take(fogHighColor, other.fogHighColor);
        take(fogSpaceColor, other.fogSpaceColor);
        take(fogHorizonBlend, other.fogHorizonBlend);
        take(fogStarIntensity, other.fogStarIntensity);
        take(terrainMaxVisibleDistance, other.terrainMaxVisibleDistance);
    }

    bool StyleEnvironment::empty() const {
        return !(sunAzimuth || sunAltitude || sunColor || sunIntensity || ambientIntensity || ambientColor || buildingLightIntensity || buildingAmbient || buildingVerticalGradient || buildingRoofShade || buildingAoIntensity || buildingAoGroundAttenuation || terrainLightingEnabled ||
                 shadowStrength || shadowBias || shadowSoftness || shadowDistance || shadowMapSize || shadowCascades ||
                 shadowCasterMargin || fogEnabled || fogColor || fogRangeStart || fogRangeEnd || fogHighColor || fogSpaceColor ||
                 fogHorizonBlend || fogStarIntensity || terrainMaxVisibleDistance);
    }

    ResolvedLighting resolveLighting(const std::shared_ptr<LightOptions>& lightOptions, const StyleEnvironment& env) {
        ResolvedLighting lighting;
        if (lightOptions) {
            lighting.terrainLightingEnabled = lightOptions->isTerrainLightingEnabled();
            lighting.sunDir = lightOptions->getSunDirection();
            lighting.sunColor = lightOptions->getSunColor();
            lighting.sunIntensity = lightOptions->getSunIntensity();
            lighting.ambientIntensity = lightOptions->getAmbientIntensity();
            lighting.ambientColor = lightOptions->getAmbientColor();
            lighting.shadowStrength = lightOptions->getShadowStrength();
            lighting.shadowBias = lightOptions->getShadowBias();
        lighting.shadowNormalOffset = lightOptions->getShadowNormalOffset();
            lighting.shadowSoftness = lightOptions->getShadowSoftness();
            lighting.shadowDistance = lightOptions->getShadowDistance();
            lighting.shadowMapSize = lightOptions->getShadowMapSize();
            lighting.shadowCascades = lightOptions->getShadowCascades();
            lighting.shadowCasterMargin = lightOptions->getShadowCasterMargin();
        }
        // The sun direction is derived from two properties, so it is rebuilt whenever the style
        // overrides either of them - the other one then comes from the options.
        if (env.sunAzimuth || env.sunAltitude) {
            double azimuth = (env.sunAzimuth ? *env.sunAzimuth : (lightOptions ? lightOptions->getSunAzimuth() : 315.0f)) * Const::DEG_TO_RAD;
            double altitude = (env.sunAltitude ? *env.sunAltitude : (lightOptions ? lightOptions->getSunAltitude() : 45.0f)) * Const::DEG_TO_RAD;
            double cosAltitude = std::cos(altitude);
            lighting.sunDir = cglib::vec3<float>(static_cast<float>(cosAltitude * std::sin(azimuth)),
                                                 static_cast<float>(cosAltitude * std::cos(azimuth)),
                                                 static_cast<float>(std::sin(altitude)));
        }
        if (env.sunColor) {
            lighting.sunColor = *env.sunColor;
        }
        if (env.sunIntensity) {
            lighting.sunIntensity = *env.sunIntensity;
        }
        if (env.ambientIntensity) {
            lighting.ambientIntensity = *env.ambientIntensity;
        }
        if (env.ambientColor) {
            lighting.ambientColor = *env.ambientColor;
        }
        if (env.terrainLightingEnabled) {
            lighting.terrainLightingEnabled = *env.terrainLightingEnabled;
        }
        // Buildings follow the sun unconditionally - terrainLightingEnabled decides whether the
        // GROUND is lit, and gating the walls on it too gave the extrusions a second lighting
        // model that changed shape as the terrain was toggled.
        lighting.buildingLightIntensity = lighting.sunIntensity;
        // Their AMBIENT is their own, though, and does not follow the ground's. Ambient is the
        // floor the directional term is added on top of, so an app that flattens the ground with
        // ambient 1 - a normal thing to do when a hillshade layer supplies the relief - would
        // flatten every facade with it, and a building with no side shading does not read as 3D at
        // all. mapbox's fill-extrusion shades from its own light intensity for the same reason.
        // A style ties them back together with 'building-ambient' when it wants that.
        if (env.buildingLightIntensity) {
            lighting.buildingLightIntensity = *env.buildingLightIntensity;
        }
        if (env.buildingAmbient) {
            lighting.buildingAmbient = *env.buildingAmbient;
        }
        if (env.buildingVerticalGradient) {
            lighting.buildingVerticalGradient = *env.buildingVerticalGradient;
        }
        if (env.buildingRoofShade) {
            lighting.buildingRoofShade = *env.buildingRoofShade;
        }
        if (env.buildingAoIntensity) {
            lighting.buildingAoIntensity = *env.buildingAoIntensity;
        }
        if (env.buildingAoGroundAttenuation) {
            lighting.buildingAoGroundAttenuation = *env.buildingAoGroundAttenuation;
        }
        if (env.shadowStrength) {
            lighting.shadowStrength = *env.shadowStrength;
        }
        if (env.shadowBias) {
            lighting.shadowBias = *env.shadowBias;
        }
        if (env.shadowSoftness) {
            lighting.shadowSoftness = *env.shadowSoftness;
        }
        if (env.shadowDistance) {
            lighting.shadowDistance = *env.shadowDistance;
        }
        if (env.shadowMapSize) {
            lighting.shadowMapSize = *env.shadowMapSize;
        }
        if (env.shadowCascades) {
            lighting.shadowCascades = *env.shadowCascades;
        }
        if (env.shadowCasterMargin) {
            lighting.shadowCasterMargin = *env.shadowCasterMargin;
        }
        return lighting;
    }


    ResolvedFog resolveFog(const std::shared_ptr<FogOptions>& fogOptions, const StyleEnvironment& env, const ResolvedLighting& lighting, double cameraDistance) {
        ResolvedFog fog;
        if (!fogOptions) {
            return fog;
        }
        // The switch comes first. Unlike every value below, it is ANDed rather than overridden:
        // the style saying "fog" must not re-enable a fog the application switched off, which is
        // what an app-side UI toggle means. A default-constructed ResolvedFog is not active(), so
        // every consumer stops fogging together without any value being driven to zero.
        if (!fogOptions->isEnabled() || (env.fogEnabled && !*env.fogEnabled)) {
            return fog;
        }
        float rangeStart = fogOptions->getRangeStart();
        float rangeEnd = fogOptions->getRangeEnd();
        fog.color = fogOptions->getColor();
        fog.highColor = fogOptions->getHighColor();
        fog.spaceColor = fogOptions->getSpaceColor();
        fog.horizonBlend = fogOptions->getHorizonBlend();
        fog.horizonAngle = fogOptions->getHorizonAngle();
        fog.starIntensity = fogOptions->getStarIntensity();
        if (env.fogColor) {
            fog.color = *env.fogColor;
        }
        if (env.fogRangeStart) {
            rangeStart = *env.fogRangeStart;
        }
        if (env.fogRangeEnd) {
            rangeEnd = *env.fogRangeEnd;
        }
        if (env.fogHighColor) {
            fog.highColor = *env.fogHighColor;
        }
        if (env.fogSpaceColor) {
            fog.spaceColor = *env.fogSpaceColor;
        }
        if (env.fogHorizonBlend) {
            fog.horizonBlend = *env.fogHorizonBlend;
        }
        if (env.fogStarIntensity) {
            fog.starIntensity = *env.fogStarIntensity;
        }
        // The range is in multiples of the camera-to-focus distance - a function of the zoom
        // alone, so a style tuned once holds at every zoom instead of needing an expression.
        fog.rangeStart = rangeStart;
        fog.rangeEnd = rangeEnd;
        fog.rangeScale = static_cast<float>(std::max(1.0e-9, cameraDistance));
        fog.startDistance = rangeStart * fog.rangeScale;
        fog.distance = rangeEnd * fog.rangeScale;

        // Light the fog. Haze is lit air: at noon it is the bright band the reference renderers
        // show at the horizon, at night it is a dark one, and near sunset it takes the sun's
        // colour. The scale is the same light the ground gets (ambient plus the sun once it is
        // above the horizon), so fog and terrain darken together instead of the fog floating over
        // a black map. The tint is applied in proportion to how much of that light is direct sun.
        if (lighting.terrainLightingEnabled && fog.color.getA() > 0) {
            float sunUp = std::max(0.0f, std::min(1.0f, lighting.sunDir(2)));
            float direct = std::max(0.0f, lighting.sunIntensity) * sunUp;
            float light = std::max(0.0f, std::min(1.0f, std::max(0.0f, lighting.ambientIntensity) + direct));
            float sunShare = direct > 0.0f ? std::min(1.0f, direct / std::max(1.0e-3f, light)) : 0.0f;
            auto channel = [&](int value, int sunValue) {
                float tint = 1.0f + sunShare * (sunValue / 255.0f - 1.0f);
                return static_cast<unsigned char>(std::max(0.0f, std::min(255.0f, value * light * tint)));
            };
            fog.color = Color(channel(fog.color.getR(), lighting.sunColor.getR()),
                              channel(fog.color.getG(), lighting.sunColor.getG()),
                              channel(fog.color.getB(), lighting.sunColor.getB()),
                              fog.color.getA());
        }
        return fog;
    }

}
