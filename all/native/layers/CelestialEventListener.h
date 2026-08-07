/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CELESTIALEVENTLISTENER_H_
#define _CARTO_CELESTIALEVENTLISTENER_H_

#include "ui/ClickInfo.h"

#include <memory>

namespace carto {
    class CelestialObject;

    /**
     * Reports clicks on the objects of a CelestialLayer.
     */
    class CelestialEventListener {
    public:
        virtual ~CelestialEventListener() { }

        /**
         * Called when an object of the layer is clicked. Objects are tested against the touch ray
         * together with every other layer's content, so an object behind terrain is not reported.
         * @param clickInfo The click that hit the object.
         * @param celestialObject The object that was clicked.
         * @return True if the click was handled and must not be passed on, false otherwise.
         */
        virtual bool onCelestialObjectClicked(const ClickInfo& clickInfo, const std::shared_ptr<CelestialObject>& celestialObject) {
            return false;
        }
    };

}

#endif
