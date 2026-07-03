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
#include <memory>
#include <mutex>
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
            ALLOW_LOAD
        };

        ElevationManager(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder);
        virtual ~ElevationManager();

        std::shared_ptr<TileDataSource> getDataSource() const;
        std::shared_ptr<ElevationDecoder> getElevationDecoder() const;

        float getExaggeration() const;
        void setExaggeration(float exaggeration);

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
         */
        std::shared_ptr<ElevationTileGrid> getTileGrid(const MapTile& mapTile, LoadMode mode) const;

        /**
         * Returns the meters-to-internal-display-units scale at the given internal y coordinate
         * (Mercator latitude correction included, exaggeration not included).
         */
        double getDisplayScale(double internalY) const;

        virtual double getDisplayHeight(double internalX, double internalY) const override;
        virtual bool intersectRay(const cglib::ray3<double>& ray, double& t) const override;
        virtual void getMinMaxDisplayHeight(const MapTile& tile, double& minZ, double& maxZ) const override;
        virtual unsigned int getVersion() const override;

        /**
         * Resolves the effective elevation decoder: the data source "encoding" setting takes precedence,
         * then the preferred decoder, then the MapBox decoder as the default.
         */
        static std::shared_ptr<ElevationDecoder> ResolveDecoder(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& preferredDecoder);

    private:
        struct DataSourceListener;

        static const std::size_t DEFAULT_CACHE_CAPACITY = 32 * 1024 * 1024;
        static const int FAILED_TILE_TTL_MILLISECONDS = 30 * 1000;
        static const int MAX_ANCESTOR_SEARCH_DEPTH = 8;
        static constexpr double NO_DATA_ELEVATION = -1000000.0;
        static constexpr double DEFAULT_MIN_ELEVATION = -500.0;
        static constexpr double DEFAULT_MAX_ELEVATION = 9000.0;
        static constexpr int RAY_MARCH_MAX_STEPS = 256;
        static constexpr int RAY_BISECT_STEPS = 24;

        void tilesChanged();
        double wrapInternalX(double internalX) const;
        MapTile clampTileZoom(const MapTile& mapTile) const;
        std::shared_ptr<ElevationTileGrid> getGridForInternalPos(double internalX, double internalY, LoadMode mode) const;
        std::shared_ptr<ElevationTileGrid> loadTileGrid(const MapTile& mapTile) const;

        const std::shared_ptr<TileDataSource> _dataSource;
        const std::shared_ptr<ElevationDecoder> _elevationDecoder;
        const std::shared_ptr<Projection> _projection;
        std::shared_ptr<DataSourceListener> _dataSourceListener;

        std::atomic<float> _exaggeration;
        mutable std::atomic<unsigned int> _version;
        mutable std::atomic<float> _maxSeenElevation;

        mutable cache::timed_lru_cache<long long, std::shared_ptr<ElevationTileGrid> > _gridCache;
        mutable std::mutex _mutex;
    };
}

#endif
