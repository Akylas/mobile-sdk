/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ELEVATIONTEXTURECACHE_H_
#define _CARTO_ELEVATIONTEXTURECACHE_H_

#include <array>
#include <cstdint>
#include <map>
#include <memory>

#include <vt/GLTileRenderer.h>

namespace carto {
    class ElevationManager;
    class ElevationTileGrid;
    class GLResourceManager;
    class Texture;

    /**
     * GL elevation texture cache for GPU terrain draping: implements the
     * vt::GLTileRenderer terrain texture provider on top of the ElevationManager
     * grid cache. Textures are keyed by the grid's own tile, so overzoomed tiles
     * and all tile layers share one texture per DEM tile, and neighbouring tiles
     * sampling the same DEM level sample one continuous texture.
     * Must be used from the GL thread only. Internal class, not exposed in the public API.
     */
    class ElevationTextureCache {
    public:
        ElevationTextureCache(const std::shared_ptr<ElevationManager>& elevationManager, const std::shared_ptr<GLResourceManager>& glResourceManager);

        const std::shared_ptr<ElevationManager>& getElevationManager() const { return _elevationManager; }

        /**
         * Fills the terrain texture info for the given tile using the best cached
         * elevation grid (the grid may cover an ancestor tile). Creates and caches
         * the GL texture on first use; entries refresh automatically when the
         * underlying elevation grid changes. Returns false if no grid is available.
         */
        bool getTexture(const vt::TileId& tileId, vt::GLTileRenderer::TerrainTexture& terrainTexture);

        void clear();

    private:
        struct CacheEntry {
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours; // border sources; entry rebuilds when a neighbour grid loads
            std::shared_ptr<Texture> texture;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
        };

        static constexpr std::size_t MAX_CACHED_TEXTURES = 96; // ~24MB of RGBA 256x256 textures

        const std::shared_ptr<ElevationManager> _elevationManager;
        const std::shared_ptr<GLResourceManager> _glResourceManager;
        std::map<long long, CacheEntry> _cache; // keyed by the grid tile id
    };
}

#endif
