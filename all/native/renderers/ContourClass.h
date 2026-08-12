/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CONTOURCLASS_H_
#define _CARTO_CONTOURCLASS_H_

#include "graphics/Color.h"

namespace carto {

    /**
     * One elevation class of the contour lines drawn per fragment on the terrain: the lines whose
     * height is a multiple of 'interval', in this colour and width. A style layer resolves to a
     * list of them (mvt::resolveContourStyle), a HillshadeRasterTileLayer configures a single one.
     * Internal class, not exposed in the public API.
     */
    struct ContourClass {
        float interval = 0.0f;   // metres between the lines of the class
        Color color;             // straight colour, opacity in the alpha channel
        float halfWidth = 0.0f;  // half stroke width, screen pixels

        bool operator == (const ContourClass& other) const {
            return interval == other.interval && halfWidth == other.halfWidth && color == other.color;
        }
        bool operator != (const ContourClass& other) const { return !(*this == other); }
    };

}

#endif
