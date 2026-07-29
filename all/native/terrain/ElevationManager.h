/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ELEVATIONMANAGER_H_
#define _CARTO_ELEVATIONMANAGER_H_

#include "components/ElevationProvider.h"
#include "core/MapPos.h"
#include "core/MapTile.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

#include <stdext/timed_lru_cache.h>

namespace carto {
    class TileDataSource;
    class ElevationDecoder;
    class ElevationTileGrid;
    class Projection;

    /**
     * Manages decoded DEM elevation grids on top of a raster elevation tile data source
     * (the same data source that can be simultaneously used by HillshadeRasterTileLayer).
     * Provides thread-safe elevation lookups in meters and in display units
     * (internal z units, including exaggeration and Mercator latitude scale),
     * ray intersection against the displaced terrain surface, and per-tile elevation bounds.
     * Internal class, not exposed in the public API.
     */
    class ElevationManager : public ElevationProvider {
    public:
        enum class LoadMode {
            /**
             * Only already decoded grids (or their cached ancestors) may be used. Never blocks.
             */
            CACHED_ONLY,
            /**
             * The elevation tile may be synchronously loaded from the data source. May block on IO/network.
             */
            ALLOW_LOAD,
            /**
             * Like ALLOW_LOAD, but a cached ancestor is not accepted as a stand-in: the tile itself
             * is loaded unless the data source has already answered that this level does not exist.
             * May block on IO/network.
             */
            LOAD_EXACT
        };

        ElevationManager(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder);
        virtual ~ElevationManager();

        std::shared_ptr<TileDataSource> getDataSource() const;
        std::shared_ptr<ElevationDecoder> getElevationDecoder() const;

        float getExaggeration() const;
        void setExaggeration(float exaggeration);

        bool isSeamlessTileEdgesEnabled() const;
        void setSeamlessTileEdgesEnabled(bool enabled);

        bool isNeighbourPrefetchEnabled() const;
        void setNeighbourPrefetchEnabled(bool enabled);

        /**
         * Sets the terrain surface resolution (mesh cells per tile edge). Elevation levels are
         * capped so that one elevation texel covers at most half a surface cell: finer data cannot
         * be expressed by the mesh, but every level costs four times the tiles, decoded grids and
         * GL textures. The cap applies to EVERY elevation query, so the displaced surface, the
         * elevation lookups, the ray intersection used for billboard occlusion and the CPU-side
         * element placement all agree on one height field.
         */
        void setSurfaceResolution(int resolution);

        std::size_t getCacheCapacity() const;
        void setCacheCapacity(std::size_t capacityInBytes);

        /**
         * Returns the elevation in meters at the given WGS84 position, loading the elevation tile if needed.
         * Matches HillshadeRasterTileLayer::getElevation semantics (returns -1000000 if no data is available).
         */
        double getElevation(const MapPos& pos) const;
        /**
         * Batch version of getElevation. One elevation value is returned for every input position, in order.
         */
        std::vector<double> getElevations(const std::vector<MapPos>& poses) const;

        /**
         * Returns the elevation in meters at the given internal coordinates. Returns 0 if no data is available.
         */
        double getElevationMeters(double internalX, double internalY, LoadMode mode) const;
        /**
         * Returns the display height (internal z units, including exaggeration and Mercator scale)
         * at the given internal coordinates. Returns 0 if no data is available.
         */
        double getDisplayHeight(double internalX, double internalY, LoadMode mode) const;
        /**
         * Returns the display height gradient (dz/dx, dz/dy, unitless) at the given internal coordinates.
         */
        void getDisplayGradient(double internalX, double internalY, LoadMode mode, double& dhdx, double& dhdy) const;

        /**
         * Returns the decoded elevation grid covering the given tile (the tile zoom is clamped to the
         * data source zoom range and cached ancestors act as fallbacks). May return null.
         * The tile must be in XYZ convention (y=0 north, same as vt::TileId and TileDataSource::loadTile).
         */
        std::shared_ptr<ElevationTileGrid> getTileGrid(const MapTile& mapTile, LoadMode mode) const;

        /**
         * Returns the tile that actually carries the elevation data for the given tile: the same
         * tile, or its ancestor at the data source maximum zoom level.
         */
        MapTile getDataTile(const MapTile& mapTile) const;

        /**
         * Requests an asynchronous load of the given elevation tile. Never blocks and never
         * performs IO on the calling thread. A no-op if the grid is already cached, already
         * queued or currently being loaded, or if neighbour prefetching is disabled.
         * Priority 2 (the tile's own elevation level) is served before 1 (edge neighbours),
         * which is served before 0 (diagonal neighbours, which only fill a corner texel).
         * The tile must be in XYZ convention (y=0 north, same as getTileGrid).
         */
        void prefetchTileGrid(const MapTile& mapTile, int priority) const;

        /**
         * Returns the meters-to-internal-display-units scale at the given internal y coordinate
         * (Mercator latitude correction included, exaggeration not included).
         */
        double getDisplayScale(double internalY) const;

        /**
         * Returns the conservative global display height range (internal z units) using
         * the latitude scale at the given internal y coordinate.
         */
        void getDisplayHeightRange(double internalY, double& minZ, double& maxZ) const;

        virtual double getDisplayHeight(double internalX, double internalY) const override;
        virtual bool intersectRay(const cglib::ray3<double>& ray, double& t) const override;
        /**
         * The tile must be in XYZ convention (y=0 north, same as vt::TileId).
         */
        virtual void getMinMaxDisplayHeight(const MapTile& tile, double& minZ, double& maxZ) const override;
        /**
         * Like getMinMaxDisplayHeight, but without the conservative clamp that always includes
         * sea level. Only valid where a loose range costs more than a missing one - fitting a
         * shadow box, not culling. Falls back to the clamped range when the tile has no data.
         */
        void getMinMaxDisplayHeightExact(const MapTile& tile, double& minZ, double& maxZ) const;
        virtual unsigned int getVersion() const override;

        /**
         * Resolves the effective elevation decoder: the data source "encoding" setting takes precedence,
         * then the preferred decoder, then the MapBox decoder as the default.
         */
        static std::shared_ptr<ElevationDecoder> ResolveDecoder(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& preferredDecoder);

    private:
        struct DataSourceListener;

        void tilesChanged();
        double wrapInternalX(double internalX) const;
        MapTile clampTileZoom(const MapTile& mapTile) const;
        std::shared_ptr<ElevationTileGrid> getGridForInternalPos(double internalX, double internalY, LoadMode mode) const;
        std::shared_ptr<ElevationTileGrid> loadTileGrid(const MapTile& mapTile) const;
        void getMinMaxDisplayHeight(const MapTile& tile, double& minZ, double& maxZ, bool exact) const;
        void runPrefetchWorker() const;

        const std::shared_ptr<TileDataSource> _dataSource;
        const std::shared_ptr<ElevationDecoder> _elevationDecoder;
        const std::shared_ptr<Projection> _projection;
        std::shared_ptr<DataSourceListener> _dataSourceListener;

        std::atomic<float> _exaggeration;
        std::atomic<bool> _seamlessTileEdges;
        std::atomic<int> _surfaceResolution;      // terrain mesh cells per tile edge
        mutable std::atomic<int> _gridSizeHint;   // texels per elevation tile edge, from the last decoded grid
        std::atomic<bool> _neighbourPrefetch;
        mutable std::atomic<unsigned int> _version;
        mutable std::atomic<float> _maxSeenElevation;

        mutable cache::timed_lru_cache<long long, std::shared_ptr<ElevationTileGrid> > _gridCache;
        mutable std::map<long long, std::shared_future<std::shared_ptr<ElevationTileGrid> > > _pendingLoads; // single-flight de-duplication of concurrent loads
        mutable std::mutex _mutex;

        // Background prefetch worker: loads elevation tiles requested by the render thread
        // (visible tiles + their neighbours) without ever blocking it. The thread is started
        // on the first request and joined in the destructor.
        mutable std::deque<MapTile> _prefetchQueue;      // low priority (neighbour borders)
        mutable std::deque<MapTile> _prefetchQueueHigh;  // high priority (the tile's own level)
        mutable std::set<long long> _prefetchTileIds;
        mutable std::vector<std::thread> _prefetchThreads;
        mutable std::condition_variable _prefetchCondition;
        mutable bool _prefetchStopped;
        mutable std::mutex _prefetchMutex;
    };
}

#endif
