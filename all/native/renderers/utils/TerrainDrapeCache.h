/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINDRAPECACHE_H_
#define _CARTO_TERRAINDRAPECACHE_H_

#include <cstddef>
#include <map>
#include <vector>

#include <vt/TileId.h>

namespace carto {

    /**
     * Shared render-to-texture drape target for 3D terrain.
     *
     * Owns one offscreen framebuffer and a per-terrain-tile colour texture, ABOVE the tile
     * layers. That is the point: every drapeable tile layer bakes into the same texture for a
     * given tile, in layer order, so a hillshade layer and a vector tile layer share one drape,
     * one terrain surface draw and one depth domain instead of each keeping their own.
     *
     * Textures are keyed by (tile, stack). A stack is a run of contiguous drapeable layers; a
     * non-drapeable layer between drapeable ones starts a new stack, which needs its own texture
     * and its own surface draw over the previous one.
     *
     * GL thread only.
     */
    class TerrainDrapeCache {
    public:
        TerrainDrapeCache();
        ~TerrainDrapeCache();

        int getResolution() const;
        /**
         * Sets the per-tile texture resolution. Existing textures are dropped, since they are
         * the old size.
         */
        void setResolution(int resolution);

        /**
         * Starts a frame. Tiles not acquired before endFrame() are released back to the pool.
         */
        void beginFrame();
        /**
         * Returns the texture for a tile, creating or recycling one if needed. needsBake is set
         * when the texture has no content matching the given fingerprint, i.e. the caller must
         * clear it and have every participating layer bake into it.
         */
        unsigned int acquire(const vt::TileId& tileId, int stack, std::size_t fingerprint, bool& needsBake);
        /**
         * Returns the framebuffer to bake into, creating it on first use.
         */
        unsigned int getFrameBuffer();
        /**
         * Releases textures not acquired during this frame.
         */
        void endFrame();

        /**
         * Deletes all GL resources. Must be called on the GL thread while the context is alive.
         */
        void deleteResources();

    private:
        struct Key {
            vt::TileId tileId;
            int stack;

            bool operator < (const Key& other) const;
        };

        struct Entry {
            unsigned int texture = 0;
            std::size_t fingerprint = 0;
            bool baked = false;
            bool used = false;
        };

        unsigned int createTexture();

        static const std::size_t MAX_POOLED_TEXTURES; // recycled textures kept between frames

        int _resolution;
        unsigned int _frameBuffer;
        std::map<Key, Entry> _entries;
        std::vector<unsigned int> _texturePool;
    };

}

#endif
