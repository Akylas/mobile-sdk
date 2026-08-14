/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINSHADOWMASKBUFFER_H_
#define _CARTO_TERRAINSHADOWMASKBUFFER_H_

namespace carto {

    /**
     * Screen-space shadow mask for the terrain surface, at a fraction of the screen resolution.
     *
     * The shadow lookup is the most expensive thing a shadowed fragment does, and the terrain
     * covers the whole screen - twice over where a paint is drawn on the drape. Resolving it once
     * per screen pixel, at half resolution, turns every one of those draws into a single texture
     * fetch. Half resolution costs nothing visually: a shadow edge is a penumbra anyway.
     *
     * GL thread only.
     */
    class TerrainShadowMaskBuffer {
    public:
        TerrainShadowMaskBuffer();
        ~TerrainShadowMaskBuffer();

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
         * Deletes all GL resources. Must be called on the GL thread while the context is alive.
         */
        void deleteResources();

    private:
        bool createResources();

        int _width;
        int _height;
        unsigned int _frameBuffer;
        unsigned int _texture;
        unsigned int _depthBuffer;
        bool _failed;
    };

}

#endif
