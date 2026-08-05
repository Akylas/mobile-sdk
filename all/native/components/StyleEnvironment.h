/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_STYLEENVIRONMENT_H_
#define _CARTO_STYLEENVIRONMENT_H_

#include "graphics/Color.h"

#include <memory>
#include <optional>

#include <cglib/vec.h>

namespace carto {
    class TerrainOptions;
    class LightOptions;

    /**
     * The sun, shadow, fog and terrain-distance values a vector tile style provides in its Map
     * block. Every field is optional: unset means the style said nothing about it and the
     * application's own LightOptions/TerrainOptions setting stands.
     *
     * The values are evaluated per frame from the style expressions, so any of them may be
     * zoom-dependent (linear(), step functions, whatever the style writes).
     *
     * Internal class, not exposed through the public API.
     */
    struct StyleEnvironment {
        std::optional<float> sunAzimuth;
        std::optional<float> sunAltitude;
        std::optional<Color> sunColor;
        std::optional<float> sunIntensity;
        std::optional<float> ambientIntensity;
        std::optional<bool> terrainLightingEnabled;
        std::optional<float> shadowStrength;
        std::optional<float> shadowBias;
        std::optional<float> shadowSoftness;
        std::optional<float> shadowDistance;
        std::optional<int> shadowMapSize;
        std::optional<int> shadowCascades;
        std::optional<int> shadowCasterMargin;
        std::optional<Color> fogColor;
        std::optional<float> fogStartDistance;
        std::optional<float> fogDistance;
        std::optional<float> terrainMaxVisibleDistance;

        /**
         * Takes over every value the other environment defines and this one does not. Used to
         * merge several layers' styles: the first layer that says something about a property wins.
         */
        void mergeMissing(const StyleEnvironment& other);

        bool empty() const;
    };

    /**
     * The lighting to actually render with: the application's LightOptions, with every value the
     * style defines substituted in.
     */
    struct ResolvedLighting {
        bool terrainLightingEnabled = false;
        cglib::vec3<float> sunDir = cglib::vec3<float>(0, 0, 1);
        Color sunColor = Color(255, 255, 255, 255);
        float sunIntensity = 1.0f;
        float ambientIntensity = 0.35f;
        float shadowStrength = 0.0f;
        float shadowBias = 0.25f;
        float shadowSoftness = 1.0f;
        float shadowDistance = 0.0f;
        int shadowMapSize = 1024;
        int shadowCascades = 3;
        int shadowCasterMargin = 3;
    };

    ResolvedLighting resolveLighting(const std::shared_ptr<LightOptions>& lightOptions, const StyleEnvironment& env);

    /**
     * The distance fog to actually render with: TerrainOptions, with every value the style
     * defines substituted in, and the colour lit by the sun when terrain lighting is on.
     * Distances are in meters, as in the API.
     */
    struct ResolvedFog {
        Color color = Color(0, 0, 0, 0);
        float startDistance = 0.0f;
        float distance = 0.0f;

        /**
         * True when there is a fog to draw at all: a visible colour over a positive range.
         */
        bool active() const { return color.getA() > 0 && distance > startDistance; }
    };

    /**
     * Resolves the fog and lights it: fog is air, so it is as bright as the light falling on it.
     * Without this a fog tuned for daylight stays bright white through the night, floating over a
     * dark map. Only applied when terrain lighting is on - otherwise there is no sun to speak of
     * and the configured colour is used as-is.
     */
    ResolvedFog resolveFog(const std::shared_ptr<TerrainOptions>& terrainOptions, const StyleEnvironment& env, const ResolvedLighting& lighting);

}

#endif
