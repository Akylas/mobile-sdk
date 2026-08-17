/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_FOGOPTIONS_H_
#define _MASSIF_FOGOPTIONS_H_

#include "graphics/Color.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace massif {

    /**
     * The atmosphere: the haze distant ground fades into, the colours it carries up into the sky,
     * and the stars beyond it. Attached to the map via Options::setFogOptions.
     *
     * Modelled on the Mapbox "fog" style property, so a value tuned for a Mapbox style transfers
     * directly: Range is RangeStart/RangeEnd, Color is color, HighColor is high-color, SpaceColor
     * is space-color, HorizonBlend is horizon-blend and StarIntensity is star-intensity.
     *
     * Enabled is the switch: turning it off keeps every value configured, so a UI toggle does not
     * have to drive a colour or a range through zero and back. With it on, the fog still needs a
     * Color with a non-zero alpha over a positive range - the default colour is transparent, so
     * attaching a FogOptions changes nothing until a colour is set.
     *
     * Ranges are in multiples of the camera-to-focus distance, not in meters: that distance is a
     * function of the zoom alone, so one setting holds at every zoom instead of needing a
     * per-zoom expression. At the default 0.8 to 8, the fog starts just in front of the focus
     * point and saturates well past the horizon.
     *
     * The whole blend can be replaced with setShaderSource, which reaches the tile content, the
     * background plane and the sky alike - see that method.
     *
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class FogOptions {
    public:
        /**
         * Interface for monitoring fog option change events. Internal.
         */
        struct OnChangeListener {
            virtual ~OnChangeListener() { }

            /**
             * Listener method that gets called when a fog option has changed.
             * @param optionName The name of the option that has changed.
             */
            virtual void onFogOptionChanged(const std::string& optionName) = 0;
        };

        /**
         * Constructs a FogOptions object with default values.
         */
        FogOptions();
        virtual ~FogOptions();

        /**
         * Returns whether the fog is drawn at all.
         * @return True if the fog is drawn. The default is true.
         */
        bool isEnabled() const;
        /**
         * Enables or disables the fog without touching any of its values, so a toggle does not
         * have to drive the color or the range through zero. Off means no fog anywhere - the tile
         * content, the background plane and the sky all stop fogging together.
         * Unlike every other property, this one is ANDed with the style rather than overridden by
         * it: a style cannot re-enable a fog the application switched off.
         * Style property: "fog-enabled" (0 or 1).
         * @param enabled True to draw the fog.
         */
        void setEnabled(bool enabled);

        /**
         * Returns the fog color.
         * @return The fog color. The default is transparent (no fog).
         */
        Color getColor() const;
        /**
         * Sets the color distant terrain, rasters, geometry and 3D extrusions fade towards.
         * The alpha channel is how opaque the fog gets at full distance, so a fully transparent
         * color (the default) means no fog at all. Fog is what makes a long view distance look
         * like distance rather than like a hard cut, and it is what hides the edge of the terrain
         * when the maximum visible distance is limited.
         * With terrain lighting on, the color is lit by the sun before it is used - haze is air,
         * so it darkens at night and warms at a low sun.
         * Style property: "fog-color".
         * @param color The new fog color.
         */
        void setColor(const Color& color);

        /**
         * Returns where the fog starts.
         * @return The start of the range, in multiples of the camera-to-focus distance. The default is 0.8.
         */
        float getRangeStart() const;
        /**
         * Sets where the fog starts, in multiples of the camera-to-focus distance. Nothing nearer
         * than this is fogged at all. Mapbox range[0]. Style property: "fog-range-start".
         * @param rangeStart The new start of the range (clamped to 0 and above).
         */
        void setRangeStart(float rangeStart);

        /**
         * Returns where the fog reaches full strength.
         * @return The end of the range, in multiples of the camera-to-focus distance. The default is 8.
         */
        float getRangeEnd() const;
        /**
         * Sets where the fog reaches its full strength, in multiples of the camera-to-focus
         * distance. A value at or below RangeStart turns the fog off. Mapbox range[1].
         * Style property: "fog-range-end".
         * @param rangeEnd The new end of the range (clamped to 0 and above).
         */
        void setRangeEnd(float rangeEnd);

        /**
         * Returns the color of the upper atmosphere.
         * @return The high color. The default is transparent, which leaves the sky to SkyOptions.
         */
        Color getHighColor() const;
        /**
         * Sets the color the sky takes above the fog band - Mapbox high-color, the lit upper
         * atmosphere. Transparent (the default) leaves the sky gradient to SkyOptions alone.
         * Style property: "fog-high-color".
         * @param color The new high color.
         */
        void setHighColor(const Color& color);

        /**
         * Returns the color of the sky at the zenith, beyond the atmosphere.
         * @return The space color. The default is transparent, which leaves the sky to SkyOptions.
         */
        Color getSpaceColor() const;
        /**
         * Sets the color the sky reaches straight up, beyond the atmosphere - Mapbox space-color.
         * Transparent (the default) leaves the sky gradient to SkyOptions alone.
         * Style property: "fog-space-color".
         * @param color The new space color.
         */
        void setSpaceColor(const Color& color);

        /**
         * Returns how far up the sky the fog is blended in.
         * @return The blend, 0 to 1 of a quarter turn. The default is 0.133, the previous 12 degrees.
         */
        float getHorizonBlend() const;
        /**
         * Sets how far above the horizon the fog color fades out of the sky, as a fraction of a
         * quarter turn (1 = the whole sky, Mapbox horizon-blend). The fog is strongest at the
         * horizon and gone by this angle, so the haze the ground fades into carries on into the
         * sky instead of ending in a band at the skyline. Zero leaves the sky alone.
         * Style property: "fog-horizon-blend".
         * @param horizonBlend The new blend (clamped to 0..1).
         */
        void setHorizonBlend(float horizonBlend);

        /**
         * Returns the elevation angle the fog is still at full strength at.
         * @return The angle in degrees, or a negative value when it follows the terrain. The default is -1.
         */
        float getHorizonAngle() const;
        /**
         * Sets the elevation angle, in degrees, that the fog is still at FULL strength at, with
         * HorizonBlend measured from there. Zero is the mathematical horizon, which is right on a
         * flat map, where the skyline IS the horizon. In the mountains the skyline is a ridge and a
         * ridge stands well above the horizon once the camera is near it, so the haze is spent by
         * the time the ray clears the silhouette and hazy ground meets clean sky along it.
         * A negative value (the default) takes the angle from the terrain instead - the highest
         * ground the view can hold, seen at the distance the fog saturates at - capped at half the
         * blend so the full-strength band can never swamp the sky. Set 0 to fade from the horizon,
         * or a positive angle to pin it. This one has no Mapbox equivalent.
         * @param degrees The new angle in degrees (clamped to 90), or negative to follow the terrain.
         */
        void setHorizonAngle(float degrees);

        /**
         * Returns how brightly stars are drawn beyond the atmosphere.
         * @return The star intensity, 0 to 1. The default is 0 (no stars).
         */
        float getStarIntensity() const;
        /**
         * Sets how brightly stars are drawn in the part of the sky the atmosphere has faded out of
         * - Mapbox star-intensity. 0 (the default) draws none. They are drawn by the built-in sky
         * shader only, so a custom sky shader has to draw its own.
         * Style property: "fog-star-intensity".
         * @param starIntensity The new star intensity (clamped to 0..1).
         */
        void setStarIntensity(float starIntensity);

        /**
         * Returns the custom fog fragment shader source, or an empty string if the built-in
         * blend is used.
         * @return The custom shader source.
         */
        std::string getShaderSource() const;
        /**
         * Sets a custom fog fragment shader, used by the tile content, the background plane and
         * the sky alike, in 2D and in 3D. The source must define
         *
         *     vec4 applyFog(vec4 color, float amount, float dist);
         *
         * where color is the fragment's PREMULTIPLIED color, amount is the built-in fog factor for
         * it (0 in front of the range, 1 past it, already scaled by the fog color's alpha) and dist
         * is the distance from the camera in multiples of the camera-to-focus distance - the unit
         * the range is in. The sky calls it too, with its angular haze as amount and the range end
         * as dist. These are available to it:
         *
         *     uniform vec4  uFogColor;      // the resolved and lit fog color, rgba 0..1
         *     uniform vec4  uFogHighColor;  // the upper atmosphere color, rgba 0..1
         *     uniform vec4  uFogSpaceColor; // the zenith color, rgba 0..1
         *     uniform vec4  uFogParams;     // range start, 1 / (end - start), internal -> range units, range end
         *
         * Pass an empty string to go back to the built-in blend. If the shader fails to compile,
         * the built-in blend is used and the error is logged.
         * @param shaderSource The GLSL source, or an empty string for the built-in blend.
         */
        void setShaderSource(const std::string& shaderSource);

        /**
         * Registers listener for fog option change events. Internal method.
         * @param listener The listener for change events.
         */
        void registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);
        /**
         * Unregisters listener from fog option change events. Internal method.
         * @param listener The previously added listener.
         */
        void unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);

    private:
        void notifyOptionChanged(const std::string& optionName);

        std::atomic<bool> _enabled;
        std::atomic<int> _colorARGB;
        std::atomic<float> _rangeStart;
        std::atomic<float> _rangeEnd;
        std::atomic<int> _highColorARGB;
        std::atomic<int> _spaceColorARGB;
        std::atomic<float> _horizonBlend;
        std::atomic<float> _horizonAngle;
        std::atomic<float> _starIntensity;

        std::string _shaderSource;
        mutable std::mutex _shaderSourceMutex;

        std::vector<std::shared_ptr<OnChangeListener> > _onChangeListeners;
        mutable std::mutex _onChangeListenersMutex;
    };

}

#endif
