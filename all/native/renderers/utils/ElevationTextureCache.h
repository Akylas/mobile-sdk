/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ELEVATIONTEXTURECACHE_H_
#define _CARTO_ELEVATIONTEXTURECACHE_H_

#include <array>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include "core/MapTile.h"

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
     *
     * A texture is PREPARED before it is used, never built inside the frame that first
     * samples it (tangram gets this for free: its elevation raster is the tile's own texture,
     * created when the tile loads). Encoding the padded texture runs on a worker thread, and
     * the upload runs on the GL thread under a per-frame budget - measured on a Crosscall, an
     * encode+upload in the middle of the frame that samples it cost 45 ms + 52 ms for one
     * 514x514 texture, which is most of a frame per tile.
     *
     * Must be used from the GL thread only (except the worker, which touches nothing else).
     * Internal class, not exposed in the public API.
     */
    class ElevationTextureCache {
    public:
        ElevationTextureCache(const std::shared_ptr<ElevationManager>& elevationManager, const std::shared_ptr<GLResourceManager>& glResourceManager);
        ~ElevationTextureCache();

        const std::shared_ptr<ElevationManager>& getElevationManager() const { return _elevationManager; }

        /**
         * Fills the terrain texture info for the given tile using the best cached
         * elevation grid (the grid may cover an ancestor tile). Returns false when no texture
         * is ready yet - the encode is queued and the tile renders flat (or from an ancestor
         * grid) until it is, exactly as it does while the elevation data itself is loading.
         */
        bool getTexture(const vt::TileId& tileId, vt::GLTileRenderer::TerrainTexture& terrainTexture);

        /**
         * Starts a new frame: uploads what the worker has encoded (up to the frame's budget) and
         * drops the per-frame tile resolution memo. The provider is called once per tile per
         * render pass, so without the memo every pass would redo the grid and neighbour lookups
         * (9 locked cache lookups per tile) for the same result.
         */
        void beginFrame();

        /**
         * Resolves every tile at the elevation source's own maximum detail instead of at the level
         * the terrain mesh can express. For a cache feeding per-fragment shading (the terrain
         * paint): the mesh cap costs two zoom levels of relief, which at high zoom is all of it.
         */
        void setFullDetail(bool enabled);

        void clear();

    private:
        struct CacheEntry {
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours; // border sources; entry rebuilds when a neighbour grid loads
            std::shared_ptr<Texture> texture;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
            std::uint64_t lastUsed = 0; // LRU stamp
        };

        // What the worker is asked for, and what it hands back.
        struct EncodeJob {
            long long gridTileId = -1;
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours;
        };
        struct EncodedTexture {
            long long gridTileId = -1;
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours;
            std::vector<std::uint8_t> rgbaData;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
            int width = 0;
            int height = 0;
        };

        static constexpr std::size_t MAX_CACHED_TEXTURES = 96; // ~24MB of RGBA 256x256 textures
        // Uploads per frame, and the time they may take. A tile with no texture yet renders FLAT,
        // so a budget that is too tight is visible as terrain that stays flat while it catches up;
        // one that is too loose brings back the stall this pipeline exists to remove. Time-bounded
        // with a floor of one upload, so progress is guaranteed however slow the device is.
        static constexpr int MAX_UPLOADS_PER_FRAME = 8;
        static constexpr double MAX_UPLOAD_MS_PER_FRAME = 6.0;
        static constexpr std::size_t MAX_ENCODE_QUEUE = 32;

        bool resolveEntry(const vt::TileId& tileId, MapTile& gridTileOut);
        static void fillTexture(const CacheEntry& entry, float metersToInternal, vt::GLTileRenderer::TerrainTexture& terrainTexture);
        // Queues an encode unless the same grid+neighbours is already queued, encoding or ready.
        void requestEncode(long long gridTileId, const std::shared_ptr<ElevationTileGrid>& grid, const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours);
        void uploadReadyTextures();
        void runEncodeWorker();
        void stopEncodeWorker();
        void evictLeastRecentlyUsed();

        const std::shared_ptr<ElevationManager> _elevationManager;
        const std::shared_ptr<GLResourceManager> _glResourceManager;
        std::map<long long, CacheEntry> _cache; // keyed by the grid tile id
        std::map<long long, MapTile> _frameResolved; // render tile id -> its elevation grid tile (zoom -1: no data), reset every frame
        bool _fullDetail = false; // resolve at the source's own max zoom, not at the mesh's level
        std::uint64_t _accessCounter = 0; // monotonic LRU clock
        std::uint64_t _frameStartCounter = 0; // LRU clock at the start of the current frame

        // Encode pipeline. The worker only ever touches the queues and the grids handed to it.
        mutable std::mutex _encodeMutex;
        std::condition_variable _encodeCondition;
        std::deque<EncodeJob> _encodeQueue;      // drained newest first: the newest request is the visible one
        std::set<long long> _encodePending;      // queued or being encoded
        std::deque<EncodedTexture> _encodedQueue; // waiting for the GL thread to upload
        std::unique_ptr<std::thread> _encodeThread;
        bool _encodeStopped = false;
    };
}

#endif
