/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ELEVATIONTEXTURECACHE_H_
#define _CARTO_ELEVATIONTEXTURECACHE_H_

#include <array>
#include <atomic>
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
#include "terrain/ElevationTileGrid.h" // BorderStrips is a member of a queued patch

#include <vt/GLTileRenderer.h>

namespace carto {
    class Bitmap;
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
        void setDetailLevels(int extraLevels);

        void clear();

    private:
        class BorderBitmap; // a Bitmap whose border strips can be rewritten in place

        // Grids are identified by their TILE, not by the pointer they happen to live behind: the
        // elevation cache is an LRU, so the same DEM tile can be decoded into a new object at any
        // time. Comparing pointers made that re-decode look like new data and re-encoded (or, since
        // border patching, re-patched) a texture whose content had not changed at all.
        using GridKey = long long; // grid tile id, or -1 for a missing neighbour
        static GridKey gridKey(const std::shared_ptr<ElevationTileGrid>& grid);
        static std::array<GridKey, 8> gridKeys(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& grids);

        struct CacheEntry {
            std::shared_ptr<ElevationTileGrid> grid;
            GridKey gridKeyValue = -1;
            std::array<GridKey, 8> neighbourKeys = { { -1, -1, -1, -1, -1, -1, -1, -1 } };
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours; // border sources; the border is patched when one loads
            std::shared_ptr<BorderBitmap> bitmap; // what the texture is rebuilt from after a context loss
            std::shared_ptr<Texture> texture;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
            std::uint64_t lastUsed = 0; // LRU stamp
        };

        // What the worker is asked for, and what it hands back.
        struct EncodeJob {
            long long gridTileId = -1;
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours;
            bool bordersOnly = false; // the entry already has this grid's texture; only its ring changed
        };
        // The BITMAP, not the encoded bytes: building it copies the whole padded texture
        // (514x514 RGBA, a megabyte, byte by byte in Bitmap::loadFromUncompressedBytes) and that
        // copy has no reason to be on the render thread - measured on the Crosscall, north pan,
        // it was 20% of it, with another 11% freeing the encode buffer there.
        struct EncodedTexture {
            long long gridTileId = -1;
            GridKey gridKeyValue = -1;
            std::array<GridKey, 8> neighbourKeys = { { -1, -1, -1, -1, -1, -1, -1, -1 } };
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours;
            std::shared_ptr<BorderBitmap> bitmap;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
        };

        // A neighbour arriving changes ONLY the 2-texel ring of the texture (the border itself,
        // and this grid's outermost row/column where a coarser neighbour box-filters it). During a
        // pan that is the common case by far, and re-encoding a megabyte for it is most of what
        // this pipeline costs. The ring is encoded on the worker and patched into the existing
        // texture and its bitmap - same result, ~1.5% of the texels.
        struct BorderPatch {
            long long gridTileId = -1;
            GridKey gridKeyValue = -1;      // the patch is void if the entry's grid changed meanwhile
            std::array<GridKey, 8> neighbourKeys = { { -1, -1, -1, -1, -1, -1, -1, -1 } };
            std::shared_ptr<ElevationTileGrid> grid;
            std::array<std::shared_ptr<ElevationTileGrid>, 8> neighbours;
            ElevationTileGrid::BorderStrips strips;
            std::array<float, 4> decode = { { 0, 0, 0, 0 } };
        };

        // Two bytes a texel (LUMINANCE_ALPHA) instead of four, so twice as many textures fit in the
        // memory the old cap used - and the working set is what decides whether extra DEM detail is
        // affordable (each level beyond the mesh cap is 4x the textures).
        static constexpr std::size_t MAX_CACHED_TEXTURES = 192;
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
        // 'bordersOnly' when the entry already holds a texture built from this exact grid and only
        // the neighbours changed.
        void requestEncode(long long gridTileId, const std::shared_ptr<ElevationTileGrid>& grid, const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, bool bordersOnly);
        void uploadReadyTextures();
        void applyBorderPatches();
        void runEncodeWorker();
        void stopEncodeWorker();
        void evictLeastRecentlyUsed();

        const std::shared_ptr<ElevationManager> _elevationManager;
        const std::shared_ptr<GLResourceManager> _glResourceManager;
        std::map<long long, CacheEntry> _cache; // keyed by the grid tile id
        std::map<long long, MapTile> _frameResolved; // render tile id -> its elevation grid tile (zoom -1: no data), reset every frame
        int _detailLevels = 0; // elevation levels resolved BEYOND what the mesh can express
        std::uint64_t _accessCounter = 0; // monotonic LRU clock
        std::uint64_t _frameStartCounter = 0; // LRU clock at the start of the current frame

        // Encode pipeline. The worker only ever touches the queues and the grids handed to it.
        mutable std::mutex _encodeMutex;
        std::condition_variable _encodeCondition;
        std::deque<EncodeJob> _encodeQueue;      // drained newest first: the newest request is the visible one
        std::set<long long> _encodePending;      // queued or being encoded
        std::deque<EncodedTexture> _encodedQueue; // waiting for the GL thread to upload
        std::deque<BorderPatch> _patchQueue;      // waiting for the GL thread to patch
        std::vector<std::uint8_t> _encodeScratch; // worker-thread only: the encode buffer, reused
        std::unique_ptr<std::thread> _encodeThread;
        bool _encodeStopped = false;
    };
}

#endif
