/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_LIGHTOPTIONS_H_
#define _CARTO_LIGHTOPTIONS_H_

#include "graphics/Color.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <cglib/vec.h>

namespace carto {

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

        std::vector<std::shared_ptr<OnChangeListener> > _onChangeListeners;
        mutable std::mutex _onChangeListenersMutex;
    };

}

#endif
