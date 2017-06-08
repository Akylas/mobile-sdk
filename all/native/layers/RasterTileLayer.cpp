#include "RasterTileLayer.h"
#include "components/Exceptions.h"
#include "components/CancelableThreadPool.h"
#include "datasources/TileDataSource.h"
#include "layers/RasterTileEventListener.h"
#include "renderers/MapRenderer.h"
#include "renderers/TileRenderer.h"
#include "renderers/components/RayIntersectedElement.h"
#include "renderers/drawdatas/TileDrawData.h"
#include "graphics/Bitmap.h"
#include "ui/RasterTileClickInfo.h"
#include "utils/Log.h"

#include <array>
#include <algorithm>

#include <vt/TileId.h>
#include <vt/Tile.h>
#include <vt/TileBitmap.h>
#include <vt/TileLayer.h>
#include <vt/TileLayerBuilder.h>

namespace {

    std::array<std::uint8_t, 4> readTileBitmapColor(const carto::vt::TileBitmap& bitmap, int x, int y) {
        x = std::max(0, std::min(x, bitmap.getWidth() - 1));
        y = bitmap.getHeight() - 1 - std::max(0, std::min(y, bitmap.getHeight() - 1));

        switch (bitmap.getFormat()) {
        case carto::vt::TileBitmap::Format::GRAYSCALE: {
                std::uint8_t val = bitmap.getData()[y * bitmap.getWidth() + x];
                return std::array<std::uint8_t, 4> { { val, val, val, 255 } };
            }
        case carto::vt::TileBitmap::Format::RGB: {
                const std::uint8_t* valPtr = &bitmap.getData()[(y * bitmap.getWidth() + x) * 3];
                return std::array<std::uint8_t, 4> { { valPtr[0], valPtr[1], valPtr[2], 255 } };
            }
        case carto::vt::TileBitmap::Format::RGBA: {
                const std::uint8_t* valPtr = &bitmap.getData()[(y * bitmap.getWidth() + x) * 4];
                return std::array<std::uint8_t, 4> { { valPtr[0], valPtr[1], valPtr[2], valPtr[3] } };
            }
        }
        return std::array<std::uint8_t, 4> { { 0, 0, 0, 0 } };
    }

    std::array<std::uint8_t, 4> readTileBitmapColor(const carto::vt::TileBitmap& bitmap, float x, float y) {
        std::array<float, 4> result { 0, 0, 0, 0 };
        for (int dy = 0; dy < 2; dy++) {
            for (int dx = 0; dx < 2; dx++) {
                int x0 = static_cast<int>(std::floor(x));
                int y0 = static_cast<int>(std::floor(y));

                std::array<std::uint8_t, 4> color = readTileBitmapColor(bitmap, x0 + dx, y0 + dy);
                for (int i = 0; i < 4; i++) {
                    result[i] += color[i] * (dx == 0 ? x0 + 1.0f - x : x - x0) * (dy == 0 ? y0 + 1.0f - y : y - y0);
                }
            }
        }
        return std::array<std::uint8_t, 4> { { static_cast<std::uint8_t>(result[0]), static_cast<std::uint8_t>(result[1]), static_cast<std::uint8_t>(result[2]), static_cast<std::uint8_t>(result[3]) } };
    }

}

namespace carto {

    RasterTileLayer::RasterTileLayer(const std::shared_ptr<TileDataSource>& dataSource) :
        TileLayer(dataSource),
        _rasterTileEventListener(),
        _visibleTileIds(),
        _tempDrawDatas(),
        _visibleCache(128 * 1024 * 1024), // limit should be never reached during normal use cases
        _preloadingCache(DEFAULT_PRELOADING_CACHE_SIZE)
    {
        setCullDelay(DEFAULT_CULL_DELAY);
    }
    
    RasterTileLayer::~RasterTileLayer() {
    }
    
    std::size_t RasterTileLayer::getTextureCacheCapacity() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _preloadingCache.capacity();
    }
    
    void RasterTileLayer::setTextureCacheCapacity(std::size_t capacityInBytes) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _preloadingCache.resize(capacityInBytes);
    }
    
    std::shared_ptr<RasterTileEventListener> RasterTileLayer::getRasterTileEventListener() const {
        return _rasterTileEventListener.get();
    }
    
    void RasterTileLayer::setRasterTileEventListener(const std::shared_ptr<RasterTileEventListener>& eventListener) {
        _rasterTileEventListener.set(eventListener);
        tilesChanged(false); // we must reload the tiles, we do not keep full element information if this is not required
    }
    
    bool RasterTileLayer::tileExists(const MapTile& tile, bool preloadingCache) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        long long tileId = tile.getTileId();
        if (preloadingCache) {
            return _preloadingCache.exists(tileId);
        }
        else {
            return _visibleCache.exists(tileId);
        }
    }
    
    bool RasterTileLayer::tileValid(const MapTile& tile, bool preloadingCache) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        long long tileId = tile.getTileId();
        if (preloadingCache) {
            return _preloadingCache.exists(tileId) && _preloadingCache.valid(tileId);
        }
        else {
            return _visibleCache.exists(tileId) && _visibleCache.valid(tileId);
        }
    }
    
    void RasterTileLayer::fetchTile(const MapTile& tile, bool preloadingTile, bool invalidated) {
        long long tileId = tile.getTileId();
        if (_fetchingTiles.exists(tile.getTileId())) {
            return;
        }

        if (!invalidated) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            if (_preloadingCache.exists(tileId) && _preloadingCache.valid(tileId)) {
                if (!preloadingTile) {
                    _preloadingCache.move(tileId, _visibleCache); // move to visible cache, just in case the element gets trashed
                }
                else {
                    _preloadingCache.get(tileId);
                }
                return;
            }
    
            if (_visibleCache.exists(tileId) && _visibleCache.valid(tileId)) {
                _visibleCache.get(tileId); // just mark usage, do not move to preloading, it will be moved at later stage
                return;
            }
        }
    
        auto task = std::make_shared<FetchTask>(std::static_pointer_cast<RasterTileLayer>(shared_from_this()), tile, preloadingTile);
        _fetchingTiles.add(tile.getTileId(), task);
        
        std::shared_ptr<CancelableThreadPool> tileThreadPool;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            tileThreadPool = _tileThreadPool;
        }
        if (tileThreadPool) {
            tileThreadPool->execute(task, preloadingTile ? getUpdatePriority() + PRELOADING_PRIORITY_OFFSET : getUpdatePriority());
        }
    }
    
    void RasterTileLayer::clearTiles(bool preloadingTiles) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (preloadingTiles) {
            _preloadingCache.clear();
        }
        else {
            _visibleCache.clear();
        }
    }

    void RasterTileLayer::tilesChanged(bool removeTiles) {
        // Invalidate current tasks
        for (const std::shared_ptr<FetchTaskBase>& task : _fetchingTiles.getTasks()) {
            task->invalidate();
        }

        // Flush caches
        if (removeTiles) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _visibleCache.clear();
            _preloadingCache.clear();
        }
        else {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _visibleCache.invalidate_all(std::chrono::steady_clock::now());
            _preloadingCache.clear();
        }
        refresh();
    }

    void RasterTileLayer::calculateDrawData(const MapTile& visTile, const MapTile& closestTile, bool preloadingTile) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        long long closestTileId = closestTile.getTileId();
        std::shared_ptr<const vt::Tile> vtTile;
        _visibleCache.read(closestTileId, vtTile);
        if (!vtTile) {
            _preloadingCache.read(closestTileId, vtTile);
        }
        if (vtTile) {
            vt::TileId vtTileId(visTile.getZoom(), visTile.getX(), visTile.getY());
            if (closestTile.getZoom() > visTile.getZoom()) {
                int dx = visTile.getX() >> visTile.getZoom();
                int dy = visTile.getY() >> visTile.getZoom();
                vtTileId = vt::TileId(closestTile.getZoom(), closestTile.getX() + (dx << closestTile.getZoom()), closestTile.getY() + (dy << closestTile.getZoom()));
            }
            _tempDrawDatas.push_back(std::make_shared<TileDrawData>(vtTileId, vtTile, closestTile.getTileId(), preloadingTile));
        }
    }
    
    void RasterTileLayer::refreshDrawData(const std::shared_ptr<CullState>& cullState) {
        // Move tiles between caches
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            // Get all tiles currently in the visible cache
            std::unordered_set<long long> lastVisibleCacheTiles = _visibleCache.keys();
            
            // Remember unused tiles from the visible cache
            for (const std::shared_ptr<TileDrawData>& drawData : _tempDrawDatas) {
                if (!drawData->isPreloadingTile()) {
                    long long tileId = drawData->getTileId();
                    lastVisibleCacheTiles.erase(tileId);

                    if (!_visibleCache.exists(tileId) && _preloadingCache.exists(tileId)) {
                        _preloadingCache.move(tileId, _visibleCache);
                    }
                }
            }
            
            // Move all unused tiles from visible cache to preloading cache
            for (long long tileId : lastVisibleCacheTiles) {
                _visibleCache.move(tileId, _preloadingCache);
            }
        }
        
        // Update renderer if needed, run culler
        bool refresh = false;
        if (auto renderer = getRenderer()) {
            if (!(_synchronizedRefresh && _fetchingTiles.getVisibleCount() > 0)) {
                if (renderer->refreshTiles(_tempDrawDatas)) {
                    refresh = true;
                }
            }
        }
    
        if (refresh) {
            if (std::shared_ptr<MapRenderer> mapRenderer = _mapRenderer.lock()) {
                mapRenderer->requestRedraw();
            }
        }

        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _visibleTileIds.clear();
            for (const std::shared_ptr<TileDrawData>& drawData : _tempDrawDatas) {
                _visibleTileIds.push_back(drawData->getTileId());
            }
            _tempDrawDatas.clear();
        }
    }
    
    int RasterTileLayer::getMinZoom() const {
        return _dataSource->getMinZoom();
    }
    
    int RasterTileLayer::getMaxZoom() const {
        return _dataSource->getMaxZoom();
    }

    std::vector<long long> RasterTileLayer::getVisibleTileIds() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _visibleTileIds;
    }
        
    void RasterTileLayer::calculateRayIntersectedElements(const Projection& projection, const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        DirectorPtr<RasterTileEventListener> eventListener = _rasterTileEventListener;

        if (eventListener) {
            std::vector<std::tuple<vt::TileId, double, vt::TileBitmap, cglib::vec2<float> > > hitResults;
            if (auto renderer = getRenderer()) {
                renderer->calculateRayIntersectedBitmaps(ray, viewState, hitResults);
            }

            for (auto it = hitResults.rbegin(); it != hitResults.rend(); it++) {
                vt::TileId vtTileId = std::get<0>(*it);
                double t = std::get<1>(*it);
                const vt::TileBitmap& tileBitmap = std::get<2>(*it);
                cglib::vec2<float> tilePos = std::get<3>(*it);
                if (tileBitmap.getData().empty() || tileBitmap.getWidth() < 1 || tileBitmap.getHeight() < 1) {
                    Log::Warnf("RasterTileLayer::processClick: Bitmap data not available");
                    continue;
                }

                std::lock_guard<std::recursive_mutex> lock(_mutex);

                float x = tilePos(0) * tileBitmap.getWidth();
                float y = tilePos(1) * tileBitmap.getHeight();
                std::array<std::uint8_t, 4> interpolatedComponents = readTileBitmapColor(tileBitmap, x - 0.5f, y - 0.5f);
                Color interpolatedColor(interpolatedComponents[0], interpolatedComponents[1], interpolatedComponents[2], interpolatedComponents[3]);

                int nx = static_cast<int>(std::floor(x));
                int ny = static_cast<int>(std::floor(y));
                std::array<std::uint8_t, 4> nearestComponents = readTileBitmapColor(tileBitmap, nx, ny);
                Color nearestColor(nearestComponents[0], nearestComponents[1], nearestComponents[2], nearestComponents[3]);

                MapTile mapTile(vtTileId.x, vtTileId.y, vtTileId.zoom, _frameNr);
                MapPos mapPos = projection.fromInternal(MapPos(ray(t)(0), ray(t)(1), ray(t)(2)));

                auto pixelInfo = std::make_shared<std::tuple<MapTile, Color, Color> >(mapTile, nearestColor, interpolatedColor);
                std::shared_ptr<Layer> thisLayer = std::const_pointer_cast<Layer>(shared_from_this());
                results.push_back(RayIntersectedElement(pixelInfo, thisLayer, mapPos, mapPos, 0, false));
            }
        }

        TileLayer::calculateRayIntersectedElements(projection, ray, viewState, results);
    }

    bool RasterTileLayer::processClick(ClickType::ClickType clickType, const RayIntersectedElement& intersectedElement, const ViewState& viewState) const {
        DirectorPtr<RasterTileEventListener> eventListener = _rasterTileEventListener;

        if (eventListener) {
            if (std::shared_ptr<std::tuple<MapTile, Color, Color> > pixelInfo = intersectedElement.getElement<std::tuple<MapTile, Color, Color> >()) {
                const MapTile& mapTile = std::get<0>(*pixelInfo);
                const Color& nearestColor = std::get<1>(*pixelInfo);
                const Color& interpolatedColor = std::get<2>(*pixelInfo);
                auto clickInfo = std::make_shared<RasterTileClickInfo>(clickType, intersectedElement.getHitPos(), mapTile, nearestColor, interpolatedColor, intersectedElement.getLayer());
                return eventListener->onRasterTileClicked(clickInfo);
            }
        }

        return TileLayer::processClick(clickType, intersectedElement, viewState);
    }

    void RasterTileLayer::offsetLayerHorizontally(double offset) {
        if (auto renderer = getRenderer()) {
            renderer->offsetLayerHorizontally(offset);
        }
    }
    
    void RasterTileLayer::onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager) {
        Layer::onSurfaceCreated(shaderManager, textureManager);

        // Reset renderer
        if (auto renderer = getRenderer()) {
            renderer->onSurfaceDestroyed();
            setRenderer(std::shared_ptr<TileRenderer>());
    
            // Clear all tile caches - renderer may cache/release tile info, so old tiles are potentially unusable at this point
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _preloadingCache.clear();
            _visibleCache.clear();
        }
    
        // Create new rendererer, simply drop old one (if exists)
        auto renderer = std::make_shared<TileRenderer>(_mapRenderer, false, false, false);
        renderer->onSurfaceCreated(shaderManager, textureManager);
        setRenderer(renderer);
    }
    
    bool RasterTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, StyleTextureCache& styleCache, const ViewState& viewState) {
        updateTileLoadListener();

        if (auto renderer = getRenderer()) {
            renderer->setInteractionMode(_rasterTileEventListener.get() ? true : false);
            return renderer->onDrawFrame(deltaSeconds, viewState);
        }
        return false;
    }
    
    bool RasterTileLayer::onDrawFrame3D(float deltaSeconds, BillboardSorter& billboardSorter, StyleTextureCache& styleCache, const ViewState& viewState) {
        if (auto renderer = getRenderer()) {
            return renderer->onDrawFrame3D(deltaSeconds, viewState);
        }
        return false;
    }

    void RasterTileLayer::onSurfaceDestroyed() {
        // Reset renderer
        if (auto renderer = getRenderer()) {
            renderer->onSurfaceDestroyed();
            setRenderer(std::shared_ptr<TileRenderer>());
        }
        
        // Clear all tile caches - renderer may cache/release tile info, so old tiles are potentially unusable at this point
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _preloadingCache.clear();
            _visibleCache.clear();
        }
    
        Layer::onSurfaceDestroyed();
    }
    
    void RasterTileLayer::registerDataSourceListener() {
        _dataSourceListener = std::make_shared<DataSourceListener>(std::static_pointer_cast<RasterTileLayer>(shared_from_this()));
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }
    
    void RasterTileLayer::unregisterDataSourceListener() {
        _dataSource->unregisterOnChangeListener(_dataSourceListener);
        _dataSourceListener.reset();
    }
    
    RasterTileLayer::FetchTask::FetchTask(const std::shared_ptr<RasterTileLayer>& layer, const MapTile& tile, bool preloadingTile) :
        FetchTaskBase(layer, tile, preloadingTile)
    {
    }
    
    bool RasterTileLayer::FetchTask::loadTile(const std::shared_ptr<TileLayer>& tileLayer) {
        auto layer = std::static_pointer_cast<RasterTileLayer>(tileLayer);
    
        bool refresh = false;
        for (const MapTile& dataSourceTile : _dataSourceTiles) {
            std::shared_ptr<TileData> tileData = layer->_dataSource->loadTile(dataSourceTile);
            if (!tileData) {
                break;
            }
            if (tileData->isReplaceWithParent()) {
                continue;
            }
            if (!tileData->getData()) {
                break;
            }
    
            // Save tile to texture cache, unless invalidated
            vt::TileId vtTile(_tile.getZoom(), _tile.getX(), _tile.getY());
            vt::TileId vtDataSourceTile(dataSourceTile.getZoom(), dataSourceTile.getX(), dataSourceTile.getY());
            std::shared_ptr<Bitmap> bitmap = Bitmap::CreateFromCompressed(tileData->getData());
            if (bitmap) {
                // Check if we received the requested tile or extract/scale the corresponding part
                if (dataSourceTile != _tile) {
                    bitmap = ExtractSubTile(_tile, dataSourceTile, bitmap);
                }

                if (!isInvalidated()) {
                    // Build the bitmap object
                    std::shared_ptr<vt::Tile> vtTile = CreateVectorTile(_tile, bitmap);
                    std::size_t tileSize = EXTRA_TILE_FOOTPRINT + vtTile->getResidentSize();
                    if (isPreloading()) {
                        std::lock_guard<std::recursive_mutex> lock(layer->_mutex);
                        layer->_preloadingCache.put(_tile.getTileId(), vtTile, tileSize);
                        if (tileData->getMaxAge() >= 0) {
                            layer->_preloadingCache.invalidate(_tile.getTileId(), std::chrono::steady_clock::now() + std::chrono::milliseconds(tileData->getMaxAge()));
                        }
                    } else {
                        std::lock_guard<std::recursive_mutex> lock(layer->_mutex);
                        layer->_visibleCache.put(_tile.getTileId(), vtTile, tileSize);
                        if (tileData->getMaxAge() >= 0) {
                            layer->_visibleCache.invalidate(_tile.getTileId(), std::chrono::steady_clock::now() + std::chrono::milliseconds(tileData->getMaxAge()));
                        }
                    }
                }
                refresh = true; // NOTE: need to refresh even when invalidated
            } else {
                Log::Error("RasterTileLayer::FetchTask: Failed to decode tile");
            }
            break;
        }
        
        return refresh;
    }
    
    std::shared_ptr<Bitmap> RasterTileLayer::FetchTask::ExtractSubTile(const MapTile& subTile, const MapTile& tile, const std::shared_ptr<Bitmap>& bitmap) {
        int deltaZoom = subTile.getZoom() - tile.getZoom();
        int x = (bitmap->getWidth()  * (subTile.getX() & ((1 << deltaZoom) - 1))) >> deltaZoom;
        int y = (bitmap->getHeight() * (subTile.getY() & ((1 << deltaZoom) - 1))) >> deltaZoom;
        int w = bitmap->getWidth()  >> deltaZoom;
        int h = bitmap->getHeight() >> deltaZoom;
        std::shared_ptr<Bitmap> subBitmap = bitmap->getSubBitmap(x, y, std::max(w, 1), std::max(h, 1));
        return subBitmap->getResizedBitmap(bitmap->getWidth(), bitmap->getHeight());
    }

    std::shared_ptr<vt::Tile> RasterTileLayer::FetchTask::CreateVectorTile(const MapTile& tile, const std::shared_ptr<Bitmap>& bitmap) {
        std::shared_ptr<vt::TileBitmap> tileBitmap;
        switch (bitmap->getColorFormat()) {
        case ColorFormat::COLOR_FORMAT_GRAYSCALE:
            tileBitmap = std::make_shared<vt::TileBitmap>(vt::TileBitmap::Format::GRAYSCALE, bitmap->getWidth(), bitmap->getHeight(), bitmap->getPixelData());
            break;
        case ColorFormat::COLOR_FORMAT_RGB:
            tileBitmap = std::make_shared<vt::TileBitmap>(vt::TileBitmap::Format::RGB, bitmap->getWidth(), bitmap->getHeight(), bitmap->getPixelData());
            break;
        case ColorFormat::COLOR_FORMAT_RGBA:
            tileBitmap = std::make_shared<vt::TileBitmap>(vt::TileBitmap::Format::RGBA, bitmap->getWidth(), bitmap->getHeight(), bitmap->getPixelData());
            break;
        default:
            tileBitmap = std::make_shared<vt::TileBitmap>(vt::TileBitmap::Format::RGBA, bitmap->getWidth(), bitmap->getHeight(), bitmap->getRGBABitmap()->getPixelData());
            break;
        }

        vt::TileId vtTile(tile.getZoom(), tile.getX(), tile.getY());
        vt::TileLayerBuilder tileLayerBuilder(vtTile, 256.0f, 1.0f); // Note: the size/scale argument is ignored
        tileLayerBuilder.addBitmap(tileBitmap);
        std::shared_ptr<vt::TileLayer> tileLayer = tileLayerBuilder.build(0, std::shared_ptr<vt::FloatFunction>(), boost::optional<vt::CompOp>());

        return std::make_shared<vt::Tile>(vtTile, std::vector<std::shared_ptr<vt::TileLayer> > { tileLayer });
    }

}
