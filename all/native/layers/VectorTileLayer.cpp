#include "VectorTileLayer.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "components/CancelableThreadPool.h"
#include "geometry/VectorTileFeature.h"
#include "graphics/utils/BackgroundBitmapGenerator.h"
#include "graphics/utils/SkyBitmapGenerator.h"
#include "datasources/TileDataSource.h"
#include "layers/VectorTileEventListener.h"
#include "projections/Projection.h"
#include "projections/ProjectionSurface.h"
#include "renderers/MapRenderer.h"
#include "renderers/TileRenderer.h"
#include "renderers/components/RayIntersectedElement.h"
#include "renderers/drawdatas/TileDrawData.h"
#include "ui/VectorTileClickInfo.h"
#include "utils/Log.h"
#include "utils/Const.h"
#include "vectortiles/VectorTileDecoder.h"

#include <vt/TileId.h>
#include <vt/Tile.h>
#include <vt/TileTransformer.h>

namespace carto {

    VectorTileLayer::VectorTileLayer(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<VectorTileDecoder>& decoder) :
        TileLayer(dataSource),
        _useTileMapMode(false),
        _vectorTileEventListener(),
        _labelRenderOrder(VectorTileRenderOrder::VECTOR_TILE_RENDER_ORDER_LAYER),
        _buildingRenderOrder(VectorTileRenderOrder::VECTOR_TILE_RENDER_ORDER_LAST),
        _tileDecoder(decoder),
        _tileDecoderListener(),
        _backgroundColor(0, 0, 0, 0),
        _backgroundBitmap(),
        _skyColor(0, 0, 0, 0),
        _skyGroundColor(0, 0, 0, 0),
        _skyBitmap(),
        _poleTiles(),
        _labelCullThreadPool(std::make_shared<CancelableThreadPool>()),
        _visibleTileIds(),
        _tempDrawDatas(),
        _visibleCache(DEFAULT_VISIBLE_CACHE_SIZE),
        _preloadingCache(DEFAULT_PRELOADING_CACHE_SIZE)
    {
        if (!decoder) {
            throw NullArgumentException("Null decoder");
        }

        _labelCullThreadPool->setPoolSize(1);

        setCullDelay(DEFAULT_CULL_DELAY);
    }
    
    VectorTileLayer::~VectorTileLayer() {
        _labelCullThreadPool->cancelAll();
        _labelCullThreadPool->deinit();
    }
    
    std::shared_ptr<VectorTileDecoder> VectorTileLayer::getTileDecoder() const {
        return _tileDecoder;
    }
    
    std::size_t VectorTileLayer::getTileCacheCapacity() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _preloadingCache.capacity();
    }
    
    void VectorTileLayer::setTileCacheCapacity(std::size_t capacityInBytes) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        _preloadingCache.resize(capacityInBytes);
    }
    
    VectorTileRenderOrder::VectorTileRenderOrder VectorTileLayer::getLabelRenderOrder() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _labelRenderOrder;
    }
    
    void VectorTileLayer::setLabelRenderOrder(VectorTileRenderOrder::VectorTileRenderOrder renderOrder) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _labelRenderOrder = renderOrder;
        }
        redraw();
    }
    
    VectorTileRenderOrder::VectorTileRenderOrder VectorTileLayer::getBuildingRenderOrder() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _buildingRenderOrder;
    }
    
    void VectorTileLayer::setBuildingRenderOrder(VectorTileRenderOrder::VectorTileRenderOrder renderOrder) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _buildingRenderOrder = renderOrder;
        }
        redraw();
    }
    
    std::shared_ptr<VectorTileEventListener> VectorTileLayer::getVectorTileEventListener() const {
        return _vectorTileEventListener.get();
    }
    
    void VectorTileLayer::setVectorTileEventListener(const std::shared_ptr<VectorTileEventListener>& eventListener) {
        _vectorTileEventListener.set(eventListener);
        tilesChanged(false); // we must reload the tiles, we do not keep full element information if this is not required
    }
    
    bool VectorTileLayer::tileExists(const MapTile& tile, bool preloadingCache) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        long long tileId = getTileId(tile);
        if (preloadingCache) {
            return _preloadingCache.exists(tileId);
        } else {
            return _visibleCache.exists(tileId);
        }
    }
    
    bool VectorTileLayer::tileValid(const MapTile& tile, bool preloadingCache) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        long long tileId = getTileId(tile);
        if (preloadingCache) {
            return _preloadingCache.exists(tileId) && _preloadingCache.valid(tileId);
        } else {
            return _visibleCache.exists(tileId) && _visibleCache.valid(tileId);
        }
    }
    
    void VectorTileLayer::fetchTile(const MapTile& tile, bool preloadingTile, bool invalidated) {
        long long tileId = getTileId(tile);
        if (_fetchingTiles.exists(tileId)) {
            return;
        }

        if (!invalidated) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            if (_preloadingCache.exists(tileId) && _preloadingCache.valid(tileId)) {
                if (!preloadingTile) {
                    _preloadingCache.move(tileId, _visibleCache); // move to visible cache, just in case the element gets trashed
                } else {
                    _preloadingCache.get(tileId);
                }
                return;
            }

            if (_visibleCache.exists(tileId) && _visibleCache.valid(tileId)) {
                _visibleCache.get(tileId); // do not move to preloading, it will be moved at later stage
                return;
            }
        }
        
        auto task = std::make_shared<FetchTask>(std::static_pointer_cast<VectorTileLayer>(shared_from_this()), MapTile(tile.getX(), tile.getY(), tile.getZoom(), 0), preloadingTile);
        _fetchingTiles.add(tileId, task);
        
        std::shared_ptr<CancelableThreadPool> tileThreadPool;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            tileThreadPool = _tileThreadPool;
        }
        if (tileThreadPool) {
            tileThreadPool->execute(task, preloadingTile ? getUpdatePriority() + PRELOADING_PRIORITY_OFFSET : getUpdatePriority());
        }
    }

    void VectorTileLayer::clearTiles(bool preloadingTiles) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        if (preloadingTiles) {
            _preloadingCache.clear();
        } else {
            _visibleCache.clear();
        }
    }

    void VectorTileLayer::tilesChanged(bool removeTiles) {
        // Invalidate current tasks
        for (const std::shared_ptr<FetchTaskBase>& task : _fetchingTiles.getTasks()) {
            task->invalidate();
        }

        // Flush caches
        if (removeTiles) {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _visibleCache.clear();
            _preloadingCache.clear();
        } else {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _visibleCache.invalidate_all(std::chrono::steady_clock::now());
            _preloadingCache.clear();
        }
        refresh();
    }

    long long VectorTileLayer::getTileId(const MapTile& mapTile) const {
        if (_useTileMapMode) {
            return MapTile(mapTile.getX(), mapTile.getY(), mapTile.getZoom(), 0).getTileId();
        } else {
            return mapTile.getTileId();
        }
    }

    std::shared_ptr<VectorTileDecoder::TileMap> VectorTileLayer::getTileMap(long long tileId) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        TileInfo tileInfo;
        if (_visibleCache.peek(tileId, tileInfo)) {
            return tileInfo.getTileMap();
        }
        if (_preloadingCache.peek(tileId, tileInfo)) {
            return tileInfo.getTileMap();
        }
        return std::shared_ptr<VectorTileDecoder::TileMap>();
    }

    std::shared_ptr<vt::Tile> VectorTileLayer::getPoleTile(int y) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        vt::Color color = (y < 0 ? _tileDecoder->getMapSettings()->northPoleColor : _tileDecoder->getMapSettings()->southPoleColor);
        std::shared_ptr<vt::Tile>& tile = _poleTiles[y < 0 ? 0 : 1];
        if (!tile || tile->getBackground()->getColor() != color) {
            float tileSize = 256.0f; // 'normalized' tile size in pixels. Not really important
            vt::TileId vtTile(0, 0, y);
            auto tileBackground = std::make_shared<vt::TileBackground>(color, std::shared_ptr<const vt::BitmapPattern>());
            tile = std::make_shared<vt::Tile>(vtTile, tileSize, tileBackground, std::vector<std::shared_ptr<vt::TileLayer> >());
        }
        return tile;
    }
    
    void VectorTileLayer::calculateDrawData(const MapTile& visTile, const MapTile& closestTile, bool preloadingTile) {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        long long closestTileId = getTileId(closestTile);
        TileInfo tileInfo;
        _visibleCache.read(closestTileId, tileInfo);
        if (!tileInfo.getTileMap()) {
            _preloadingCache.read(closestTileId, tileInfo);
        }
        if (std::shared_ptr<VectorTileDecoder::TileMap> tileMap = tileInfo.getTileMap()) {
            auto it = tileMap->find(_useTileMapMode ? closestTile.getFrameNr() : 0);
            if (it != tileMap->end()) {
                std::shared_ptr<const vt::Tile> vtTile = it->second;
                vt::TileId vtTileId(visTile.getZoom(), visTile.getX(), visTile.getY());
                if (closestTile.getZoom() > visTile.getZoom()) {
                    int dx = visTile.getX() >> visTile.getZoom();
                    int dy = visTile.getY() >> visTile.getZoom();
                    vtTileId = vt::TileId(closestTile.getZoom(), closestTile.getX() + (dx << closestTile.getZoom()), closestTile.getY() + (dy << closestTile.getZoom()));
                }
                _tempDrawDatas.push_back(std::make_shared<TileDrawData>(vtTileId, vtTile, closestTileId, preloadingTile));
            }
        }
    }
    
    void VectorTileLayer::refreshDrawData(const std::shared_ptr<CullState>& cullState) {
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
        bool cull = false;
        if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
            if (!(_synchronizedRefresh && _fetchingTiles.getVisibleCount() > 0)) {
                std::vector<std::shared_ptr<TileDrawData>> drawDatas = _tempDrawDatas;

                // Add poles
                if (auto options = _options.lock()) {
                    if (options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_SPHERICAL) {
                        const cglib::frustum3<double>& frustum = cullState->getViewState().getFrustum();
                        for (int y = -1; y <= 1; y += 2) {
                            vt::TileId vtTile(0, 0, y);
                            cglib::bbox3<double> bbox = getTileTransformer()->calculateTileBBox(vtTile);
                            if (frustum.inside(bbox)) {
                                drawDatas.push_back(std::make_shared<TileDrawData>(vtTile, getPoleTile(y), -1, false));
                            }
                        }
                    }
                }
                
                if (tileRenderer->refreshTiles(drawDatas)) {
                    refresh = true;
                    cull = true;
                }
            }
        }
    
        if (!_lastCullState || cullState->getViewState().getModelviewProjectionMat() != _lastCullState->getViewState().getModelviewProjectionMat()) {
            cull = true;
        }
    
        if (cull) {
            _labelCullThreadPool->cancelAll();
            std::shared_ptr<CancelableTask> task = std::make_shared<LabelCullTask>(std::static_pointer_cast<VectorTileLayer>(shared_from_this()), getTileRenderer(), cullState->getViewState());
            _labelCullThreadPool->execute(task);
        }
    
        if (refresh) {
            redraw();
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
    
    int VectorTileLayer::getMinZoom() const {
        return std::max(_dataSource->getMinZoom(), _tileDecoder->getMinZoom());
    }
    
    int VectorTileLayer::getMaxZoom() const {
        return _tileDecoder->getMaxZoom(); // NOTE: datasource max zoom is handled differently
    }    
    
    std::vector<long long> VectorTileLayer::getVisibleTileIds() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _visibleTileIds;
    }

    void VectorTileLayer::calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<RayIntersectedElement>& results) const {
        DirectorPtr<VectorTileEventListener> eventListener = _vectorTileEventListener;

        if (eventListener) {
            for (int pass = 0; pass < 2; pass++) {
                std::vector<std::tuple<vt::TileId, double, long long> > hitResults;
                if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
                    if (pass == 0) {
                        tileRenderer->calculateRayIntersectedElements(ray, viewState, hitResults);
                    } else {
                        tileRenderer->calculateRayIntersectedElements3D(ray, viewState, hitResults);
                    }
                }

                for (auto it = hitResults.rbegin(); it != hitResults.rend(); it++) {
                    vt::TileId vtTileId = std::get<0>(*it);
                    double t = std::get<1>(*it);
                    long long id = std::get<2>(*it);

                    std::lock_guard<std::recursive_mutex> lock(_mutex);

                    TileInfo tileInfo;
                    _visibleCache.peek(getTileId(MapTile(vtTileId.x, vtTileId.y, vtTileId.zoom, _frameNr)), tileInfo);

                    if (std::shared_ptr<BinaryData> tileData = tileInfo.getTileData()) {
                        if (std::shared_ptr<VectorTileFeature> tileFeature = _tileDecoder->decodeFeature(id, vtTileId, tileData, tileInfo.getTileBounds())) {
                            std::shared_ptr<Layer> thisLayer = std::const_pointer_cast<Layer>(shared_from_this());
                            results.push_back(RayIntersectedElement(tileFeature, thisLayer, ray(t), ray(t), pass > 0));
                        } else {
                            Log::Warnf("VectorTileLayer::calculateRayIntersectedElements: Failed to decode feature %lld", id);
                        }
                    } else {
                        Log::Warn("VectorTileLayer::calculateRayIntersectedElements: Failed to find tile data");
                    }
                }
            }
        }

        TileLayer::calculateRayIntersectedElements(ray, viewState, results);
    }

    bool VectorTileLayer::processClick(ClickType::ClickType clickType, const RayIntersectedElement& intersectedElement, const ViewState& viewState) const {
        std::shared_ptr<ProjectionSurface> projectionSurface = viewState.getProjectionSurface();
        
        DirectorPtr<VectorTileEventListener> eventListener = _vectorTileEventListener;

        if (eventListener) {
            if (auto tileFeature = intersectedElement.getElement<VectorTileFeature>()) {
                MapPos hitPos = _dataSource->getProjection()->fromInternal(projectionSurface->calculateMapPos(intersectedElement.getHitPos()));
                auto clickInfo = std::make_shared<VectorTileClickInfo>(clickType, hitPos, hitPos, tileFeature, intersectedElement.getLayer());
                return eventListener->onVectorTileClicked(clickInfo);
            }
        }

        return TileLayer::processClick(clickType, intersectedElement, viewState);
    }

    void VectorTileLayer::offsetLayerHorizontally(double offset) {
        if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
            tileRenderer->offsetLayerHorizontally(offset);
        }
    }
    
    void VectorTileLayer::onSurfaceCreated(const std::shared_ptr<ShaderManager>& shaderManager, const std::shared_ptr<TextureManager>& textureManager) {
        Layer::onSurfaceCreated(shaderManager, textureManager);

        // Reset renderer    
        if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
            tileRenderer->onSurfaceDestroyed();
            setTileRenderer(std::shared_ptr<TileRenderer>());
    
            // Clear all tile caches - renderer may cache/release tile info, so old tiles are potentially unusable at this point
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _preloadingCache.clear();
            _visibleCache.clear();
        }

        // Create new rendererer, simply drop old one (if exists)
        resetTileTransformer();
        auto tileRenderer = std::make_shared<TileRenderer>(_mapRenderer, getTileTransformer());
        tileRenderer->onSurfaceCreated(shaderManager, textureManager);
        setTileRenderer(tileRenderer);
    }
    
    bool VectorTileLayer::onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, StyleTextureCache& styleCache, const ViewState& viewState) {
        updateTileLoadListener();

        if (std::shared_ptr<MapRenderer> mapRenderer = _mapRenderer.lock()) {
            if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
                float opacity = getOpacity();

                if (opacity < 1.0f) {
                    mapRenderer->clearAndBindScreenFBO(Color(0, 0, 0, 0), true, true);
                }

                tileRenderer->setLabelOrder(static_cast<int>(getLabelRenderOrder()));
                tileRenderer->setBuildingOrder(static_cast<int>(getBuildingRenderOrder()));
                tileRenderer->setInteractionMode(_vectorTileEventListener.get() ? true : false);
                tileRenderer->setSubTileBlending(false);
                bool refresh = tileRenderer->onDrawFrame(deltaSeconds, viewState);

                if (opacity < 1.0f) {
                    mapRenderer->blendAndUnbindScreenFBO(opacity);
                }

                return refresh;
            }
        }
        return false;
    }
        
    bool VectorTileLayer::onDrawFrame3D(float deltaSeconds, BillboardSorter& billboardSorter, StyleTextureCache& styleCache, const ViewState& viewState) {
        if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
            return tileRenderer->onDrawFrame3D(deltaSeconds, viewState);
        }
        return false;
    }
    
    void VectorTileLayer::onSurfaceDestroyed() {
        // Reset renderer
        if (std::shared_ptr<TileRenderer> tileRenderer = getTileRenderer()) {
            tileRenderer->onSurfaceDestroyed();
            setTileRenderer(std::shared_ptr<TileRenderer>());
        }

        // Clear all tile caches - renderer may cache/release tile info, so old tiles are potentially unusable at this point
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _preloadingCache.clear();
            _visibleCache.clear();
        }
    
        Layer::onSurfaceDestroyed();
    }
    
    std::shared_ptr<Bitmap> VectorTileLayer::getBackgroundBitmap() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        Color backgroundColor = _backgroundColor;
        if (std::shared_ptr<mvt::Map::Settings> mapSettings = _tileDecoder->getMapSettings()) {
            backgroundColor = Color(mapSettings->backgroundColor.value());
        }
        if (backgroundColor != _backgroundColor || !_backgroundBitmap) {
            if (backgroundColor != Color(0, 0, 0, 0)) {
                _backgroundBitmap = BackgroundBitmapGenerator(BACKGROUND_BLOCK_SIZE, BACKGROUND_BLOCK_COUNT).generateBitmap(backgroundColor);
            } else {
                _backgroundBitmap.reset();
            }
            _backgroundColor = backgroundColor;
        }
        return _backgroundBitmap;
    }

    std::shared_ptr<Bitmap> VectorTileLayer::getSkyBitmap() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);

        std::shared_ptr<Options> options = _options.lock();
        if (!options) {
            return std::shared_ptr<Bitmap>();
        }
        Color skyGroundColor = _skyGroundColor;
        if (std::shared_ptr<mvt::Map::Settings> mapSettings = _tileDecoder->getMapSettings()) {
            skyGroundColor = Color(mapSettings->backgroundColor.value());
        }
        Color skyColor = options->getSkyColor();
        if (skyGroundColor != _skyGroundColor || skyColor != _skyColor || !_skyBitmap) {
            if (skyColor == Color(0, 0, 0, 0)) {
                _skyBitmap.reset();
            } else {
                _skyBitmap = SkyBitmapGenerator(1, 128).generateBitmap(skyGroundColor, skyColor);
            }
            _skyGroundColor = skyGroundColor;
            _skyColor = skyColor;
        }
        return _skyBitmap;
    }

    void VectorTileLayer::registerDataSourceListener() {
        _tileDecoderListener = std::make_shared<TileDecoderListener>(std::static_pointer_cast<VectorTileLayer>(shared_from_this()));
        _tileDecoder->registerOnChangeListener(_tileDecoderListener);
    
        _dataSourceListener = std::make_shared<DataSourceListener>(std::static_pointer_cast<VectorTileLayer>(shared_from_this()));
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }
    
    void VectorTileLayer::unregisterDataSourceListener() {
        _dataSource->unregisterOnChangeListener(_dataSourceListener);
        _dataSourceListener.reset();

        _tileDecoder->unregisterOnChangeListener(_tileDecoderListener);
        _tileDecoderListener.reset();
    }
    
    VectorTileLayer::TileDecoderListener::TileDecoderListener(const std::shared_ptr<VectorTileLayer>& layer) :
        _layer(layer)
    {
    }
        
    void VectorTileLayer::TileDecoderListener::onDecoderChanged() {
        if (std::shared_ptr<VectorTileLayer> layer = _layer.lock()) {
            layer->tilesChanged(false);
        } else {
            Log::Error("VectorTileLayer::TileDecoderListener: Lost connection to layer");
        }
    }
    
    VectorTileLayer::FetchTask::FetchTask(const std::shared_ptr<VectorTileLayer>& layer, const MapTile& tile, bool preloadingTile) :
        FetchTaskBase(layer, tile, preloadingTile)
    {
    }
    
    bool VectorTileLayer::FetchTask::loadTile(const std::shared_ptr<TileLayer>& tileLayer) {
        auto layer = std::static_pointer_cast<VectorTileLayer>(tileLayer);
        
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
    
            vt::TileId vtTile(_tile.getZoom(), _tile.getX(), _tile.getY());
            vt::TileId vtDataSourceTile(dataSourceTile.getZoom(), dataSourceTile.getX(), dataSourceTile.getY());
            std::shared_ptr<vt::TileTransformer> tileTransformer = layer->getTileTransformer();
            std::shared_ptr<VectorTileDecoder::TileMap> tileMap = layer->_tileDecoder->decodeTile(vtDataSourceTile, vtTile, tileTransformer, tileData->getData());
            if (tileMap) {
                // Construct tile info - keep original data if interactivity is required
                VectorTileLayer::TileInfo tileInfo(layer->calculateMapTileBounds(dataSourceTile.getFlipped()), layer->_vectorTileEventListener.get() ? tileData->getData() : std::shared_ptr<BinaryData>(), tileMap);

                // Store tile to cache, unless invalidated
                if (!isInvalidated()) {
                    long long tileId = layer->getTileId(_tile);
                    std::lock_guard<std::recursive_mutex> lock(layer->_mutex);
                    if (layer->getTileTransformer() == tileTransformer) { // extra check that the tile is created with correct transformer. Otherwise simply drop it.
                        if (isPreloading()) {
                            layer->_preloadingCache.put(tileId, tileInfo, tileInfo.getSize());
                            if (tileData->getMaxAge() >= 0) {
                                layer->_preloadingCache.invalidate(tileId, std::chrono::steady_clock::now() + std::chrono::milliseconds(tileData->getMaxAge()));
                            }
                        } else {
                            layer->_visibleCache.put(tileId, tileInfo, tileInfo.getSize());
                            if (tileData->getMaxAge() >= 0) {
                                layer->_visibleCache.invalidate(tileId, std::chrono::steady_clock::now() + std::chrono::milliseconds(tileData->getMaxAge()));
                            }
                        }
                    }
                }
                
                // Debug tile performance issues
                if (Log::IsShowDebug()) {
                    int maxDrawCallCount = 0;
                    for (auto it = tileMap->begin(); it != tileMap->end(); it++) {
                        int drawCallCount = 0;
                        for (const std::shared_ptr<vt::TileLayer>& vtLayer : it->second->getLayers()) {
                            drawCallCount += static_cast<int>(vtLayer->getBitmaps().size() + vtLayer->getGeometries().size());
                        }
                        maxDrawCallCount = std::max(maxDrawCallCount, drawCallCount);
                    }
                    if (maxDrawCallCount >= 20) {
                        Log::Debugf("VectorTileLayer::FetchTask: Tile requires %d draw calls", maxDrawCallCount);
                    }
                }
                
                refresh = true; // NOTE: need to refresh even when invalidated
            } else if (!tileData->getData()->empty()) {
                Log::Error("VectorTileLayer::FetchTask: Failed to decode tile");
            }
            break;
        }
        
        return refresh;
    }
        
    VectorTileLayer::LabelCullTask::LabelCullTask(const std::shared_ptr<VectorTileLayer>& layer, const std::shared_ptr<TileRenderer>& tileRenderer, const ViewState& viewState) :
        _layer(layer),
        _tileRenderer(tileRenderer),
        _viewState(viewState)
    {
    }
        
    void VectorTileLayer::LabelCullTask::cancel() {
    }
        
    void VectorTileLayer::LabelCullTask::run() {
        std::shared_ptr<VectorTileLayer> layer = _layer.lock();
        if (!layer) {
            return;
        }
    
        if (std::shared_ptr<TileRenderer> tileRenderer = _tileRenderer.lock()) {
            if (tileRenderer->cullLabels(_viewState)) {
                layer->redraw();
            }
        }
    }

    std::size_t VectorTileLayer::TileInfo::getSize() const {
        std::size_t size = EXTRA_TILE_FOOTPRINT;
        if (_tileData) {
            size += _tileData->size();
        }
        for (auto it = _tileMap->begin(); it != _tileMap->end(); it++) {
            size += it->second->getResidentSize();
        }
        return size;
    }
    
}
