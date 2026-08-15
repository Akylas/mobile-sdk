/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_REDRAWREQUESTLISTENER_H_
#define _MASSIF_REDRAWREQUESTLISTENER_H_

namespace massif {

    /**
     * An internal listener class for notifying about screen redraw requests.
     */
    class RedrawRequestListener {
    public:
        virtual ~RedrawRequestListener() { }
    
        /**
         * Called when the screen needs to be redrawn.
         */
        virtual void onRedrawRequested() const { }
    };
    
}

#endif
