/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_SKYOPTIONS_H_
#define _CARTO_SKYOPTIONS_H_

#include "graphics/Color.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

    /**
     * Shader-based sky configuration, attached to the map via Options::setSkyOptions.
     *
     * The sky is drawn as a single full-screen pass before everything else, so it costs one
     * quad regardless of the camera. The built-in shader draws a two-colour gradient around
     * the horizon plus a sun disc and glow, using the sun direction from Options::getLightOptions.
     *
     * The whole appearance can be replaced with setShaderSource. The supplied GLSL must define
     *
     *     vec4 skyColor(vec3 rayDir);
     *
     * where rayDir is the normalised world-space view ray for the fragment (x east, y north,
     * z up), and the result is the non-premultiplied sky colour. These are available to it:
     *
     *     uniform vec3  u_sunDir;        // unit vector towards the sun, world space
     *     uniform vec4  u_sunColor;      // sun colour, rgba 0..1
     *     uniform vec4  u_skyColor;      // configured sky colour (zenith), rgba 0..1
     *     uniform vec4  u_horizonColor;  // configured horizon colour, rgba 0..1
     *     uniform float u_sunIntensity;  // LightOptions sun intensity
     *     uniform float u_time;          // seconds since the map view was created
     *     uniform float u_zoom;          // current fractional map zoom
     *     uniform float u_cameraHeight;  // camera height above the map plane, in metres
     *     uniform vec2  u_resolution;    // viewport size in pixels
     *
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class SkyOptions {
    public:
        /**
         * Interface for monitoring sky option change events. Internal.
         */
        struct OnChangeListener {
            virtual ~OnChangeListener() { }

            /**
             * Listener method that gets called when a sky option has changed.
             * @param optionName The name of the option that has changed.
             */
            virtual void onSkyOptionChanged(const std::string& optionName) = 0;
        };

        /**
         * Constructs a SkyOptions object with default values.
         */
        SkyOptions();
        virtual ~SkyOptions();

        /**
         * Returns whether the shader sky is enabled.
         * @return True if the shader sky is drawn. The default is true.
         */
        bool isEnabled() const;
        /**
         * Enables or disables the shader sky. When disabled, the legacy sky bitmap band
         * (Options::setSkyColor / the style sky bitmap) is drawn instead.
         * @param enabled True to draw the shader sky.
         */
        void setEnabled(bool enabled);

        /**
         * Returns the zenith sky color.
         * @return The sky color. The default is a light blue.
         */
        Color getSkyColor() const;
        /**
         * Sets the zenith sky color, used by the built-in shader at the top of the sky.
         * @param color The new sky color.
         */
        void setSkyColor(const Color& color);

        /**
         * Returns the horizon color.
         * @return The horizon color. The default is a pale blue-white.
         */
        Color getHorizonColor() const;
        /**
         * Sets the horizon color, used by the built-in shader at the horizon line.
         * @param color The new horizon color.
         */
        void setHorizonColor(const Color& color);

        /**
         * Returns the ground color.
         * @return The color drawn below the horizon. The default is the horizon color.
         */
        Color getGroundColor() const;
        /**
         * Sets the color drawn below the horizon. The map normally covers that part of the
         * screen, so this only shows in the wedge between the far edge of the drawn map and
         * the mathematical horizon - it should stay close to the horizon color, which is the
         * default. Setting it transparent leaves the clear color there.
         * @param color The new ground color.
         */
        void setGroundColor(const Color& color);

        /**
         * Returns the angular blend width between the horizon color and the sky color.
         * @return The blend width in degrees. The default is 12.
         */
        float getHorizonBlend() const;
        /**
         * Sets how far above the horizon, in degrees, the horizon color fades into the sky color.
         * @param degrees The new blend width in degrees (clamped to 0..90).
         */
        void setHorizonBlend(float degrees);

        /**
         * Returns how far up the sky the terrain fog is blended in.
         * @return The fog blend height in degrees above the horizon. The default is 12.
         */
        float getFogBlend() const;
        /**
         * Sets how far above the horizon, in degrees, the terrain fog colour fades out of the sky.
         * The fog (TerrainOptions/style fog colour, lit by the sun) is strongest right at the
         * horizon and gone by this angle, so the haze the ground fades into continues into the sky
         * instead of ending in a band at the skyline. Zero leaves the sky alone.
         * @param degrees The new fog blend height in degrees (clamped to 0..90).
         */
        void setFogBlend(float degrees);

        /**
         * Returns whether the built-in shader draws a sun disc.
         * @return True if the sun disc is drawn. The default is true.
         */
        bool isSunDiscEnabled() const;
        /**
         * Enables or disables the sun disc and its glow in the built-in shader.
         * @param enabled True to draw the sun disc.
         */
        void setSunDiscEnabled(bool enabled);

        /**
         * Returns the custom sky fragment shader source, or an empty string if the built-in
         * shader is used.
         * @return The custom shader source.
         */
        std::string getShaderSource() const;
        /**
         * Sets a custom sky fragment shader. The source must define
         * "vec4 skyColor(vec3 rayDir)" and may use the uniforms documented on this class.
         * Pass an empty string to go back to the built-in shader. If the shader fails to
         * compile, the built-in shader is used and the error is logged.
         * @param shaderSource The GLSL source, or an empty string for the built-in shader.
         */
        void setShaderSource(const std::string& shaderSource);

        /**
         * Registers listener for sky option change events. Internal method.
         * @param listener The listener for change events.
         */
        void registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);
        /**
         * Unregisters listener from sky option change events. Internal method.
         * @param listener The previously added listener.
         */
        void unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);

    private:
        void notifyOptionChanged(const std::string& optionName);

        std::atomic<bool> _enabled;
        std::atomic<int> _skyColorARGB;
        std::atomic<int> _horizonColorARGB;
        std::atomic<int> _groundColorARGB;
        std::atomic<float> _horizonBlend;
        std::atomic<float> _fogBlend;
        std::atomic<bool> _sunDiscEnabled;

        std::string _shaderSource;
        mutable std::mutex _shaderSourceMutex;

        std::vector<std::shared_ptr<OnChangeListener> > _onChangeListeners;
        mutable std::mutex _onChangeListenersMutex;
    };

}

#endif
