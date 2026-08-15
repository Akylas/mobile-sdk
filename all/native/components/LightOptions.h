/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_LIGHTOPTIONS_H_
#define _MASSIF_LIGHTOPTIONS_H_

#include "graphics/Color.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <cglib/vec.h>

namespace massif {

    /**
     * Directional light (sun) configuration, attached to the map via Options::setLightOptions.
     * The sun direction drives the sky shader, terrain surface lighting and shadows.
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class LightOptions {
    public:
        /**
         * Interface for monitoring light option change events. Internal.
         */
        struct OnChangeListener {
            virtual ~OnChangeListener() { }

            /**
             * Listener method that gets called when a light option has changed.
             * @param optionName The name of the option that has changed.
             */
            virtual void onLightOptionChanged(const std::string& optionName) = 0;
        };

        /**
         * Constructs a LightOptions object with default values.
         */
        LightOptions();
        virtual ~LightOptions();

        /**
         * Returns the sun azimuth in degrees.
         * @return The sun azimuth in degrees, clockwise from north. The default is 315 (north-west).
         */
        float getSunAzimuth() const;
        /**
         * Sets the sun azimuth in degrees, measured clockwise from north (0 = north, 90 = east).
         * The classic cartographic hillshade light comes from the north-west, which is the default.
         * @param azimuth The new sun azimuth in degrees.
         */
        void setSunAzimuth(float azimuth);

        /**
         * Returns the sun altitude in degrees above the horizon.
         * @return The sun altitude in degrees. The default is 45.
         */
        float getSunAltitude() const;
        /**
         * Sets the sun altitude in degrees above the horizon (0 = at the horizon, 90 = zenith).
         * Negative values put the sun below the horizon (night).
         * @param altitude The new sun altitude in degrees (clamped to -90..90).
         */
        void setSunAltitude(float altitude);

        /**
         * Sets the sun position from a date, a time and a location, using the standard
         * solar position algorithm. This is a convenience wrapper that computes and stores
         * the azimuth and the altitude; reading them back returns the computed values.
         * @param year The year (for example 2026).
         * @param month The month, 1..12.
         * @param day The day of the month, 1..31.
         * @param hour The hour in UTC, 0..23.
         * @param minute The minute, 0..59.
         * @param latitude The observer latitude in degrees.
         * @param longitude The observer longitude in degrees.
         */
        void setSunPositionFromTime(int year, int month, int day, int hour, int minute, double latitude, double longitude);

        /**
         * Returns the sun (directional light) color.
         * @return The sun color. The default is white.
         */
        Color getSunColor() const;
        /**
         * Sets the sun (directional light) color.
         * @param color The new sun color.
         */
        void setSunColor(const Color& color);

        /**
         * Returns the sun light intensity.
         * @return The sun intensity. The default is 1.
         */
        float getSunIntensity() const;
        /**
         * Sets the sun light intensity, a multiplier on the directional contribution.
         * @param intensity The new sun intensity (clamped to 0..8).
         */
        void setSunIntensity(float intensity);

        /**
         * Returns the ambient light intensity.
         * @return The ambient intensity. The default is 0.35.
         */
        float getAmbientIntensity() const;
        /**
         * Sets the ambient light intensity, the amount of light reaching surfaces that face
         * away from the sun. This is also the brightness floor inside shadows.
         * @param intensity The new ambient intensity (clamped to 0..1).
         */
        void setAmbientIntensity(float intensity);

        /**
         * Returns whether the sun lights the 3D terrain surface.
         * @return True if terrain surface lighting is enabled. The default is false.
         */
        bool isTerrainLightingEnabled() const;
        /**
         * Sets whether the sun lights the 3D terrain surface. When enabled, the terrain
         * surface shader computes the slope from the elevation data and shades the map with
         * the current sun position - a live hillshade that follows the time of day, replacing
         * the pre-baked hillshade raster layer for the common case. Requires 3D terrain with
         * draping enabled (TerrainOptions.setDrapeFillsEnabled).
         * @param enabled True to light the terrain surface with the sun.
         */
        void setTerrainLightingEnabled(bool enabled);

        /**
         * Returns the shadow strength.
         * @return The shadow strength. The default is 0 (no shadows).
         */
        float getShadowStrength() const;
        /**
         * Sets how strongly the sun's shadows darken the terrain, 0 (off) to 1 (full).
         * Shadows are cast by the terrain itself onto the terrain, so ridges shade valleys
         * at low sun. Requires terrain lighting.
         * @param strength The new shadow strength (clamped to 0..1).
         */
        void setShadowStrength(float strength);

        /**
         * Returns the shadow map resolution.
         * @return The shadow map size in pixels, per cascade. The default is 1024.
         */
        int getShadowMapSize() const;
        /**
         * Sets the shadow map resolution in pixels, per cascade. Higher is sharper and costs
         * more memory (size * size * 4 bytes per cascade) and fill rate. The cascades share one
         * texture, so the size is clamped to what fits: 4096 / cascades.
         * @param size The new shadow map size (clamped to 256..4096 / cascades).
         */
        void setShadowMapSize(int size);

        /**
         * Returns the number of shadow cascades.
         * @return The cascade count. The default is 3.
         */
        int getShadowCascades() const;
        /**
         * Sets how many shadow map cascades are rendered (1 to 4). One map has to cover
         * everything visible, so at a tilt its texels are metres of ground and shadow edges
         * become staircases. Cascades split the view distance: the near one covers a small
         * region with the same number of texels, the far one - where a screen pixel is tens of
         * metres of ground anyway - keeps the coarse cover. Each cascade costs one more caster
         * pass and one more page of shadow texture.
         * @param cascades The new cascade count (clamped to 1..4).
         */
        void setShadowCascades(int cascades);

        /**
         * Returns the shadow distance in meters.
         * @return The shadow distance. The default is 0 (cover everything visible).
         */
        float getShadowDistance() const;
        /**
         * Sets the radius around the camera focus, in meters, that the shadow map covers.
         * The shadow map has a fixed resolution, so the larger the ground it spans the coarser
         * its texels - which is why shadows look sharp looking straight down and pixelated at a
         * low tilt, where the visible ground reaches to the horizon. Limiting the distance keeps
         * the texels small; ground beyond it simply has no shadows. 0 covers everything visible.
         * @param distance The new shadow distance in meters.
         */
        void setShadowDistance(float distance);

        /**
         * Returns the shadow caster margin in tiles.
         * @return The caster margin. The default is 3.
         */
        int getShadowCasterMargin() const;
        /**
         * Sets how many tiles beyond the visible ones are rendered as shadow casters. A mountain
         * just off screen still casts its shadow into the view, and without a margin that shadow
         * disappears as you zoom in and the mountain leaves the visible set. Costs one caster
         * draw per extra tile.
         * @param margin The new caster margin in tiles (clamped to 0..8).
         */
        void setShadowCasterMargin(int margin);

        /**
         * Returns the shadow softness.
         * @return The PCF radius in shadow-map texels. The default is 1.
         */
        float getShadowSoftness() const;
        /**
         * Sets the shadow edge softness, as a radius in shadow-map texels. Larger values blur the
         * shadow edges, which also hides the stair-stepping of a low-resolution shadow map.
         * @param softness The new softness (clamped to 0..8).
         */
        void setShadowSoftness(float softness);

        /**
         * Returns the shadow depth bias.
         * @return The shadow depth bias in meters. The default is 0.5.
         */
        float getShadowBias() const;
        /**
         * Sets the shadow depth bias in meters: the depth slack that keeps a lit surface from
         * shadowing itself. Too small gives acne (dark speckle on lit slopes), too large detaches
         * shadows from what casts them. It is metric on purpose - expressed as a fraction of the
         * light frustum it would grow with the shadowed area, and the shadow would drift away
         * from its caster as the view zoomed out.
         * @param bias The new shadow bias.
         */
        void setShadowBias(float bias);

        /**
         * Returns the sun direction as a unit vector in internal map coordinates.
         * The vector points from the surface *towards* the sun. Internal method.
         * @return The unit sun direction.
         */
        cglib::vec3<float> getSunDirection() const;

        /**
         * Registers listener for light option change events. Internal method.
         * @param listener The listener for change events.
         */
        void registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);
        /**
         * Unregisters listener from light option change events. Internal method.
         * @param listener The previously added listener.
         */
        void unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);

    private:
        void notifyOptionChanged(const std::string& optionName);

        std::atomic<float> _sunAzimuth;
        std::atomic<float> _sunAltitude;
        std::atomic<int> _sunColorARGB;
        std::atomic<float> _sunIntensity;
        std::atomic<float> _ambientIntensity;
        std::atomic<bool> _terrainLightingEnabled;
        std::atomic<float> _shadowStrength;
        std::atomic<int> _shadowMapSize;
        std::atomic<int> _shadowCascades;
        std::atomic<float> _shadowBias;
        std::atomic<float> _shadowSoftness;
        std::atomic<float> _shadowDistance;
        std::atomic<int> _shadowCasterMargin;

        std::vector<std::shared_ptr<OnChangeListener> > _onChangeListeners;
        mutable std::mutex _onChangeListenersMutex;
    };

}

#endif
