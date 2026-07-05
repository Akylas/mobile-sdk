#include "TileRenderer.h"
#include "components/Options.h"
#include "components/ThreadWorker.h"
#include "graphics/ViewState.h"
#include "projections/ProjectionSurface.h"
#include "projections/PlanarProjectionSurface.h"
#include "renderers/MapRenderer.h"
#include "renderers/drawdatas/TileDrawData.h"
#include "renderers/TerrainRenderer.h"
#include "renderers/utils/ElevationTextureCache.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/VTRenderer.h"
#include "layers/HillshadeRasterTileLayer.h"
#include "terrain/ElevationManager.h"
#include "utils/Const.h"
#include "utils/Log.h"
#include "utils/Const.h"

#include <vt/Label.h>
#include <vt/LabelCuller.h>
#include <vt/TileTransformer.h>
#include <vt/GLExtensions.h>

#include <cmath>
#include <unordered_map>

#include <cglib/mat.h>

namespace carto {

    struct TileRenderer::LabelOcclusionState {
        std::mutex mutex;
        cglib::vec3<double> cameraPos = cglib::vec3<double>(0, 0, 0);
        unsigned int elevationVersion = 0;
        std::unordered_map<long long, bool> results;
    };

    TileRenderer::TileRenderer() :
        _mapRenderer(),
        _options(),
        _tileTransformer(),
        _vtRenderer(),
        _interactionMode(false),
        _layerBlendingSpeed(1.0f),
        _labelBlendingSpeed(1.0f),
        _labelOrder(0),
        _buildingOrder(1),
        _rasterFilterMode(vt::RasterFilterMode::BILINEAR),
        _normalMapLightingShader(LIGHTING_SHADER_NORMALMAP),
        _normalMapShadowColor(0, 0, 0, 255),
        _normalMapAccentColor(0, 0, 0, 255),
        _normalMapHighlightColor(255, 255, 255, 255),
        _rendererLayerFilter(),
        _clickHandlerLayerFilter(),
        _horizontalLayerOffset(0),
        _viewDir(0, 0, 0),
        _mainLightDir(0, 0, 0),
        _normalLightDir(0, 0, 0),
        _normalIlluminationMapRotationEnabled(false),
        _normalIlluminationDirection(0,0,0),
        _mapRotation(0),
        _hillshadeMethod(HillshadeMethod::STANDARD),
        _hillshadeExaggeration(0.5f),
        _tiles(),
        _mutex()
    {
    }
    
    TileRenderer::~TileRenderer() {
    }
    
    void TileRenderer::setComponents(const std::weak_ptr<Options>& options, const std::weak_ptr<MapRenderer>& mapRenderer) {
        std::lock_guard<std::mutex> lock(_mutex);
        _options = options;
        _mapRenderer = mapRenderer;
        _vtRenderer.reset();
    }

    std::shared_ptr<vt::TileTransformer> TileRenderer::getTileTransformer() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _tileTransformer;
    }

    void TileRenderer::setTileTransformer(const std::shared_ptr<vt::TileTransformer>& tileTransformer) {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_tileTransformer != tileTransformer) {
            _vtRenderer.reset();
        }
        _tileTransformer = tileTransformer;
    }
    
    void TileRenderer::setInteractionMode(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _interactionMode = enabled;
    }

    void TileRenderer::setTerrainRenderOrder(int order) {
        std::lock_guard<std::mutex> lock(_mutex);
        _terrainRenderOrder = order;
    }

    void TileRenderer::setTerrainDepthWriteMode(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _terrainDepthWriteMode = enabled;
    }
    
    void TileRenderer::setLayerBlendingSpeed(float speed) {
        std::lock_guard<std::mutex> lock(_mutex);
        _layerBlendingSpeed = speed;
    }

    void TileRenderer::setLabelBlendingSpeed(float speed) {
        std::lock_guard<std::mutex> lock(_mutex);
        _labelBlendingSpeed = speed;
    }

    void TileRenderer::setLabelOrder(int order) {
        std::lock_guard<std::mutex> lock(_mutex);
        _labelOrder = order;
    }
    
    void TileRenderer::setBuildingOrder(int order) {
        std::lock_guard<std::mutex> lock(_mutex);
        _buildingOrder = order;
    }

    void TileRenderer::setRasterFilterMode(vt::RasterFilterMode filterMode) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rasterFilterMode = filterMode;
    }

    void TileRenderer::setNormalMapShadowColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapShadowColor = color;
    }

    void TileRenderer::setNormalMapHighlightColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapHighlightColor = color;
    }
    void TileRenderer::setNormalMapAccentColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapAccentColor = color;
    }
    void TileRenderer::setNormalMapLightingShader(const std::string& shader) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::string newValue = shader;
        if (newValue.length() == 0) {
            newValue = LIGHTING_SHADER_NORMALMAP;
        }
        if (newValue != _normalMapLightingShader) {
            _normalMapLightingShader = newValue;
            _vtRenderer.reset();
        }
    }
    void TileRenderer::setNormalIlluminationDirection(MapVec direction) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalIlluminationDirection = direction;
    }

    void TileRenderer::setNormalIlluminationMapRotationEnabled(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalIlluminationMapRotationEnabled = enabled;
    }

    void TileRenderer::setHillshadeMethod(int method) {
        std::lock_guard<std::mutex> lock(_mutex);
        _hillshadeMethod = method;
    }

    void TileRenderer::setHillshadeExaggeration(float exaggeration) {
        std::lock_guard<std::mutex> lock(_mutex);
        _hillshadeExaggeration = exaggeration;
    }

    void TileRenderer::setRendererLayerFilter(const std::optional<std::regex>& filter) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rendererLayerFilter = filter;
    }

    void TileRenderer::setClickHandlerLayerFilter(const std::optional<std::regex>& filter) {
        std::lock_guard<std::mutex> lock(_mutex);
        _clickHandlerLayerFilter = filter;
    }

    void TileRenderer::offsetLayerHorizontally(double offset) {
        std::lock_guard<std::mutex> lock(_mutex);
        _horizontalLayerOffset += offset;
    }
    
    bool TileRenderer::onDrawFrame(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!initializeRenderer()) {
            return false;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return false;
        }

        cglib::mat4x4<double> modelViewMat = viewState.getModelviewMat() * cglib::translate4_matrix(cglib::vec3<double>(_horizontalLayerOffset, 0, 0));
        vt::ViewState vtViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(), viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        vtViewState.planarTerrain = isPlanarTerrainMode(); // labels rescale by view depth so terrain elevation does not blow up their screen size
        tileRenderer->setViewState(vtViewState);
        tileRenderer->setInteractionMode(_interactionMode);
        tileRenderer->setRasterFilterMode(_rasterFilterMode);
        tileRenderer->setLayerBlendingSpeed(_layerBlendingSpeed);
        tileRenderer->setLabelBlendingSpeed(_labelBlendingSpeed);
        tileRenderer->setRendererLayerFilter(_rendererLayerFilter);

        // Terrain state: enable depth-based terrain rendering and rebuild tile surfaces
        // when the elevation data changes (new DEM tiles, exaggeration change). The rebuild
        // is debounced: during the initial load a new elevation tile may arrive almost every
        // frame and rebuilding all surfaces each time would kill interactivity.
        bool terrainMode = false;
        float terrainDepthBias = 0.0f;
        std::shared_ptr<TerrainOptions> activeTerrainOptions;
        if (auto options = _options.lock()) {
            if (options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
                if (auto terrainOptions = options->getTerrainOptions()) {
                    if (terrainOptions->isEnabled()) {
                        terrainMode = true;
                        // Tile geometry lies exactly on the terrain surfaces (same transformer and
                        // tesselation), so it only needs a small equality slack - the slope-scaled
                        // polygon offset in the vt renderer provides the distance-stable pull
                        // towards the viewer. A large constant clip-space bias would translate to
                        // hundreds of meters of depth tolerance at far distances (see-through ridges).
                        terrainDepthBias = terrainOptions->getDepthBias() * 0.1f;
                        activeTerrainOptions = terrainOptions;
                        unsigned int elevationVersion = terrainOptions->getElevationManager()->getVersion();
                        if (elevationVersion != _elevationVersion) {
                            auto now = std::chrono::steady_clock::now();
                            if (!_lastSurfaceResetTime || now - *_lastSurfaceResetTime > std::chrono::milliseconds(SURFACE_RESET_DELAY)) {
                                _elevationVersion = elevationVersion;
                                _lastSurfaceResetTime = now;
                                tileRenderer->resetTileSurfaces();
                            } else if (auto mapRenderer = _mapRenderer.lock()) {
                                mapRenderer->requestRedraw(); // apply the pending rebuild on a later frame
                            }
                        }
                    }
                }
            }
        }
        // GPU terrain draping: provide elevation textures so that draped geometry is
        // displaced in the vertex shader - every layer samples the same textures, so all
        // layers agree on heights exactly. Requires vertex texture fetch support;
        // without it the CPU displacement path with polygon offsets stays active.
        vt::GLTileRenderer::TerrainTextureProvider terrainTextureProvider;
        if (terrainMode && activeTerrainOptions) {
            if (_maxVertexTextureUnits < 0) {
                GLint maxVertexTextureUnits = 0;
                glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextureUnits);
                _maxVertexTextureUnits = maxVertexTextureUnits;
                if (maxVertexTextureUnits <= 0) {
                    Log::Warn("TileRenderer::onDrawFrame: No vertex texture support, using CPU terrain displacement");
                }
            }
            if (_maxVertexTextureUnits > 0) {
                std::shared_ptr<ElevationManager> elevationManager = activeTerrainOptions->getElevationManager();
                if (_elevationTextureCache && _elevationTextureCache->getElevationManager() != elevationManager) {
                    _elevationTextureCache.reset();
                }
                if (!_elevationTextureCache && elevationManager) {
                    if (auto mapRenderer = _mapRenderer.lock()) {
                        _elevationTextureCache = std::make_shared<ElevationTextureCache>(elevationManager, mapRenderer->getGLResourceManager());
                    }
                }
                if (_elevationTextureCache) {
                    std::shared_ptr<ElevationTextureCache> elevationTextureCache = _elevationTextureCache;
                    terrainTextureProvider = [elevationTextureCache](const vt::TileId& tileId, vt::GLTileRenderer::TerrainTexture& terrainTexture) {
                        return elevationTextureCache->getTexture(tileId, terrainTexture);
                    };
                    // Heights agree exactly between layers in GPU draping mode, so layers are
                    // separated by a fixed clip-space delta by their stacking order (the vt
                    // renderer adds per-sublayer deltas and the geometry slack on top).
                    terrainDepthBias = static_cast<float>(_terrainRenderOrder) * 128.0f / 524288.0f;
                }
            }
        }
        tileRenderer->setTerrainTextureProvider(terrainTextureProvider);
        if (terrainMode && activeTerrainOptions) {
            // Labels are anchored when their tile is decoded, possibly before elevation
            // data arrives - re-anchor them whenever the elevation version changes
            std::shared_ptr<ElevationManager> elevationManager = activeTerrainOptions->getElevationManager();
            tileRenderer->setLabelElevationProvider([elevationManager](const cglib::vec3<double>& pos) {
                return elevationManager->getDisplayHeight(pos(0), pos(1), ElevationManager::LoadMode::CACHED_ONLY);
            }, elevationManager->getVersion());
        } else {
            tileRenderer->setLabelElevationProvider(std::function<double(const cglib::vec3<double>&)>(), 0);
        }
        tileRenderer->setTerrainMode(terrainMode, terrainDepthBias);
        tileRenderer->setTerrainDepthWrite(terrainMode && _terrainDepthWriteMode);
        tileRenderer->setDebugWireframe(false); // debug: terrain mesh wireframe + stencil overlay
        updateLabelOcclusionTest(tileRenderer, viewState, activeTerrainOptions);


        _mapRotation = viewState.getRotation();
        _viewDir = cglib::unit(viewState.getFocusPosNormal());
        if (auto options = _options.lock()) {
            MapPos internalFocusPos = viewState.getProjectionSurface()->calculateMapPos(viewState.getFocusPos());
            _mainLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, options->getMainLightDirection())));
            MapVec normalIlluminationDir = options->getMainLightDirection();
            if (_normalIlluminationDirection != MapVec(0,0,0)) {
                normalIlluminationDir = _normalIlluminationDirection;
            }
            if (_normalIlluminationMapRotationEnabled) {
                double y = normalIlluminationDir.getY();
                double x = normalIlluminationDir.getX();
                double azimuthal = ((x > 0) ? acos(y) : -acos(y)) * Const::RAD_TO_DEG - _mapRotation;
                double sin = std::sin(azimuthal * Const::DEG_TO_RAD);
                double cos = std::cos(azimuthal * Const::DEG_TO_RAD);
                normalIlluminationDir = MapVec(sin, cos, normalIlluminationDir.getZ());
            }

            _normalLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, normalIlluminationDir)));
        }

        bool refresh = false;
        try {
            refresh = tileRenderer->startFrame(deltaSeconds * 3);

            tileRenderer->renderGeometry(true, false);
            if (_labelOrder == 0) {
                tileRenderer->renderLabels(true, false);
            }
            if (_buildingOrder == 0) {
                tileRenderer->renderGeometry(false, true);
            }
            if (_labelOrder == 0) {
                tileRenderer->renderLabels(false, true);
            }
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::onDrawFrame: Rendering failed: %s", ex.what());
        }
    
        // Reset GL state to the expected state
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLContext::CheckGLError("TileRenderer::onDrawFrame");
        return refresh;
    }
    
    bool TileRenderer::onDrawFrame3D(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);
        
        if (!_vtRenderer) {
            return false;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return false;
        }

        bool refresh = false;
        try {
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(true, false);
            }
            if (_buildingOrder == 1) {
                tileRenderer->renderGeometry(false, true);
            }
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(false, true);
            }

            refresh = tileRenderer->endFrame();
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::onDrawFrame3D: Rendering failed: %s", ex.what());
        }

        // Reset GL state to the expected state
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);

        GLContext::CheckGLError("TileRenderer::onDrawFrame3D");
        return refresh;
    }
    
    bool TileRenderer::cullLabels(vt::LabelCuller& culler, const ViewState& viewState) {
        std::shared_ptr<vt::GLTileRenderer> tileRenderer;
        cglib::mat4x4<double> modelViewMat;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            if (_vtRenderer) {
                tileRenderer = _vtRenderer->getTileRenderer();
            }
            modelViewMat = viewState.getModelviewMat() * cglib::translate4_matrix(cglib::vec3<double>(_horizontalLayerOffset, 0, 0));
        }

        if (!tileRenderer) {
            return false;
        }
        vt::ViewState cullViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(),
viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        cullViewState.planarTerrain = isPlanarTerrainMode(); // keep culling envelopes consistent with the rendered label sizes
        culler.setViewState(cullViewState);

        try {
            tileRenderer->cullLabels(culler);
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::cullLabels: Culling failed: %s", ex.what());
            return false;
        }
        return true;
    }
    
    bool TileRenderer::refreshTiles(const std::vector<std::shared_ptr<TileDrawData> >& drawDatas) {
        std::lock_guard<std::mutex> lock(_mutex);

        std::map<vt::TileId, std::shared_ptr<const vt::Tile> > tiles;
        for (const std::shared_ptr<TileDrawData>& drawData : drawDatas) {
            tiles[drawData->getVTTileId()] = drawData->getVTTile();
        }

        bool changed = (tiles != _tiles) || (_horizontalLayerOffset != 0);
        if (!changed) {
            return false;
        }

        if (_vtRenderer) {
            if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer()) {
                if (_horizontalLayerOffset != 0) {
                    tileRenderer->teleportVisibleTiles((int)std::round(_horizontalLayerOffset / Const::WORLD_SIZE), 0);
                }
                tileRenderer->setVisibleTiles(tiles);
            }
        }
        _tiles = std::move(tiles);
        _horizontalLayerOffset = 0;
        return true;
    }

    void TileRenderer::calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        tileRenderer->setClickHandlerLayerFilter(_clickHandlerLayerFilter);

        // Tile geometry is built flat in terrain mode (heights are applied on the GPU):
        // pre-intersect the ray with the terrain surface and pick vertically below the hit
        cglib::ray3<double> geometryRay = ray;
        if (auto options = _options.lock()) {
            if (options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
                if (auto terrainOptions = options->getTerrainOptions()) {
                    if (terrainOptions->isEnabled()) {
                        double t = 0;
                        if (terrainOptions->getElevationManager()->intersectRay(ray, t)) {
                            cglib::vec3<double> hitPos = ray(t);
                            geometryRay = cglib::ray3<double>(cglib::vec3<double>(hitPos(0), hitPos(1), Const::MAX_HEIGHT), cglib::vec3<double>(0, 0, -1));
                        }
                    }
                }
            }
        }

        std::vector<cglib::ray3<double> > geometryRays = { geometryRay };
        std::vector<cglib::ray3<double> > labelRays = { ray }; // labels are anchored at terrain height, use the original ray
        tileRenderer->findGeometryIntersections(geometryRays, radius, radius, true, false, results);
        if (_labelOrder == 0) {
            tileRenderer->findLabelIntersections(labelRays, radius, true, false, results);
        }
        if (_buildingOrder == 0) {
            tileRenderer->findGeometryIntersections(geometryRays, radius, radius, false, true, results);
        }
        if (_labelOrder == 0) {
            tileRenderer->findLabelIntersections(labelRays, radius, false, true, results);
        }
    }
        
    void TileRenderer::calculateRayIntersectedElements3D(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        std::vector<cglib::ray3<double> > rays = { ray };
        if (_labelOrder == 1) {
            tileRenderer->findLabelIntersections(rays, radius, true, false, results);
        }
        if (_buildingOrder == 1) {
            tileRenderer->findGeometryIntersections(rays, radius, radius, false, true, results);
        }
        if (_labelOrder == 1) {
            tileRenderer->findLabelIntersections(rays, radius, false, true, results);
        }
    }

    void TileRenderer::calculateRayIntersectedBitmaps(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<vt::GLTileRenderer::BitmapIntersectionInfo>& results) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (!_vtRenderer) {
            return;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return;
        }

        std::vector<cglib::ray3<double> > rays = { ray };
        tileRenderer->findBitmapIntersections(rays, results);
    }

    Color TileRenderer::evaluateColorFunc(const vt::ColorFunction& colorFunc, const ViewState& viewState) {
        cglib::mat4x4<double> modelViewMat = viewState.getModelviewMat();
        vt::ViewState vtViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(),
viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        return Color(colorFunc(vtViewState).value());
    }

    bool TileRenderer::isPlanarTerrainMode() const {
        if (auto options = _options.lock()) {
            if (options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
                if (auto terrainOptions = options->getTerrainOptions()) {
                    return terrainOptions->isEnabled();
                }
            }
        }
        return false;
    }

    void TileRenderer::updateLabelOcclusionTest(const std::shared_ptr<vt::GLTileRenderer>& tileRenderer, const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions) {
        if (!terrainOptions || !terrainOptions->isBillboardOcclusionEnabled()) {
            _labelOcclusionState.reset();
            tileRenderer->setLabelOcclusionTest(std::function<bool(const cglib::vec3<double>&)>());
            return;
        }

        // Preferred path: pixel-exact occlusion against the read-back terrain depth buffer
        // (rendered by MapRenderer each frame) - matches what is actually on screen and is
        // much cheaper than ray-marching the elevation grids per label.
        if (auto mapRenderer = _mapRenderer.lock()) {
            if (mapRenderer->getTerrainRenderer() != nullptr) {
                {
                    _labelOcclusionState.reset();
                    cglib::mat4x4<double> mvpMat = viewState.getModelviewProjectionMat();
                    float screenWidth = static_cast<float>(viewState.getWidth());
                    float screenHeight = static_cast<float>(viewState.getHeight());
                    std::weak_ptr<MapRenderer> mapRendererWeak = _mapRenderer;
                    tileRenderer->setLabelOcclusionTest([mapRendererWeak, mvpMat, screenWidth, screenHeight](const cglib::vec3<double>& pos) {
                        auto mapRenderer = mapRendererWeak.lock();
                        if (!mapRenderer) {
                            return false;
                        }
                        TerrainRenderer* terrainRenderer = mapRenderer->getTerrainRenderer();
                        if (!terrainRenderer) {
                            return false;
                        }
                        cglib::vec4<double> clipPos = cglib::transform(cglib::vec4<double>(pos(0), pos(1), pos(2), 1), mvpMat);
                        if (clipPos(3) <= 0) {
                            return false;
                        }
                        float screenX = static_cast<float>((clipPos(0) / clipPos(3) * 0.5 + 0.5) * screenWidth);
                        float screenY = static_cast<float>((0.5 - clipPos(1) / clipPos(3) * 0.5) * screenHeight);
                        float depthW = terrainRenderer->getDepthW(screenX, screenY);
                        // occluded if clearly behind the terrain at this pixel (labels are
                        // anchored ON the terrain, so allow a small relative tolerance)
                        return static_cast<float>(clipPos(3)) > depthW * 1.02f;
                    });
                    return;
                }
            }
        }

        std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager();
        if (!_labelOcclusionState) {
            _labelOcclusionState = std::make_shared<LabelOcclusionState>();
        }
        std::shared_ptr<LabelOcclusionState> state = _labelOcclusionState;

        // Invalidate cached results when the camera moves significantly or the elevation data changes
        cglib::vec3<double> cameraPos = viewState.getCameraPos();
        double moveThreshold = 0.01 * cglib::length(viewState.getFocusPos() - cameraPos);
        unsigned int elevationVersion = elevationManager->getVersion();
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (cglib::length(cameraPos - state->cameraPos) > moveThreshold || elevationVersion != state->elevationVersion) {
                state->results.clear();
                state->cameraPos = cameraPos;
                state->elevationVersion = elevationVersion;
            }
        }

        tileRenderer->setLabelOcclusionTest([state, elevationManager, cameraPos](const cglib::vec3<double>& pos) -> bool {
            // Quantize the position for caching (roughly 4m grid)
            const double QUANT = 10.0;
            long long key = (static_cast<long long>(pos(0) * QUANT) * 73856093LL) ^ (static_cast<long long>(pos(1) * QUANT) * 19349663LL) ^ (static_cast<long long>(pos(2) * QUANT) * 83492791LL);
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                auto it = state->results.find(key);
                if (it != state->results.end()) {
                    return it->second;
                }
            }

            double dist = cglib::length(pos - cameraPos);
            cglib::vec3<double> target = pos + cglib::vec3<double>(0, 0, dist * 0.005);
            cglib::ray3<double> ray(cameraPos, target - cameraPos);
            double t = 0;
            bool occluded = elevationManager->intersectRay(ray, t) && t > 0 && t < 0.995;
            {
                std::lock_guard<std::mutex> lock(state->mutex);
                state->results[key] = occluded;
            }
            return occluded;
        });
    }

    bool TileRenderer::initializeRenderer() {
        if (_vtRenderer && _vtRenderer->isValid()) {
            return true;
        }

        std::shared_ptr<MapRenderer> mapRenderer = _mapRenderer.lock();
        if (!mapRenderer) {
            return false; // safety check, should never happen
        }

        Log::Debug("TileRenderer: Initializing renderer");
        _vtRenderer = mapRenderer->getGLResourceManager()->create<VTRenderer>(_tileTransformer);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer()) {
            tileRenderer->setVisibleTiles(_tiles);

            if (!std::dynamic_pointer_cast<PlanarProjectionSurface>(mapRenderer->getProjectionSurface())) {
                vt::GLTileRenderer::LightingShader lightingShader2D(true, LIGHTING_SHADER_2D, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_viewDir"), 1, _viewDir.data());
                });
                tileRenderer->setLightingShader2D(lightingShader2D);
            }

            vt::GLTileRenderer::LightingShader lightingShader3D(true, LIGHTING_SHADER_3D, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                if (auto options = _options.lock()) {
                    const Color& ambientLightColor = options->getAmbientLightColor();
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_ambientColor"), ambientLightColor.getR() / 255.0f, ambientLightColor.getG() / 255.0f, ambientLightColor.getB() / 255.0f, ambientLightColor.getA() / 255.0f);
                    const Color& mainLightColor = options->getMainLightColor();
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_lightColor"), mainLightColor.getR() / 255.0f, mainLightColor.getG() / 255.0f, mainLightColor.getB() / 255.0f, mainLightColor.getA() / 255.0f);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDir"), 1, _mainLightDir.data());
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_viewDir"), 1, _viewDir.data());
                }
            });
            tileRenderer->setLightingShader3D(lightingShader3D);

            vt::GLTileRenderer::LightingShader lightingShaderNormalMap(false, _normalMapLightingShader, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                    // Pass colors without premultiplying RGB by alpha, matching MapLibre's approach
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_shadowColor"), _normalMapShadowColor.getR() / 255.0f, _normalMapShadowColor.getG() / 255.0f, _normalMapShadowColor.getB() / 255.0f, _normalMapShadowColor.getA() / 255.0f);
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_accentColor"), _normalMapAccentColor.getR() / 255.0f, _normalMapAccentColor.getG() / 255.0f, _normalMapAccentColor.getB() / 255.0f, _normalMapAccentColor.getA() / 255.0f);
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_highlightColor"), _normalMapHighlightColor.getR() / 255.0f, _normalMapHighlightColor.getG() / 255.0f, _normalMapHighlightColor.getB() / 255.0f, _normalMapHighlightColor.getA() / 255.0f);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDir"), 1, _normalLightDir.data() );
                    glUniform1i(glGetUniformLocation(shaderProgram, "u_method"), (_hillshadeMethod));
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_exaggeration"), _hillshadeExaggeration);
            });
            tileRenderer->setLightingShaderNormalMap(lightingShaderNormalMap);
        }

        return _vtRenderer && _vtRenderer->isValid();
    }

    const std::string TileRenderer::LIGHTING_SHADER_2D = R"GLSL(
        uniform vec3 u_viewDir;
        vec4 applyLighting(lowp vec4 color, mediump vec3 normal) {
            mediump float lighting = max(0.0, dot(normal, u_viewDir)) * 0.5 + 0.5;
            return vec4(color.rgb * lighting, color.a);
        }
    )GLSL";

    const std::string TileRenderer::LIGHTING_SHADER_3D = R"GLSL(
        uniform vec4 u_ambientColor;
        uniform vec4 u_lightColor;
        uniform vec3 u_lightDir;
        uniform vec3 u_viewDir;
        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, highp_opt float height, bool sideVertex) {
            if (sideVertex) {
                lowp vec3 dimmedColor = color.rgb * (1.0 - 0.5 / (1.0 + height * height));
                mediump vec3 lighting = max(0.0, dot(normal, u_lightDir)) * u_lightColor.rgb + u_ambientColor.rgb;
                return vec4(dimmedColor.rgb * lighting, color.a);
            } else {
                mediump float lighting = max(0.0, dot(normal, u_viewDir)) * 0.5 + 0.5;
                return vec4(color.rgb * lighting, color.a);
            }
        }
    )GLSL";

    const std::string TileRenderer::LIGHTING_SHADER_NORMALMAP = R"GLSL(
        uniform vec4 u_shadowColor;
        uniform vec4 u_highlightColor;
        uniform vec4 u_accentColor;
        uniform vec3 u_lightDir;
        uniform int u_method;
        uniform float u_exaggeration;

        #define PI 3.141592653589793
        #define STANDARD 0
        #define COMBINED 1
        #define IGOR 2
        #define MULTIDIRECTIONAL 3
        #define BASIC 4

        float get_aspect(vec2 deriv) {
            return deriv.x != 0.0 ? atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
        }

        // Based on GDALHillshadeIgorAlg()
        vec4 igor_hillshade(vec2 deriv, vec3 lightDir) {
            float aspect = get_aspect(deriv);
            // Convert light direction to azimuth
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float slope_strength = atan(length(deriv)) * 2.0/PI;
            float aspect_strength = 1.0 - abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            float shadow_strength = slope_strength * aspect_strength;
            float highlight_strength = slope_strength * (1.0-aspect_strength);
            vec4 result = u_shadowColor * shadow_strength + u_highlightColor * highlight_strength;
            // Premultiply RGB by alpha to handle transparency correctly
            return vec4(result.rgb * result.a, result.a);
        }

        // MapLibre's legacy hillshade algorithm
        vec4 standard_hillshade(vec2 deriv, vec3 lightDir) {
            // Convert light direction to azimuth
            float azimuth = atan(lightDir.y, lightDir.x) + PI;

            // We multiply the slope by an arbitrary z-factor of 0.625
            float slope = atan(0.625 * length(deriv));
            float aspect = get_aspect(deriv);

            float intensity = u_exaggeration;

            // Scale the slope exponentially based on intensity
            float base = 1.875 - intensity * 1.75;
            float maxValue = 0.5 * PI;
            float scaledSlope = intensity != 0.5 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

            // The accent color is calculated with the cosine of the slope
            float accent = cos(scaledSlope);
            vec4 accent_color = (1.0 - accent) * u_accentColor * clamp(intensity * 2.0, 0.0, 1.0);
            
            // Shade color based on aspect and azimuth
            float shade = abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            vec4 shade_color = mix(u_shadowColor, u_highlightColor, shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);
            
            vec4 result = accent_color * (1.0 - shade_color.a) + shade_color;
            // Premultiply RGB by alpha to handle transparency correctly
            return vec4(result.rgb * result.a, result.a);
        }

        // Based on GDALHillshadeAlg()
        vec4 basic_hillshade(vec2 deriv, vec3 lightDir) {
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float altitude = asin(clamp(lightDir.z, -1.0, 1.0));
            
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = (sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv));

            float shade = clamp(cang, 0.0, 1.0);
            vec4 result;
            if(shade > 0.5) {
                result = u_highlightColor * (2.0*shade - 1.0);
            } else {
                result = u_shadowColor * (1.0 - 2.0*shade);
            }
            // Premultiply RGB by alpha to handle transparency correctly
            return vec4(result.rgb * result.a, result.a);
        }

        // Multidirectional hillshade (simplified to single light for now)
        vec4 multidirectional_hillshade(vec2 deriv, vec3 lightDir) {
            // For now, just use basic hillshade with the main light
            // In the future, this could be extended to support multiple lights
            return basic_hillshade(deriv, lightDir);
        }

        // Based on GDALHillshadeCombinedAlg()
        vec4 combined_hillshade(vec2 deriv, vec3 lightDir) {
            float azimuth = atan(lightDir.y, lightDir.x) + PI;
            float altitude = asin(clamp(lightDir.z, -1.0, 1.0));
            
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = acos(clamp((sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv)), -1.0, 1.0));

            cang = clamp(cang, 0.0, PI/2.0);

            float shade = cang * atan(length(deriv)) * 4.0/PI/PI;
            float highlight = (PI/2.0-cang) * atan(length(deriv)) * 4.0/PI/PI;

            vec4 result = u_shadowColor*shade + u_highlightColor*highlight;
            // Premultiply RGB by alpha to handle transparency correctly
            return vec4(result.rgb * result.a, result.a);
        }

        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {
            // Extract derivatives from normal vector
            // The normal is already in tangent space where z points up
            // For a flat surface, normal would be (0, 0, 1)
            // The derivatives represent the slope in x and y directions
            vec2 deriv = vec2(-normal.x / max(normal.z, 0.001), -normal.y / max(normal.z, 0.001));
            
            // Apply exaggeration to derivatives
            deriv *= u_exaggeration * 2.0;
            
            vec4 hillshadeColor;
            if (u_method == BASIC) {
                hillshadeColor = basic_hillshade(deriv, u_lightDir);
            } else if (u_method == COMBINED) {
                hillshadeColor = combined_hillshade(deriv, u_lightDir);
            } else if (u_method == IGOR) {
                hillshadeColor = igor_hillshade(deriv, u_lightDir);
            } else if (u_method == MULTIDIRECTIONAL) {
                hillshadeColor = multidirectional_hillshade(deriv, u_lightDir);
            } else {
                // STANDARD (default)
                hillshadeColor = standard_hillshade(deriv, u_lightDir);
            }
            
            // The VT library multiplies our result by 'intensity', but hillshading should have
            // constant strength regardless of slope. Divide by intensity to cancel this out.
            // Use max() to avoid division by zero for flat areas.
            float intensityFactor = max(intensity, 0.01);
            return vec4(hillshadeColor.rgb / intensityFactor, hillshadeColor.a);
        }
    )GLSL";

}
