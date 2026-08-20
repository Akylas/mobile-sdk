/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_SCREENMASKBUFFER_H_
#define _MASSIF_SCREENMASKBUFFER_H_

namespace massif {

    /**
     * A one-channel screen-space mask, at a fraction of the screen resolution, cleared to white.
     *
     * Two users, for the same reason: a value that many later fragments need is resolved once per
     * screen pixel instead of per draw. The terrain shadow, where the lookup is the most expensive
     * thing a shadowed fragment does and the terrain covers the whole screen - twice over where a
     * paint is drawn on the drape. And the extrusions' contact shadows, where the overlapping
     * capsules have to be reduced by MIN before anything multiplies them into the ground.
     *
     * The reduced resolution costs nothing visually in either case - both masks are penumbras -
     * and the LINEAR filter is what keeps its own texels from showing.
     *
     * GL thread only.
     */
    class ScreenMaskBuffer {
    public:
        /** useDepth: attach a depth buffer, for a pass whose draws overlap in depth. */
        explicit ScreenMaskBuffer(bool useDepth = true);
        ~ScreenMaskBuffer();

        int getWidth() const;
        int getHeight() const;

        /**
         * Sets the screen size and the divisor the mask is rendered at. Existing resources are
         * dropped when the resulting size changes.
         */
        void setSize(int screenWidth, int screenHeight, int divisor);

        /**
         * Returns the mask texture, creating the resources on first use. Returns 0 when the
         * framebuffer could not be completed.
         */
        unsigned int getTexture();
        /**
         * Binds the framebuffer and clears it to "fully lit". Returns false if unavailable.
         */
        bool beginPass();
        /**
         * Restores the previous framebuffer and viewport.
         */
        void endPass(unsigned int previousFrameBuffer, int viewportWidth, int viewportHeight);

        /**
         * Same, for a caller that owns the render state itself: binds, attaches, sets the viewport
         * and clears to white, and touches nothing else. beginPass above also sets blend, cull and
         * depth, which is wrong inside a pass that has already established its own (the drape bake
         * needs culling OFF, and getting that back enabled empties every tile it bakes afterwards).
         */
        bool beginPassRaw();
        void endPassRaw(unsigned int previousFrameBuffer, int viewportWidth, int viewportHeight);

        /**
         * Deletes all GL resources. Must be called on the GL thread while the context is alive.
         */
        void deleteResources();

    private:
        bool createResources();

        const bool _useDepth;
        int _width;
        int _height;
        unsigned int _frameBuffer;
        unsigned int _texture;
        unsigned int _depthBuffer;
        bool _failed;
    };

}

#endif
