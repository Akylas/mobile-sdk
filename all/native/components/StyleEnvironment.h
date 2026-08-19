/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_STYLEENVIRONMENT_H_
#define _MASSIF_STYLEENVIRONMENT_H_

#include "graphics/Color.h"

#include <memory>
#include <optional>

#include <cglib/vec.h>

namespace massif {
    class TerrainOptions;
    class LightOptions;
    class FogOptions;

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
        std::optional<Color> ambientColor;
        std::optional<float> buildingLightIntensity;
        std::optional<float> buildingAmbient;
        std::optional<float> buildingVerticalGradient;
        std::optional<bool> terrainLightingEnabled;
        std::optional<float> shadowStrength;
        std::optional<float> shadowBias;
        std::optional<float> shadowSoftness;
        std::optional<float> shadowDistance;
        std::optional<int> shadowMapSize;
        std::optional<int> shadowCascades;
        std::optional<int> shadowCasterMargin;
        std::optional<bool> fogEnabled;
        std::optional<Color> fogColor;
        std::optional<float> fogRangeStart;
        std::optional<float> fogRangeEnd;
        std::optional<Color> fogHighColor;
        std::optional<Color> fogSpaceColor;
        std::optional<float> fogHorizonBlend;
        std::optional<float> fogStarIntensity;
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
        Color ambientColor = Color(255, 255, 255, 255);
        // What the 3D extrusions light with: the same normalised Lambert the terrain surface uses,
        // so walls and the ground agree. Follows the sun unless the style overrides it.
        float buildingLightIntensity = 1.0f;
        float buildingAmbient = 0.35f;
        // How dark the foot of a wall goes, as a fraction of its colour, measured ALONG the wall.
        float buildingVerticalGradient = 0.65f;
        float shadowStrength = 0.0f;
        float shadowBias = 0.25f;
        float shadowNormalOffset = 3.0f;
        float shadowSoftness = 1.0f;
        float shadowDistance = 0.0f;
        int shadowMapSize = 1024;
        int shadowCascades = 3;
        int shadowCasterMargin = 3;
    };

    ResolvedLighting resolveLighting(const std::shared_ptr<LightOptions>& lightOptions, const StyleEnvironment& env);

    /**
     * The distance fog to actually render with: FogOptions, with every value the style defines
     * substituted in, and the colour lit by the sun when terrain lighting is on.
     * The API expresses the range in multiples of the camera-to-focus distance; the distances
     * here are the resolved product, in INTERNAL units, which is what every shader wants.
     */
    struct ResolvedFog {
        Color color = Color(0, 0, 0, 0);
        Color highColor = Color(0, 0, 0, 0);
        Color spaceColor = Color(0, 0, 0, 0);
        float rangeStart = 0.0f; // multiples of the camera-to-focus distance, as the API states it
        float rangeEnd = 0.0f;
        float rangeScale = 1.0f; // internal units per range unit
        float startDistance = 0.0f; // rangeStart * rangeScale, i.e. internal units
        float distance = 0.0f;
        float horizonBlend = 0.0f;
        float horizonAngle = -1.0f;
        float starIntensity = 0.0f;

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
     *
     * cameraDistance is ViewState::calculateCameraDistance() in internal units, which the range is
     * measured in. It is a function of the zoom alone, so one range setting holds at every zoom.
     */
    ResolvedFog resolveFog(const std::shared_ptr<FogOptions>& fogOptions, const StyleEnvironment& env, const ResolvedLighting& lighting, double cameraDistance);

}

#endif
