#include "StyleEnvironment.h"
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
        return !(sunAzimuth || sunAltitude || sunColor || sunIntensity || ambientIntensity || terrainLightingEnabled ||
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

}
