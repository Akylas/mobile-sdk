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
#include <limits>

namespace carto {

    static const std::size_t DEFAULT_CACHE_CAPACITY = 64 * 1024 * 1024;
    // Grids the cache must hold whatever the source resolution is. A fixed byte budget is really a
    // tile budget the DEM raster size decides: a 512x512 RGB source is 768 KB per grid, so 64 MB
    // is 85 grids - while one terrain view asked for 122-167 distinct grids (the cover pyramid,
    // TileLayer::TERRAIN_COVER_TILE_BUDGET tiles collapsing onto ~160 DEM tiles once clamped, plus
    // the contour source's finer tiles and the border prefetch). Every grid past the limit evicted
    // one still in use and was decoded again on the next pass. Measured on a Crosscall at the demo
    // camera, per startup: 85 grids = 1525 loads of 167 tiles and 32 s of WEBP decode, 128 grids =
    // 189 loads of 157, 192 grids = 157 loads of 157 and 3.1 s of decode.
    // An app that cannot spend the memory (144 MB here, 36 MB for a 256x256 source) sets its own
    // number with TerrainOptions::setElevationCacheCapacity, which then wins over this rule.
    static const std::size_t MIN_CACHED_GRIDS = 192;
    static const int FAILED_TILE_TTL_MILLISECONDS = 30 * 1000;
    static const int MAX_ANCESTOR_SEARCH_DEPTH = 8;
    static const std::size_t MAX_PREFETCH_QUEUE_SIZE = 64;
    static const int PREFETCH_THREADS = 3; // elevation tiles are network+decode bound; one worker converges too slowly
    static constexpr double NO_DATA_ELEVATION = -1000000.0;
    static constexpr double DEFAULT_MIN_ELEVATION = -500.0;
    static constexpr double DEFAULT_MAX_ELEVATION = 9000.0;
    static constexpr int RAY_MARCH_MAX_STEPS = 256;
    static constexpr int RAY_BISECT_STEPS = 24;
    // Latitude quantum of the metres-to-internal scale memo, ~40 m of world (see getDisplayScale).
    static const double DISPLAY_SCALE_STEP = Const::WORLD_SIZE / 1048576.0;

    struct ElevationManager::DataSourceListener : public TileDataSource::OnChangeListener {
        explicit DataSourceListener(ElevationManager& manager) : _manager(manager) { }

        virtual void onTilesChanged(bool removeTiles) override {
            _manager.tilesChanged();
        }

    private:
        ElevationManager& _manager;
    };

    unsigned long long ElevationManager::NextInstanceId() {
        static std::atomic<unsigned long long> counter { 0 };
        return counter.fetch_add(1) + 1;
    }

    ElevationManager::ElevationManager(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder) :
        _dataSource(dataSource),
        _elevationDecoder(ResolveDecoder(dataSource, elevationDecoder)),
        _projection(dataSource->getProjection()),
        _dataSourceListener(),
        _instanceId(NextInstanceId()),
        _exaggeration(1.0f),
        _seamlessTileEdges(true),
        _surfaceResolution(32),
        _gridSizeHint(256),
        _neighbourPrefetch(true),
        _version(1),
        _dataVersion(1),
        _maxSeenElevation(0.0f),
        _gridCache(DEFAULT_CACHE_CAPACITY),
        _mutex(),
        _prefetchStopped(false)
    {
        _dataSourceListener = std::make_shared<DataSourceListener>(*this);
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }

    ElevationManager::~ElevationManager() {
        {
            std::lock_guard<std::mutex> lock(_prefetchMutex);
            _prefetchStopped = true;
            _prefetchQueue.clear();
            _prefetchQueueHigh.clear();
            _prefetchTileIds.clear();
        }
        _prefetchCondition.notify_all();
        for (std::thread& thread : _prefetchThreads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
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
        float value = std::max(0.0f, exaggeration);
        if (_exaggeration.exchange(value) == value) {
            return; // re-setting the same value invalidates every CPU-side height for nothing
        }
        // The DATA version deliberately stands still: heights are scaled on the GPU and the tile
        // surfaces are built flat, so nothing geometric is stale. Only what reads heights on the
        // CPU - label anchors, the raycast - has to catch up, and that watches the global version.
        bumpGlobalVersion();
    }

    bool ElevationManager::isSeamlessTileEdgesEnabled() const {
        return _seamlessTileEdges.load();
    }

    void ElevationManager::setSeamlessTileEdgesEnabled(bool enabled) {
        if (_seamlessTileEdges.exchange(enabled) != enabled) {
            _version++; _dataVersion++; // elevation texture borders change, force a rebuild
        }
    }

    void ElevationManager::setSurfaceResolution(int resolution) {
        int value = std::max(1, resolution);
        if (_surfaceResolution.exchange(value) != value) {
            _version++; _dataVersion++; // the elevation level cap changes with it
        }
    }

    bool ElevationManager::isNeighbourPrefetchEnabled() const {
        return _neighbourPrefetch.load();
    }

    void ElevationManager::setNeighbourPrefetchEnabled(bool enabled) {
        _neighbourPrefetch.store(enabled);
    }

    std::size_t ElevationManager::getCacheCapacity() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _gridCache.capacity();
    }

    void ElevationManager::setCacheCapacity(std::size_t capacityInBytes) {
        std::lock_guard<std::mutex> lock(_mutex);
        _gridCacheCapacityFixed = true;
        _gridCache.resize(capacityInBytes);
    }

    double ElevationManager::getElevation(const MapPos& pos) const {
        MapPos dataSourcePos = _projection->fromWgs84(pos);
        // TileUtils returns TMS-convention tiles (y=0 south); getTileGrid expects XYZ (y=0 north)
        MapTile mapTile = TileUtils::CalculateMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection).getFlipped();
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
            MapTile mapTile = TileUtils::CalculateMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection).getFlipped();
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
        return lookupTileGrid(clampTileZoom(mapTile), mode);
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::getDataTileGrid(const MapTile& dataTile, LoadMode mode) const {
        return lookupTileGrid(clampDataTileZoom(dataTile), mode);
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::lookupTileGrid(const MapTile& tile, LoadMode mode) const {
        if (tile.getZoom() < _dataSource->getMinZoom()) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        // Dense point queries - label re-anchoring walks every vertex of every label, the
        // terrain raycast marches a ray - ask for the same tile thousands of times in a row,
        // and every one of them takes the cache mutex and walks the ancestor chain. Remember
        // the last resolved (tile -> grid) per thread: grids are immutable and every
        // elevation change bumps the version, so a memo of the same version is the same
        // answer the walk below would produce. LOAD_EXACT is excluded on purpose - it must
        // not be satisfied by a grid resolved through the ancestor search.
        // The MODE is part of the key. CACHED_ONLY accepts a cached ANCESTOR as a stand-in while
        // ALLOW_LOAD loads the tile itself, so the two resolve the same tile to different grids
        // while a level is still streaming in. Without the mode here, whichever query ran first on
        // this thread answered the other one: a CACHED_ONLY lookup would memoise a coarse ancestor
        // and hand it to the next ALLOW_LOAD caller. Two consumers of the same ground then disagree
        // about its height by the LOD chord error - metres on flat ground, tens of metres on
        // relief - which is what made the terrain shadow map and the surface that reads it drift
        // apart after a zoom change, shadowing the ground and the buildings with their own depth.
        struct GridMemo {
            unsigned long long instanceId = 0;
            unsigned int version = 0;
            long long tileId = -1;
            LoadMode mode = LoadMode::CACHED_ONLY;
            std::shared_ptr<ElevationTileGrid> grid;
        };
        static thread_local GridMemo memo;
        unsigned int memoVersion = _version.load();
        bool memoizable = (mode != LoadMode::LOAD_EXACT);
        if (memoizable && memo.instanceId == _instanceId && memo.version == memoVersion && memo.tileId == tile.getTileId() && memo.mode == mode && memo.grid) {
            return memo.grid;
        }

        // Look for the tile or any of its cached ancestors
        bool tileFailed = false;
        if (mode == LoadMode::LOAD_EXACT) {
            // The caller wants THIS level, not a stand-in: a cached ancestor must not short
            // circuit the load, or a tile that once fell back to its parent would stay on it
            // forever - and neighbouring tiles displaced by different elevation levels tear the
            // surface open along their shared edge. A grid cached under this tile id that covers
            // an ancestor is the data source saying the level does not exist here, so it stands.
            std::lock_guard<std::mutex> lock(_mutex);
            std::shared_ptr<ElevationTileGrid> grid;
            if (_gridCache.read(tile.getTileId(), grid)) {
                if (grid) {
                    return grid;
                }
                tileFailed = true; // recently failed to load, do not retry until the failure marker expires
            }
        } else {
            std::lock_guard<std::mutex> lock(_mutex);
            MapTile searchTile = tile;
            for (int depth = 0; depth <= MAX_ANCESTOR_SEARCH_DEPTH; depth++) {
                std::shared_ptr<ElevationTileGrid> grid;
                if (_gridCache.read(searchTile.getTileId(), grid)) {
                    if (grid) {
                        memo = GridMemo { _instanceId, memoVersion, tile.getTileId(), mode, grid };
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

        // Single-flight: many tile fetch threads typically request the same elevation tile
        // at nearly the same time (16 layer tiles can share one clamped elevation tile).
        // Only the first caller performs the load; the others wait for its result.
        long long tileId = tile.getTileId();
        std::promise<std::shared_ptr<ElevationTileGrid> > promise;
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _pendingLoads.find(tileId);
            if (it != _pendingLoads.end()) {
                std::shared_future<std::shared_ptr<ElevationTileGrid> > future = it->second;
                lock.unlock();
                return future.get();
            }
            _pendingLoads[tileId] = promise.get_future().share();
        }

        // Load and decode outside of the lock
        std::shared_ptr<ElevationTileGrid> grid;
        try {
            grid = loadTileGrid(tile);
        }
        catch (...) {
            std::lock_guard<std::mutex> lock(_mutex);
            _pendingLoads.erase(tileId);
            promise.set_value(std::shared_ptr<ElevationTileGrid>());
            throw;
        }
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (grid) {
                // Grow the budget to the source's own grid size before inserting, so the cache is
                // a tile count rather than a byte count the raster resolution decides (see
                // MIN_CACHED_GRIDS). Only ever grows, and a caller that set its own capacity keeps it.
                std::size_t minCapacity = grid->getDataSize() * MIN_CACHED_GRIDS;
                if (!_gridCacheCapacityFixed && _gridCache.capacity() < minCapacity) {
                    Log::Infof("ElevationManager: elevation grid cache %d -> %d MB (%d grids of %d KB)",
                               static_cast<int>(_gridCache.capacity() >> 20), static_cast<int>(minCapacity >> 20),
                               static_cast<int>(MIN_CACHED_GRIDS), static_cast<int>(grid->getDataSize() >> 10));
                    _gridCache.resize(minCapacity);
                }
                _gridCache.put(grid->getTile().getTileId(), grid, grid->getDataSize());
                if (grid->getTile() != tile) {
                    // Loaded an ancestor (replace-with-parent); also mark the requested tile as resolved via ancestor
                    _gridCache.put(tileId, grid, 1024);
                }
                float maxSeen = _maxSeenElevation.load();
                while (grid->getMaxHeight() > maxSeen && !_maxSeenElevation.compare_exchange_weak(maxSeen, grid->getMaxHeight())) { }

                // Record WHICH tile changed, not just that something did. Consumers with
                // per-tile derived data can then rebuild only the affected tiles. Bump the
                // version under the same lock as the cache insert, so a consumer that sees
                // the new version also sees the grid and a matching log entry.
                // The DATA version moves with it: a decoded tile IS new elevation data, and a
                // consumer telling a scale-only change apart from a data change compares the
                // two. While this only moved for whole-data-set changes, every tile load read
                // as scale-only, which takes the blanket invalidation path - a whole screen of
                // label anchors resampled per arriving tile instead of those over that tile.
                _dataVersion++;
                unsigned int version = _version.fetch_add(1) + 1;
                _changeLog.emplace_back(version, grid->getTile());
                while (_changeLog.size() > MAX_CHANGE_LOG_ENTRIES) {
                    _changeLog.pop_front();
                    _changeLogFirstVersion = _changeLog.front().first;
                }
            } else {
                _gridCache.put(tileId, std::shared_ptr<ElevationTileGrid>(), 1024);
                _gridCache.invalidate(tileId, std::chrono::steady_clock::now() + std::chrono::milliseconds(FAILED_TILE_TTL_MILLISECONDS));
            }
            _pendingLoads.erase(tileId);
        }
        promise.set_value(grid);
        return grid;
    }

    MapTile ElevationManager::getDataTile(const MapTile& mapTile) const {
        return clampTileZoom(mapTile);
    }

    MapTile ElevationManager::getDetailDataTile(const MapTile& mapTile, int extraLevels) const {
        // Kept for callers that ask for MORE than the standard rule gives; the rule itself no
        // longer holds anything back (see clampTileZoom), so extraLevels only ever removes the
        // source zoom bias, and never goes below the tile's own zoom.
        if (extraLevels <= 0) {
            return clampTileZoom(mapTile);
        }
        MapTile tile = mapTile;
        int limit = DEM_TEXELS_PER_TILE_UNIT << extraLevels;
        for (int size = _gridSizeHint.load(); size > limit && tile.getZoom() > 0; size /= 2) {
            tile = tile.getParent();
        }
        return clampDataTileZoom(tile);
    }

    MapTile ElevationManager::getFullDetailDataTile(const MapTile& mapTile) const {
        // No mesh-resolution cap: a per-fragment consumer (hillshade shading) resolves relief the
        // surface geometry cannot, so capping it there leaves it blurred by two zoom levels.
        return clampDataTileZoom(mapTile);
    }

    void ElevationManager::prefetchTileGrid(const MapTile& dataTile, int priority) const {
        if (!_neighbourPrefetch.load()) {
            return;
        }
        MapTile tile = clampDataTileZoom(dataTile);
        if (tile.getZoom() < _dataSource->getMinZoom()) {
            return;
        }

        long long tileId = tile.getTileId();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            std::shared_ptr<ElevationTileGrid> grid;
            if (_gridCache.read(tileId, grid)) {
                return; // already loaded, resolved via an ancestor, or recently failed
            }
            if (_pendingLoads.find(tileId) != _pendingLoads.end()) {
                return; // already being loaded by another thread
            }
        }

        {
            std::lock_guard<std::mutex> lock(_prefetchMutex);
            if (_prefetchStopped) {
                return;
            }
            if (!_prefetchTileIds.insert(tileId).second) {
                return; // already queued
            }
            std::deque<MapTile>& queue = (priority >= 2 ? _prefetchQueueHigh : _prefetchQueue);
            // The queues are drained newest first, so the lowest priority requests (diagonal
            // neighbours) go to the far end instead of the near one.
            if (priority <= 0) {
                queue.push_front(tile);
            } else {
                queue.push_back(tile);
            }
            while (queue.size() > MAX_PREFETCH_QUEUE_SIZE) {
                _prefetchTileIds.erase(queue.front().getTileId());
                queue.pop_front();
            }
            while (static_cast<int>(_prefetchThreads.size()) < PREFETCH_THREADS) {
                _prefetchThreads.emplace_back([this]() { runPrefetchWorker(); });
            }
        }
        _prefetchCondition.notify_one();
    }

    void ElevationManager::runPrefetchWorker() const {
        while (true) {
            MapTile tile(0, 0, 0, 0);
            {
                std::unique_lock<std::mutex> lock(_prefetchMutex);
                _prefetchCondition.wait(lock, [this]() { return _prefetchStopped || !_prefetchQueue.empty() || !_prefetchQueueHigh.empty(); });
                if (_prefetchStopped) {
                    return;
                }
                // High priority (a tile's own elevation level) before border neighbours, and
                // newest request first: it belongs to the current viewport, while the oldest
                // entries may already have scrolled out of view.
                std::deque<MapTile>& queue = (_prefetchQueueHigh.empty() ? _prefetchQueue : _prefetchQueueHigh);
                tile = queue.back();
                queue.pop_back();
                _prefetchTileIds.erase(tile.getTileId());
            }
            try {
                getDataTileGrid(tile, LoadMode::LOAD_EXACT); // queued tiles are elevation tiles already
            }
            catch (const std::exception& ex) {
                Log::Warnf("ElevationManager::runPrefetchWorker: Failed to prefetch elevation tile: %s", ex.what());
            }
        }
    }

    double ElevationManager::getDisplayScale(double internalY) const {
        // The metres-to-internal scale only depends on the latitude, and a dense consumer (the
        // label re-anchor, the raycast) walks points a few metres apart - but tanh() is the
        // single most expensive thing in that loop (measured: tanh + expm1 = 21% of the render
        // thread). Quantise the latitude to DISPLAY_SCALE_STEP and remember the last step: the
        // step spans ~40 m, over which the scale moves by ~4e-7 relative, i.e. under two
        // millimetres on a 3000 m summit. Quantising rather than interpolating keeps it a
        // function of the position alone, so the same vertex always gets the same height and
        // nothing oscillates between frames.
        double step = std::floor(internalY / DISPLAY_SCALE_STEP + 0.5);
        struct ScaleMemo {
            double step = std::numeric_limits<double>::quiet_NaN();
            double scale = 0;
        };
        static thread_local ScaleMemo memo;
        if (memo.step == step) {
            return memo.scale;
        }
        double sin = std::tanh(step * DISPLAY_SCALE_STEP * 2 * Const::PI / Const::WORLD_SIZE);
        double cos = std::sqrt(std::max(1.0e-6, 1.0 - sin * sin));
        double scale = Const::WORLD_SIZE / Const::EARTH_CIRCUMFERENCE / cos;
        memo = ScaleMemo { step, scale };
        return scale;
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

        // Most march steps hit the same elevation grid; keep the last used grid around
        // to avoid a cache lookup (and its mutex) per sample.
        std::shared_ptr<ElevationTileGrid> cachedGrid;
        auto sampleDisplayHeight = [&](double internalX, double internalY) -> double {
            double wrappedX = wrapInternalX(internalX);
            if (!cachedGrid || !cachedGrid->getInternalBounds().contains(MapPos(wrappedX, internalY, 0))) {
                cachedGrid = getGridForInternalPos(wrappedX, internalY, LoadMode::CACHED_ONLY);
            }
            if (!cachedGrid) {
                return 0.0;
            }
            return cachedGrid->sampleHeight(wrappedX, internalY) * exaggeration * getDisplayScale(internalY);
        };

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
        double prevDelta = pos(2) - sampleDisplayHeight(pos(0), pos(1));
        if (prevDelta <= 0) {
            t = t0;
            return true;
        }
        for (int i = 1; i <= RAY_MARCH_MAX_STEPS; i++) {
            double f = static_cast<double>(i) / RAY_MARCH_MAX_STEPS;
            double curT = t0 + (t1 - t0) * f * f;
            pos = ray(curT);
            double delta = pos(2) - sampleDisplayHeight(pos(0), pos(1));
            if (delta <= 0) {
                double tLow = prevT;
                double tHigh = curT;
                for (int j = 0; j < RAY_BISECT_STEPS; j++) {
                    double tMid = (tLow + tHigh) * 0.5;
                    pos = ray(tMid);
                    double midDelta = pos(2) - sampleDisplayHeight(pos(0), pos(1));
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
        getMinMaxDisplayHeight(tile, minZ, maxZ, false);
    }

    void ElevationManager::getMinMaxDisplayHeightExact(const MapTile& tile, double& minZ, double& maxZ) const {
        getMinMaxDisplayHeight(tile, minZ, maxZ, true);
    }

    void ElevationManager::getMinMaxDisplayHeight(const MapTile& tile, double& minZ, double& maxZ, bool exact) const {
        // Fall back to the maximum elevation actually observed so far (starting flat) instead
        // of a large conservative constant: a many-kilometers default bound would pull far
        // tiles into the view frustum, causing them to fetch elevation data, which changes
        // their bounds again - churning the visible tile set while data streams in.
        double minMeters = 0;
        double maxMeters = _maxSeenElevation.load();
        bool haveData = false;
        MapBounds bounds = TileUtils::CalculateMapTileBounds(tile.getFlipped(), _projection);
        if (std::shared_ptr<ElevationTileGrid> grid = getTileGrid(tile, LoadMode::CACHED_ONLY)) {
            minMeters = grid->getMinHeight();
            maxMeters = grid->getMaxHeight();
            haveData = true;
        }
        MapPos internalCenter = _projection->toInternal(bounds.getCenter());
        MapPos internalMin = _projection->toInternal(bounds.getMin());
        MapPos internalMax = _projection->toInternal(bounds.getMax());
        double scale = std::max(getDisplayScale(internalMin.getY()), std::max(getDisplayScale(internalMax.getY()), getDisplayScale(internalCenter.getY())));
        double exaggeration = _exaggeration.load();
        // The bounds normally include sea level whatever the data says, so that a tile without
        // data (which reports nothing) still gets a usable range. A caller that is fitting a box
        // to the terrain rather than culling against it wants the range the ground REALLY spans:
        // a valley tile at 1000..2000 m reported as 0..2000 m doubles the height slab, and a
        // shadow box is stretched by that slab divided by the tangent of the sun's altitude - at a
        // low sun the difference is kilometres of wasted box, i.e. coarser texels everywhere.
        if (exact && haveData) {
            minZ = minMeters * exaggeration * scale;
            maxZ = maxMeters * exaggeration * scale;
            return;
        }
        minZ = std::min(0.0, minMeters * exaggeration * scale);
        maxZ = std::max(0.0, maxMeters * exaggeration * scale);
    }

    unsigned int ElevationManager::getDataVersion() const {
        return _dataVersion.load();
    }

    unsigned int ElevationManager::getVersion() const {
        return _version.load();
    }

    bool ElevationManager::getChangedTiles(unsigned int sinceVersion, std::vector<MapTile>& tiles) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (sinceVersion + 1 < _changeLogFirstVersion) {
            return false;
        }
        for (const std::pair<unsigned int, MapTile>& entry : _changeLog) {
            if (entry.first > sinceVersion) {
                tiles.push_back(entry.second);
            }
        }
        return true;
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
        _dataVersion++;
        bumpGlobalVersion();
    }

    void ElevationManager::bumpGlobalVersion() {
        std::lock_guard<std::mutex> lock(_mutex);

        // Everything derived from the elevation data is stale at once, which the per-tile
        // change log can not express - drop it so that consumers take the full
        // invalidation path until the next tile-level change.
        unsigned int version = _version.fetch_add(1) + 1;
        _changeLog.clear();
        _changeLogFirstVersion = version + 1;
    }

    double ElevationManager::wrapInternalX(double internalX) const {
        double worldSize = Const::WORLD_SIZE;
        return internalX - worldSize * std::floor(internalX / worldSize + 0.5);
    }

    MapTile ElevationManager::clampTileZoom(const MapTile& mapTile) const {
        // Tangram's rule, verbatim (RasterSource::addRasterTask):
        //     subTileID = tileId.zoomBiasAdjusted(zoomDiff).withMaxSourceZoom(maxZoom);
        // the elevation tile is the render tile's OWN z/x/y, adjusted by the elevation source's
        // ZOOM BIAS - one level per doubling of its tile size, because a 512-texel tile at z-1
        // has the same texel density as a 256-texel tile at z - and capped by the source's own
        // maximum zoom. Nothing else: no cap against what the surface mesh can express, and no
        // detail dial on top of it. Those were this fork's, and they are what made the hillshade
        // blurry - the mesh resolution decides how finely the GROUND is tesselated, not how much
        // relief the per-fragment shading may resolve.
        // NOTE: this maps a RENDER tile to its elevation tile and is deliberately NOT idempotent -
        // it drops the bias on every call. Applying it to an elevation tile again (getTileGrid on a
        // getDataTile result, or on a neighbour of one) costs another level each hop, which is why
        // the elevation-tile entry points use clampDataTileZoom instead.
        MapTile tile = mapTile;
        for (int size = _gridSizeHint.load(); size > DEM_TEXELS_PER_TILE_UNIT && tile.getZoom() > 0; size /= 2) {
            tile = tile.getParent();
        }
        return clampDataTileZoom(tile);
    }

    MapTile ElevationManager::clampDataTileZoom(const MapTile& dataTile) const {
        // Only the data source zoom range: idempotent, safe to apply to an elevation tile.
        MapTile tile = dataTile;
        int maxZoom = _dataSource->getMaxZoom();
        while (tile.getZoom() > maxZoom) {
            tile = tile.getParent();
        }
        return tile;
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::getGridForInternalPos(double internalX, double internalY, LoadMode mode) const {
        // Dense point queries - a label re-anchor samples every vertex of every label - walk the
        // same grid thousands of times in a row, and finding WHICH tile a point belongs to costs
        // a projection transform, a tile id, a flip and the zoom clamp before the cache lookup
        // (and its own memo) is even reached. Measured with labels over 3D terrain, that tile
        // math was 70% of the render thread. A grid is immutable and every elevation change bumps
        // the version, so the last grid whose bounds contain the point is the same answer the
        // resolution below would produce - the raycast in intersectRay keeps its grid for exactly
        // this reason. LOAD_EXACT is excluded: it must not be satisfied by an ancestor stand-in.
        struct PosMemo {
            unsigned long long instanceId = 0;
            unsigned int version = 0;
            LoadMode mode = LoadMode::CACHED_ONLY;
            std::shared_ptr<ElevationTileGrid> grid;
        };
        static thread_local PosMemo memo;
        unsigned int memoVersion = _version.load();
        bool memoizable = (mode != LoadMode::LOAD_EXACT);
        if (memoizable && memo.instanceId == _instanceId && memo.version == memoVersion && memo.mode == mode && memo.grid) {
            if (memo.grid->getInternalBounds().contains(MapPos(internalX, internalY, 0))) {
                return memo.grid;
            }
        }

        MapPos dataSourcePos = _projection->fromInternal(MapPos(internalX, internalY, 0));
        MapTile mapTile = TileUtils::CalculateClippedMapTile(dataSourcePos, _dataSource->getMaxZoom(), _projection).getFlipped();
        std::shared_ptr<ElevationTileGrid> grid = getTileGrid(mapTile, mode);
        if (memoizable && grid) {
            memo = PosMemo { _instanceId, memoVersion, mode, grid };
        }
        return grid;
    }

    std::shared_ptr<ElevationTileGrid> ElevationManager::loadTileGrid(const MapTile& requestedTile) const {
        // The tile is in XYZ convention (y=0 north), which is what TileDataSource::loadTile expects.
        // TileUtils works in TMS convention (y=0 south), hence the getFlipped() for bounds math.
        MapTile mapTile = requestedTile;
        std::shared_ptr<TileData> tileData = _dataSource->loadTile(mapTile);
        while (tileData && tileData->isReplaceWithParent() && mapTile.getZoom() > 0) {
            mapTile = mapTile.getParent();
            tileData = _dataSource->loadTile(mapTile);
        }
        if (!tileData || !tileData->getData()) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        std::shared_ptr<Bitmap> tileBitmap = Bitmap::CreateFromCompressed(tileData->getData());
        if (!tileBitmap) {
            Log::Error("ElevationManager::loadTileGrid: Failed to decode elevation tile bitmap");
            return std::shared_ptr<ElevationTileGrid>();
        }

        MapBounds bounds = TileUtils::CalculateMapTileBounds(mapTile.getFlipped(), _projection);
        MapPos internalMin = _projection->toInternal(bounds.getMin());
        MapPos internalMax = _projection->toInternal(bounds.getMax());
        MapBounds internalBounds(MapPos(std::min(internalMin.getX(), internalMax.getX()), std::min(internalMin.getY(), internalMax.getY())),
                                 MapPos(std::max(internalMin.getX(), internalMax.getX()), std::max(internalMin.getY(), internalMax.getY())));

        std::array<double, 4> coeffs = _elevationDecoder->getColorComponentCoefficients();
        std::shared_ptr<ElevationTileGrid> grid = ElevationTileGrid::DecodeBitmap(mapTile, internalBounds, tileBitmap, coeffs);
        if (grid && grid->getWidth() > 0) {
            _gridSizeHint.store(grid->getWidth()); // drives the elevation level cap in clampTileZoom
        }
        return grid;
    }
}
