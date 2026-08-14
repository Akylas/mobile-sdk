/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINSHADOWMAP_H_
#define _CARTO_TERRAINSHADOWMAP_H_

namespace carto {

    /**
     * Offscreen target for the directional shadow caster pass.
     *
     * Holds one colour texture with the light-space depth packed into RGB (plus a depth
     * renderbuffer for the pass itself), so no depth-texture extension is required. The caster
     * geometry is the terrain surface itself, drawn with the same vertex shader and the same
     * elevation fetch as the on-screen draw - the shadow geometry is therefore bit-identical to
     * the rendered geometry, which is what keeps self-shadowing free of acne from a mismatched
     * proxy mesh.
     *
     * GL thread only.
     */
    class TerrainShadowMap {
    public:
        // Must match the cascade count the vt shaders declare (vt::GLTileRenderer::MAX_SHADOW_CASCADES).
        static constexpr int MAX_CASCADES = 4;

        TerrainShadowMap();
        ~TerrainShadowMap();

        int getSize() const;
        int getCascades() const;
        /**
         * Sets the shadow map resolution and the number of cascades. The cascades are pages of
         * one texture, laid out side by side with the nearest first, so a fragment shader needs
         * one sampler and one scale to reach any of them. Existing resources are dropped on a
         * change.
         */
        void setSize(int size, int cascades);

        /**
         * Returns the packed-depth texture, creating the resources on first use. Returns 0 when
         * the framebuffer could not be completed.
         */
        unsigned int getTexture();
        /**
         * Binds the framebuffer, clearing every page to "infinitely far" when clearAll is set.
         * Returns false if unavailable.
         */
        bool beginPass(bool clearAll);
        /**
         * Restricts drawing (and clearing) to one cascade's page. Must be called after beginPass.
         */
        void setCascadeViewport(int cascade);
        /**
         * Clears the current cascade's page alone, for a pass that refreshes only some of them.
         */
        void clearCascade();
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
        bool createResourcesAtSize();

        int _size;
        int _cascades;
        unsigned int _frameBuffer;
        unsigned int _texture;
        unsigned int _depthBuffer;
        bool _failed;
    };

}

#endif
