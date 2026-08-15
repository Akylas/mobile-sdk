/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_STYLE_H_
#define _MASSIF_STYLE_H_

#include "graphics/Color.h"

namespace massif {

    /**
     * A base class for other Style objects.
     */
    class Style {
    public:
        virtual ~Style();
    
        /**
         * Returns the color of the vector element.
         * @return The color of the vector element.
         */
        const Color& getColor() const;
    
    protected:
        explicit Style(const Color& color);
    
        Color _color;
    };

}

#endif
