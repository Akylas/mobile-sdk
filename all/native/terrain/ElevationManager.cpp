#include "ElevationManager.h"
#include "terrain/ElevationTileGrid.h"
#include "core/BinaryData.h"
#include "datasources/TileDataSource.h"
#include "datasources/components/TileData.h"
#include "graphics/Bitmap.h"
#include "projections/Projection.h"
#include "rastertiles/ElevationDecoder.h"
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include "utils/Const.h"
#include "utils/Log.h"
#include "utils/TileUtils.h"

#include <algorithm>
#include <cmath>

namespace carto {

    struct ElevationManager::DataSourceListener : public TileDataSource::OnChangeListener {
        explicit DataSourceListener(ElevationManager& manager) : _manager(manager) { }

        virtual void onTilesChanged(bool removeTiles) override {
            _manager.tilesChanged();
        }

    private:
        ElevationManager& _manager;
    };

    ElevationManager::ElevationManager(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder) :
        _dataSource(dataSource),
        _elevationDecoder(ResolveDecoder(dataSource, elevationDecoder)),
        _projection(dataSource->getProjection()),
        _dataSourceListener(),
        _exaggeration(1.0f),
        _version(1),
        _maxSeenElevation(0.0f),
        _gridCache(DEFAULT_CACHE_CAPACITY),
        _mutex()
    {
        _dataSourceListener = std::make_shared<DataSourceListener>(*this);
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }

    ElevationManager::~ElevationManager() {
        _dataSource->unregisterOnChangeListener(_dataSourceListener);
    }

    std::shared_ptr<TileDataSource> ElevationManager::getDataSource() const {
        return _dataSource;
    }

    std::shared_ptr<ElevationDecoder> ElevationManager::getElevationDecoder() const {
        return _elevationDecoder;
    }

    float ElevationManager::getExaggeration() const {
        return _exaggeration.load();
    }

    void ElevationManager::setExaggeration(float exaggeration) {
        _exaggeration.store(std::max(0.0f, exaggeration));
        _version++;
    }

    std::size_t ElevationManager::getCacheCapacity() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _gridCache.capacity();
    }

    void ElevationManager::setCacheCapacity(std::size_t capacityInBytes) {
        std::lock_guard<std::mutex> lock(_mutex);
        _gridCache.resize(capacityInBytes);
    }

    double ElevationManager::getElevation(const MapPos& pos) const {
        MapPos dataSourcePos = _projection->fromWgs84(pos);
        MapTile mapTile = TileUtils::CalculateMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection);
        std::shared_ptr<ElevationTileGrid> grid = getTileGrid(mapTile, LoadMode::ALLOW_LOAD);
        if (!grid) {
            Log::Error("ElevationManager::getElevation: no tile found to get elevation");
            return NO_DATA_ELEVATION;
        }
        MapPos internalPos = _projection->toInternal(dataSourcePos);
        return grid->sampleHeight(internalPos.getX(), internalPos.getY());
    }

    std::vector<double> ElevationManager::getElevations(const std::vector<MapPos>& poses) const {
        std::vector<double> results;
        results.reserve(poses.size());
        for (const MapPos& pos : poses) {
            MapPos dataSourcePos = _projection->fromWgs84(pos);
            MapTile mapTile = TileUtils::CalculateMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection);
            std::shared_ptr<ElevationTileGrid> grid = getTileGrid(mapTile, LoadMode::ALLOW_LOAD);
            if (grid) {
                MapPos internalPos = _projection->toInternal(dataSourcePos);
                results.push_back(grid->sampleHeight(internalPos.getX(), internalPos.getY()));
            } else {
                results.push_back(NO_DATA_ELEVATION);
            }
        }
        return results;
    }

    double ElevationManager::getElevationMeters(double internalX, double internalY, LoadMode mode) const {
        double wrappedX = wrapInternalX(internalX);
        std::shared_ptr<ElevationTileGrid> grid = getGridForInternalPos(wrappedX, internalY, mode);
        if (!grid) {
            return 0.0;
        }
        return grid->sampleHeight(wrappedX, internalY);
    }

    double ElevationManager::getDisplayHeight(double internalX, double internalY, LoadMode mode) const {
        double meters = getElevationMeters(internalX, internalY, mode);
        return meters * _exaggeration.load() * getDisplayScale(internalY);
    }

    void ElevationManager::getDisplayGradient(double internalX, double internalY, LoadMode mode, double& dhdx, double& dhdy) const {
        dhdx = 0;
        dhdy = 0;
        double wrappedX = wrapInternalX(internalX);
        std::shared_ptr<ElevationTileGrid> grid = getGridForInternalPos(wrappedX, internalY, mode);
        if (!grid) {
            return;
        }
        float gradX = 0, gradY = 0;
        grid->sampleGradient(wrappedX, internalY, gradX, gradY);
        double scale = _exaggeration.load() * getDisplayScale(internalY);
        dhdx = gradX * scale;
        dhdy = gradY * scale;
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::getTileGrid(const MapTile& mapTile, LoadMode mode) const {
        MapTile tile = clampTileZoom(mapTile);
        if (tile.getZoom() < _dataSource->getMinZoom()) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        // Look for the tile or any of its cached ancestors
        bool tileFailed = false;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            MapTile searchTile = tile;
            for (int depth = 0; depth <= MAX_ANCESTOR_SEARCH_DEPTH; depth++) {
                std::shared_ptr<ElevationTileGrid> grid;
                if (_gridCache.read(searchTile.getTileId(), grid)) {
                    if (grid) {
                        return grid;
                    }
                    if (searchTile == tile) {
                        tileFailed = true; // recently failed to load, do not retry until the failure marker expires
                    }
                }
                if (searchTile.getZoom() <= _dataSource->getMinZoom()) {
                    break;
                }
                searchTile = searchTile.getParent();
            }
        }

        if (mode == LoadMode::CACHED_ONLY || tileFailed) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        // Load and decode outside of the lock. Duplicate concurrent loads are possible but benign.
        std::shared_ptr<ElevationTileGrid> grid = loadTileGrid(tile);
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (grid) {
                _gridCache.put(grid->getTile().getTileId(), grid, grid->getDataSize());
                if (grid->getTile() != tile) {
                    // Loaded an ancestor (replace-with-parent); also mark the requested tile as resolved via ancestor
                    _gridCache.put(tile.getTileId(), grid, 1024);
                }
                float maxSeen = _maxSeenElevation.load();
                while (grid->getMaxHeight() > maxSeen && !_maxSeenElevation.compare_exchange_weak(maxSeen, grid->getMaxHeight())) { }
            } else {
                _gridCache.put(tile.getTileId(), std::shared_ptr<ElevationTileGrid>(), 1024);
                _gridCache.invalidate(tile.getTileId(), std::chrono::steady_clock::now() + std::chrono::milliseconds(FAILED_TILE_TTL_MILLISECONDS));
            }
        }
        if (grid) {
            _version++;
        }
        return grid;
    }

    double ElevationManager::getDisplayScale(double internalY) const {
        double sin = std::tanh(internalY * 2 * Const::PI / Const::WORLD_SIZE);
        double cos = std::sqrt(std::max(1.0e-6, 1.0 - sin * sin));
        return Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE / cos;
    }

    void ElevationManager::getDisplayHeightRange(double internalY, double& minZ, double& maxZ) const {
        double maxMeters = std::max(static_cast<double>(_maxSeenElevation.load()), DEFAULT_MAX_ELEVATION);
        double scale = _exaggeration.load() * getDisplayScale(internalY);
        minZ = DEFAULT_MIN_ELEVATION * scale;
        maxZ = maxMeters * scale;
    }

    double ElevationManager::getDisplayHeight(double internalX, double internalY) const {
        return getDisplayHeight(internalX, internalY, LoadMode::CACHED_ONLY);
    }

    bool ElevationManager::intersectRay(const cglib::ray3<double>& ray, double& t) const {
        if (ray.direction(2) >= 0) {
            return false; // upward/horizontal rays can not hit terrain from above
        }

        float exaggeration = _exaggeration.load();
        double maxElevation = std::max(static_cast<double>(_maxSeenElevation.load()), DEFAULT_MAX_ELEVATION);

        // Conservative display-space search interval. Use the largest latitude scale along the ray
        // to be safe; heights are re-sampled precisely at each march step anyway.
        double tGround = -ray.origin(2) / ray.direction(2);
        double scale0 = getDisplayScale(ray.origin(1));
        double scale1 = getDisplayScale(ray(tGround)(1));
        double maxScale = std::max(scale0, scale1);
        double zTop = maxElevation * exaggeration * maxScale;
        double zBottom = DEFAULT_MIN_ELEVATION * exaggeration * maxScale;

        double t0 = 0;
        if (ray.origin(2) > zTop) {
            t0 = (zTop - ray.origin(2)) / ray.direction(2);
        }
        double t1 = (zBottom - ray.origin(2)) / ray.direction(2);
        if (!(t1 > t0)) {
            return false;
        }

        // March with quadratically increasing steps (dense near the origin, coarse far away),
        // then refine the first crossing with bisection.
        double prevT = t0;
        cglib::vec3<double> pos = ray(t0);
        double prevDelta = pos(2) - getDisplayHeight(pos(0), pos(1), LoadMode::CACHED_ONLY);
        if (prevDelta <= 0) {
            t = t0;
            return true;
        }
        for (int i = 1; i <= RAY_MARCH_MAX_STEPS; i++) {
            double f = static_cast<double>(i) / RAY_MARCH_MAX_STEPS;
            double curT = t0 + (t1 - t0) * f * f;
            pos = ray(curT);
            double delta = pos(2) - getDisplayHeight(pos(0), pos(1), LoadMode::CACHED_ONLY);
            if (delta <= 0) {
                double tLow = prevT;
                double tHigh = curT;
                for (int j = 0; j < RAY_BISECT_STEPS; j++) {
                    double tMid = (tLow + tHigh) * 0.5;
                    pos = ray(tMid);
                    double midDelta = pos(2) - getDisplayHeight(pos(0), pos(1), LoadMode::CACHED_ONLY);
                    if (midDelta <= 0) {
                        tHigh = tMid;
                    } else {
                        tLow = tMid;
                    }
                }
                t = (tLow + tHigh) * 0.5;
                return true;
            }
            prevT = curT;
            prevDelta = delta;
        }
        return false;
    }

    void ElevationManager::getMinMaxDisplayHeight(const MapTile& tile, double& minZ, double& maxZ) const {
        double minMeters = DEFAULT_MIN_ELEVATION;
        double maxMeters = std::max(static_cast<double>(_maxSeenElevation.load()), DEFAULT_MAX_ELEVATION);
        MapBounds bounds = TileUtils::CalculateMapTileBounds(tile, _projection);
        if (std::shared_ptr<ElevationTileGrid> grid = getTileGrid(tile, LoadMode::CACHED_ONLY)) {
            minMeters = grid->getMinHeight();
            maxMeters = grid->getMaxHeight();
        }
        MapPos internalCenter = _projection->toInternal(bounds.getCenter());
        MapPos internalMin = _projection->toInternal(bounds.getMin());
        MapPos internalMax = _projection->toInternal(bounds.getMax());
        double scale = std::max(getDisplayScale(internalMin.getY()), std::max(getDisplayScale(internalMax.getY()), getDisplayScale(internalCenter.getY())));
        double exaggeration = _exaggeration.load();
        minZ = std::min(0.0, minMeters * exaggeration * scale);
        maxZ = std::max(0.0, maxMeters * exaggeration * scale);
    }

    unsigned int ElevationManager::getVersion() const {
        return _version.load();
    }

    std::shared_ptr<ElevationDecoder> ElevationManager::ResolveDecoder(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& preferredDecoder) {
        std::string encoding = dataSource ? dataSource->getEncoding() : std::string();
        if (encoding == "terrarium") {
            static std::shared_ptr<ElevationDecoder> terrariumDecoder = std::make_shared<TerrariumElevationDataDecoder>();
            return terrariumDecoder;
        }
        if (encoding == "mapbox") {
            static std::shared_ptr<ElevationDecoder> mapboxDecoder = std::make_shared<MapBoxElevationDataDecoder>();
            return mapboxDecoder;
        }
        if (preferredDecoder) {
            return preferredDecoder;
        }
        static std::shared_ptr<ElevationDecoder> defaultDecoder = std::make_shared<MapBoxElevationDataDecoder>();
        return defaultDecoder;
    }

    void ElevationManager::tilesChanged() {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _gridCache.clear();
        }
        _version++;
    }

    double ElevationManager::wrapInternalX(double internalX) const {
        double worldSize = Const::WORLD_SIZE;
        return internalX - worldSize * std::floor(internalX / worldSize + 0.5);
    }

    MapTile ElevationManager::clampTileZoom(const MapTile& mapTile) const {
        MapTile tile = mapTile;
        int maxZoom = _dataSource->getMaxZoom();
        while (tile.getZoom() > maxZoom) {
            tile = tile.getParent();
        }
        return tile;
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::getGridForInternalPos(double internalX, double internalY, LoadMode mode) const {
        MapPos dataSourcePos = _projection->fromInternal(MapPos(internalX, internalY, 0));
        MapTile mapTile = TileUtils::CalculateClippedMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection);
        return getTileGrid(mapTile, mode);
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::loadTileGrid(const MapTile& requestedTile) const {
        MapTile mapTile = requestedTile;
        MapTile flippedMapTile = mapTile.getFlipped();
        std::shared_ptr<TileData> tileData = _dataSource->loadTile(flippedMapTile);
        while (tileData && tileData->isReplaceWithParent() && mapTile.getZoom() > 0) {
            mapTile = mapTile.getParent();
            flippedMapTile = mapTile.getFlipped();
            tileData = _dataSource->loadTile(flippedMapTile);
        }
        if (!tileData || !tileData->getData()) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        std::shared_ptr<Bitmap> tileBitmap = Bitmap::CreateFromCompressed(tileData->getData());
        if (!tileBitmap) {
            Log::Error("ElevationManager::loadTileGrid: Failed to decode elevation tile bitmap");
            return std::shared_ptr<ElevationTileGrid>();
        }

        MapBounds bounds = TileUtils::CalculateMapTileBounds(mapTile, _projection);
        MapPos internalMin = _projection->toInternal(bounds.getMin());
        MapPos internalMax = _projection->toInternal(bounds.getMax());
        MapBounds internalBounds(MapPos(std::min(internalMin.getX(), internalMax.getX()), std::min(internalMin.getY(), internalMax.getY())),
                                 MapPos(std::max(internalMin.getX(), internalMax.getX()), std::max(internalMin.getY(), internalMax.getY())));

        std::array<double, 4> coeffs = _elevationDecoder->getColorComponentCoefficients();
        return ElevationTileGrid::DecodeBitmap(mapTile, internalBounds, tileBitmap, coeffs);
    }
}
