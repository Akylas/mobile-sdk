#include "StyleEnvironment.h"
#include "components/TerrainOptions.h"
#include "components/LightOptions.h"
#include "utils/Const.h"

#include <cmath>

namespace carto {

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
        take(buildingLightIntensity, other.buildingLightIntensity);
        take(buildingAmbient, other.buildingAmbient);
        take(terrainLightingEnabled, other.terrainLightingEnabled);
        take(shadowStrength, other.shadowStrength);
        take(shadowBias, other.shadowBias);
        take(shadowSoftness, other.shadowSoftness);
        take(shadowDistance, other.shadowDistance);
        take(shadowMapSize, other.shadowMapSize);
        take(shadowCascades, other.shadowCascades);
        take(shadowCasterMargin, other.shadowCasterMargin);
        take(fogColor, other.fogColor);
        take(fogStartDistance, other.fogStartDistance);
        take(fogDistance, other.fogDistance);
        take(terrainMaxVisibleDistance, other.terrainMaxVisibleDistance);
    }

    bool StyleEnvironment::empty() const {
        return !(sunAzimuth || sunAltitude || sunColor || sunIntensity || ambientIntensity || buildingLightIntensity || buildingAmbient || terrainLightingEnabled ||
                 shadowStrength || shadowBias || shadowSoftness || shadowDistance || shadowMapSize || shadowCascades ||
                 shadowCasterMargin || fogColor || fogStartDistance || fogDistance || terrainMaxVisibleDistance);
    }

    ResolvedLighting resolveLighting(const std::shared_ptr<LightOptions>& lightOptions, const StyleEnvironment& env) {
        ResolvedLighting lighting;
        if (lightOptions) {
            lighting.terrainLightingEnabled = lightOptions->isTerrainLightingEnabled();
            lighting.sunDir = lightOptions->getSunDirection();
            lighting.sunColor = lightOptions->getSunColor();
            lighting.sunIntensity = lightOptions->getSunIntensity();
            lighting.ambientIntensity = lightOptions->getAmbientIntensity();
            lighting.shadowStrength = lightOptions->getShadowStrength();
            lighting.shadowBias = lightOptions->getShadowBias();
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
        if (env.terrainLightingEnabled) {
            lighting.terrainLightingEnabled = *env.terrainLightingEnabled;
        }
        // Buildings: the style wins when it says anything about them, whatever the terrain does.
        // Otherwise they follow the terrain sun, and with no sun at all they keep the legacy
        // model (intensity 0) - so a style that says nothing renders exactly as before.
        if (lighting.terrainLightingEnabled) {
            lighting.buildingLightIntensity = lighting.sunIntensity;
            lighting.buildingAmbient = lighting.ambientIntensity;
        }
        if (env.buildingLightIntensity) {
            lighting.buildingLightIntensity = *env.buildingLightIntensity;
        }
        if (env.buildingAmbient) {
            lighting.buildingAmbient = *env.buildingAmbient;
            if (!env.buildingLightIntensity && lighting.buildingLightIntensity <= 0.0f) {
                lighting.buildingLightIntensity = lighting.sunIntensity; // ambient alone still means "light them"
            }
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


    ResolvedFog resolveFog(const std::shared_ptr<TerrainOptions>& terrainOptions, const StyleEnvironment& env, const ResolvedLighting& lighting) {
        ResolvedFog fog;
        if (terrainOptions) {
            fog.color = terrainOptions->getFogColor();
            fog.startDistance = terrainOptions->getFogStartDistance();
            fog.distance = terrainOptions->getFogDistance();
        }
        if (env.fogColor) {
            fog.color = *env.fogColor;
        }
        if (env.fogStartDistance) {
            fog.startDistance = *env.fogStartDistance;
        }
        if (env.fogDistance) {
            fog.distance = *env.fogDistance;
        }

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
