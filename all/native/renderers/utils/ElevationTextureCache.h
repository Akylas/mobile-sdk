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

        /**
         * Starts a new frame: drops the per-frame tile resolution memo. The provider is called
         * once per tile per render pass, so without the memo every pass would redo the grid and
         * neighbour lookups (9 locked cache lookups per tile) for the same result.
         */
        void beginFrame();

        /**
         * Sets the terrain surface resolution (grid cells per tile edge). The elevation level is
         * capped so that one elevation texel covers at most half a surface cell: finer elevation
         * data cannot be expressed by the mesh, but it would multiply the number of distinct
         * elevation textures by four per level - and with it the decoded grid working set, the
         * texture uploads and the tile requests.
         */
        void setSurfaceResolution(int resolution);

        void clear();

    private:
        struct CacheEntry {
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours; // border sources; entry rebuilds when a neighbour grid loads
            std::shared_ptr<Texture> texture;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
            std::uint64_t lastUsed = 0; // LRU stamp
        };

        static constexpr std::size_t MAX_CACHED_TEXTURES = 96; // ~24MB of RGBA 256x256 textures

        bool resolveEntry(const vt::TileId& tileId, long long& gridTileId);
        static void fillTexture(const CacheEntry& entry, float metersToInternal, vt::GLTileRenderer::TerrainTexture& terrainTexture);

        const std::shared_ptr<ElevationManager> _elevationManager;
        const std::shared_ptr<GLResourceManager> _glResourceManager;
        std::map<long long, CacheEntry> _cache; // keyed by the grid tile id
        std::map<long long, long long> _frameResolved; // render tile id -> grid tile id (-1: no data), reset every frame
        int _surfaceResolution = 32;   // terrain mesh cells per tile edge
        int _gridSizeHint = 256;       // texels per elevation tile edge, from the last resolved grid
        std::uint64_t _accessCounter = 0; // monotonic LRU clock
        std::uint64_t _frameStartCounter = 0; // LRU clock at the start of the current frame
    };
}

#endif
