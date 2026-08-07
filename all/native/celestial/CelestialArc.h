/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CELESTIALARC_H_
#define _CARTO_CELESTIALARC_H_

#include "celestial/CelestialObject.h"

#include <vector>

namespace carto {

    /**
     * A curve drawn on the sky.
     *
     * Two ways to define one, and the first is what a daily path is:
     *  - as a CIRCLE about an axis: every direction at a fixed angle from the axis. The sun's path
     *    across a day is exactly this - the axis is the celestial pole and the angle is 90 degrees
     *    minus the declination - so an application draws it with an axis and one angle rather than
     *    by sampling positions through the day.
     *  - as an explicit list of directions, for a path that is not a circle: a satellite track, a
     *    flight plan, an analemma.
     *
     * The curve is generated once and drawn as a line strip; its width is in pixels, so it stays
     * legible at any field of view.
     */
    class CelestialArc : public CelestialObject {
    public:
        CelestialArc();
        virtual ~CelestialArc();

        /**
         * Defines the arc as a circle about an axis.
         * @param axisAzimuth The azimuth of the axis in degrees, clockwise from north.
         * @param axisAltitude The altitude of the axis in degrees above the horizon.
         * @param radius The angular radius in degrees: the angle between the axis and the curve.
         */
        void setCircle(float axisAzimuth, float axisAltitude, float radius);

        /**
         * Defines the arc as an explicit list of directions, each an (azimuth, altitude) pair in
         * degrees. The curve runs through them in order and is not closed.
         * @param directions The directions, as alternating azimuth and altitude values in degrees.
         */
        void setDirections(const std::vector<double>& directions);

        /**
         * Returns the directions of an explicitly defined arc.
         * @return The directions, as alternating azimuth and altitude values in degrees.
         */
        std::vector<double> getDirections() const;

        /**
         * Returns the angular radius of a circular arc.
         * @return The angular radius in degrees.
         */
        float getRadius() const;

        /**
         * Returns the line width.
         * @return The width in pixels.
         */
        float getWidth() const;
        /**
         * Sets the line width.
         * @param pixels The width in pixels.
         */
        void setWidth(float pixels);

        /**
         * Returns whether the part of the curve below the horizon is drawn.
         * @return True if the curve is drawn below the horizon.
         */
        bool isBelowHorizonVisible() const;
        /**
         * Sets whether the part of the curve below the horizon is drawn. A sun path looks right
         * with this off - the arc then rises and sets like the sun does.
         * @param visible True to draw the curve below the horizon.
         */
        void setBelowHorizonVisible(bool visible);

        /**
         * Builds the curve as unit direction vectors, in the map's local frame (x east, y north,
         * z up). Called by the renderer; an application does not need it.
         * @return The direction vectors along the curve.
         */
        std::vector<cglib::vec3<double> > buildDirections() const;

    private:
        static const int CIRCLE_SEGMENTS;

        bool _circular;
        float _axisAzimuth;
        float _axisAltitude;
        float _radius;
        std::vector<double> _directions;
        float _width;
        bool _belowHorizonVisible;
    };

}

#endif
