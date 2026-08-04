#include "TileRenderer.h"

#include <vt/RenderStats.h>
#include "components/Options.h"
#include "components/LightOptions.h"
#include "components/TerrainOptions.h"
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

#ifdef __ANDROID__
#include <sys/system_properties.h>
#include <cstdlib>
#endif
#include "utils/Const.h"

#include <vt/Label.h>
#include <vt/LabelCuller.h>
#include <vt/TileTransformer.h>
#include <vt/GLExtensions.h>
#include <vt/NormalMapBuilder.h>

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
        _hillshadeExaggeration(1.0f),
        _hillshadeIntensity(0.5f),
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
    void TileRenderer::setNormalMapElevationEncoded(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapElevationEncoded = enabled;
    }
    void TileRenderer::setNormalMapContourInterval(float interval) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapContourInterval = interval;
    }
    void TileRenderer::setNormalMapContourColor(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapContourColor = color;
    }
    void TileRenderer::setNormalMapContourWidth(float width) {
        std::lock_guard<std::mutex> lock(_mutex);
        _normalMapContourWidth = width;
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

    void TileRenderer::setHillshadeIntensity(float intensity) {
        std::lock_guard<std::mutex> lock(_mutex);
        _hillshadeIntensity = intensity;
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
    
    bool TileRenderer::prepareFrame(float deltaSeconds, const ViewState& viewState) {
        std::lock_guard<std::mutex> lock(_mutex);

        return prepareFrameUnsafe(deltaSeconds, viewState);
    }

    // Caller must hold _mutex. onDrawFrame already does, and _mutex is not recursive.
    bool TileRenderer::prepareFrameUnsafe(float deltaSeconds, const ViewState& viewState) {
        if (_framePrepared) {
            return _framePrepareResult;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>());
        if (!tileRenderer) {
            return false;
        }
        _framePrepared = true;
        _framePrepareResult = false;
        // The cross-layer drape draws the terrain surface from MapRenderer, BEFORE onDrawFrame
        // sets the view state. Without this the surface is drawn with the previous frame's camera
        // while everything else uses the current one, so the ground lags the buildings by exactly
        // one frame during a pan and snaps into place when the motion stops.
        cglib::mat4x4<double> prepareModelViewMat = viewState.getModelviewMat() * cglib::translate4_matrix(cglib::vec3<double>(_horizontalLayerOffset, 0, 0));
        vt::ViewState prepareViewState(viewState.getProjectionMat(), prepareModelViewMat, viewState.getZoom(), viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        prepareViewState.planarTerrain = isPlanarTerrainMode();
        tileRenderer->setViewState(prepareViewState);
        try {
            _framePrepareResult = tileRenderer->startFrame(deltaSeconds * 3);
        }
        catch (const std::exception& ex) {
            Log::Errorf("TileRenderer::prepareFrame: Failed: %s", ex.what());
        }
        return _framePrepareResult;
    }

    void TileRenderer::setExternalDrapeTarget(bool enabled) {
        std::lock_guard<std::mutex> lock(_mutex);

        _externalDrapeTarget = enabled;
        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setExternalDrapeTarget(enabled);
        }
    }

    void TileRenderer::setExternalDrapeTiles(const std::vector<vt::TileId>& tileIds) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setExternalDrapeTiles(tileIds);
        }
    }

    void TileRenderer::setTerrainStackOrdinalSpan(int span) {
        _terrainStackOrdinalSpan.store(span);
    }

    int TileRenderer::getStyleLayerCount() const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->getStyleLayerCount();
        }
        return 0;
    }

    void TileRenderer::setTerrainLayerOrdinalBase(int base) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setTerrainLayerOrdinalBase(base);
        }
    }

    void TileRenderer::setTerrainGroundTiles(const std::vector<vt::TileId>& tileIds, const std::vector<int>& proxyDepths) {
        std::lock_guard<std::mutex> lock(_mutex);

        _terrainGroundActive = !tileIds.empty();
        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setTerrainGroundTiles(tileIds, proxyDepths);
        }
    }

    int TileRenderer::renderTerrainGround(const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->renderTerrainGround(vt::Color(color.getR() / 255.0f, color.getG() / 255.0f, color.getB() / 255.0f, color.getA() / 255.0f));
        }
        return 0;
    }

    bool TileRenderer::isDrapeEnabled() const {
        std::lock_guard<std::mutex> lock(_mutex);

        return _externalDrapeTarget;
    }

    void TileRenderer::collectDrapeTiles(std::map<vt::TileId, std::size_t>& drapeTiles) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->collectDrapeTiles(drapeTiles);
        }
    }

    int TileRenderer::bakeDrapeTile(const vt::TileId& tileId) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->bakeDrapeTile(tileId);
        }
        return 0;
    }

    int TileRenderer::renderDrapedSurface(const vt::TileId& tileId, unsigned int drapeTexture, float uvOffsetX, float uvOffsetY, float uvScale) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->renderDrapedSurface(tileId, static_cast<GLuint>(drapeTexture), uvOffsetX, uvOffsetY, uvScale);
        }
        return -4;
    }

    int TileRenderer::blitDrapeTexture(unsigned int srcTexture, float dstOffsetX, float dstOffsetY, float dstScale, float uvOffsetX, float uvOffsetY, float uvScale) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->blitDrapeTexture(static_cast<GLuint>(srcTexture), dstOffsetX, dstOffsetY, dstScale, uvOffsetX, uvOffsetY, uvScale);
        }
        return -4;
    }

    int TileRenderer::renderDrapedSurfaceFill(const vt::TileId& tileId, const Color& color) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->renderDrapedSurfaceFill(tileId, vt::Color(color.getR() / 255.0f, color.getG() / 255.0f, color.getB() / 255.0f, color.getA() / 255.0f));
        }
        return -4;
    }

    bool TileRenderer::calculateShadowViewProj(const std::vector<vt::TileId>& tileIds, const std::vector<vt::TileId>& casterTileIds, const cglib::vec3<float>& sunDir, const std::vector<std::pair<double, double> >& tileHeights, double minHeight, double maxHeight, float maxDistanceMeters, int mapSize, int cascade, int cascadeCount, std::vector<vt::TileId>& boxCasterTileIds, double& depthRangeMeters, double& texelMeters, cglib::mat4x4<double>& lightViewProj) const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->calculateShadowViewProj(tileIds, casterTileIds, sunDir, tileHeights, minHeight, maxHeight, maxDistanceMeters, mapSize, cascade, cascadeCount, boxCasterTileIds, depthRangeMeters, texelMeters, lightViewProj);
        }
        return false;
    }

    float TileRenderer::shadowCasterFadeSignature() const {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->shadowCasterFadeSignature();
        }
        return 0.0f;
    }

    int TileRenderer::renderShadowCasters(const std::vector<vt::TileId>& tileIds, const cglib::mat4x4<double>& lightViewProj, bool castGround) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            return tileRenderer->renderShadowCasters(tileIds, lightViewProj, castGround);
        }
        return 0;
    }

    void TileRenderer::setTerrainShadowMap(unsigned int texture, int mapSize, int cascades, const std::array<float, 4>& depthBiases, float strength, float softness, const std::array<cglib::mat4x4<double>, 4>& lightViewProjs) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setTerrainShadowMap(static_cast<GLuint>(texture), mapSize, cascades, depthBiases, strength, softness, lightViewProjs);
        }
    }

    void TileRenderer::setTerrainSunLighting(bool enabled, const cglib::vec3<float>& sunDir, const Color& sunColor, float sunIntensity, float ambientIntensity) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            vt::GLTileRenderer::TerrainLighting terrainLighting;
            if (enabled) {
                terrainLighting.enabled = true;
                terrainLighting.sunDir = sunDir;
                terrainLighting.sunColor = cglib::vec3<float>(sunColor.getR() / 255.0f, sunColor.getG() / 255.0f, sunColor.getB() / 255.0f);
                terrainLighting.sunIntensity = sunIntensity;
                terrainLighting.ambientIntensity = ambientIntensity;
            }
            tileRenderer->setTerrainLighting(terrainLighting);
        }
    }

    void TileRenderer::setTerrainPaintTiles(const std::vector<vt::TileId>& tileIds) {
        std::lock_guard<std::mutex> lock(_mutex);

        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            tileRenderer->setTerrainPaintTiles(tileIds);
        }
    }

    void TileRenderer::setTerrainPaint(bool enabled, bool fullDetail, float heightScale, bool exaggerateHeightScale, bool legacyHeightScale, float contrast, float opacity, std::size_t fingerprint) {
        std::lock_guard<std::mutex> lock(_mutex);

        _terrainPaintEnabled = enabled;
        _terrainPaintFullDetail = fullDetail;
        if (std::shared_ptr<vt::GLTileRenderer> tileRenderer = (_vtRenderer ? _vtRenderer->getTileRenderer() : std::shared_ptr<vt::GLTileRenderer>())) {
            vt::GLTileRenderer::TerrainPaint paint;
            paint.enabled = enabled;
            paint.heightScale = heightScale;
            paint.exaggerateHeightScale = exaggerateHeightScale;
            paint.legacyHeightScale = legacyHeightScale;
            paint.contrast = contrast;
            paint.opacity = opacity;
            paint.fingerprint = fingerprint;
            tileRenderer->setTerrainPaint(paint);
            tileRenderer->setTerrainPaintOnGround(isTerrainPaintOnGroundForced());
            tileRenderer->setTerrainDemTaps(terrainDemTaps());
            tileRenderer->setTerrainTileBackgrounds(isTerrainTileBackgroundsForced());
        }
    }

    // Measurement switch for tangram's arrangement: the paint drawn AS the ground, one draw per
    // tile at the bottom of the order, instead of as its layer's own surface over the ground fill.
    // Cheaper by one full-surface draw per tile, but it puts the shading under every ground-shaped
    // fill - which only looks right when those fills are translucent (tangram's earth style) or
    // when nothing ground-shaped is drawn below the paint's layer.
    //   adb shell setprop debug.carto.groundpaint 1
    // Texture fetches per terrain vertex: 16 (lattice clamp) / 4 (manual bilinear) / 1 (one
    // hardware-filtered fetch, tangram's terrain vertex). Vertex texture fetch is expensive on
    // mobile GPUs and this is 16x what the reference does, so it is the first suspect whenever the
    // frame sits in the swap wait.
    //   adb shell setprop debug.carto.demtaps 4
    // debug.carto.tilebg 1 restores the per-layer per-tile background meshes tangram does not have.
#ifdef __ANDROID__
    bool TileRenderer::isTerrainTileBackgroundsForced() {
        static const bool forced = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            return __system_property_get("debug.carto.tilebg", property) > 0 && property[0] == '1';
        }();
        return forced;
    }
#else
    bool TileRenderer::isTerrainTileBackgroundsForced() {
        return false;
    }
#endif

#ifdef __ANDROID__
    int TileRenderer::terrainDemTaps() {
        static const int taps = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.demtaps", property) > 0) {
                int value = std::atoi(property);
                if (value > 0) {
                    return value;
                }
            }
            return 16;
        }();
        return taps;
    }
#else
    int TileRenderer::terrainDemTaps() {
        return 16;
    }
#endif

#ifdef __ANDROID__
    bool TileRenderer::isTerrainPaintOnGroundForced() {
        static const bool forced = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            return __system_property_get("debug.carto.groundpaint", property) > 0 && property[0] == '1';
        }();
        return forced;
    }
#else
    bool TileRenderer::isTerrainPaintOnGroundForced() {
        return false;
    }
#endif

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
                        const std::shared_ptr<ElevationManager>& elevationManager = terrainOptions->getElevationManager();
                        unsigned int elevationVersion = elevationManager->getVersion();
                        if (elevationVersion != _elevationVersion) {
                            auto now = std::chrono::steady_clock::now();
                            // The elevation version is global but a decoded elevation tile only
                            // changes the surfaces over that tile. Rebuilding every visible
                            // surface for it re-tesselates and re-uploads the whole screen -
                            // repeatedly, while the initial elevation stream is running, with
                            // nothing on most of it actually changing. Ask which tiles changed
                            // and drop only those; the global reset stays as the fallback for
                            // whole-data-set changes (data source change, exaggeration) and for
                            // change-log overflow.
                            std::vector<MapTile> changedTiles;
                            if (_elevationVersion != 0 && elevationManager->getChangedTiles(_elevationVersion, changedTiles)) {
                                _elevationVersion = elevationVersion;
                                std::vector<vt::TileId> changedTileIds;
                                changedTileIds.reserve(changedTiles.size());
                                for (const MapTile& changedTile : changedTiles) {
                                    changedTileIds.emplace_back(changedTile.getZoom(), changedTile.getX(), changedTile.getY());
                                }
                                tileRenderer->invalidateTileSurfaces(changedTileIds);
                                // Labels are anchored onto the terrain the same way, and at one
                                // elevation sample per label vertex a blanket re-anchor of the
                                // visible label set costs several hundred milliseconds - the
                                // same targeted list keeps it to the labels actually affected.
                                tileRenderer->invalidateLabelElevation(changedTileIds);
                            } else if (!_lastSurfaceResetTime || now - *_lastSurfaceResetTime > std::chrono::milliseconds(SURFACE_RESET_DELAY)) {
                                _elevationVersion = elevationVersion;
                                _lastSurfaceResetTime = now;
                                tileRenderer->resetTileSurfaces();
                                tileRenderer->invalidateLabelElevation();
                            } else if (auto mapRenderer = _mapRenderer.lock()) {
                                mapRenderer->requestRedraw(); // apply the pending rebuild on a later frame
                                // This path asks for a frame without drawing anything new. It is
                                // meant to be a handful of frames while a rebuild is debounced; if
                                // the elevation version never settles it is an endless render loop
                                // instead, so say so rather than leaving it to be inferred from the
                                // battery.
                                static int pendingRebuildFrames = 0;
                                if ((++pendingRebuildFrames % 300) == 0) {
                                    Log::Infof("TileRenderer: %d frames spent waiting on an elevation rebuild, version %u", pendingRebuildFrames, elevationVersion);
                                }
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
                if (elevationManager) {
                    // Cap elevation levels at what the mesh can express - for every elevation
                    // consumer, not just the drawn surface (billboard occlusion ray marching and
                    // element placement query the same manager and must see the same heights).
                    elevationManager->setSurfaceResolution(activeTerrainOptions->getMeshResolution());
                }
                if (_elevationTextureCache && _elevationTextureCache->getElevationManager() != elevationManager) {
                    _elevationTextureCache.reset();
                }
                if (!_elevationTextureCache && elevationManager) {
                    if (auto mapRenderer = _mapRenderer.lock()) {
                        _elevationTextureCache = std::make_shared<ElevationTextureCache>(elevationManager, mapRenderer->getGLResourceManager());
                    }
                }
                if (_elevationTextureCache) {
                    // A paint renderer's only consumer of the elevation texture is the shading,
                    // which is per fragment: it resolves relief the mesh cannot, so its cache can
                    // ignore the mesh's level cap (which costs two zoom levels - at high zoom, all
                    // the relief there is). Each level back is 4x the elevation texture working set,
                    // so it is a dial, not a flag (DEFAULT_PAINT_DETAIL_LEVELS, and on Android:
                    //   adb shell setprop debug.carto.paintdetail 0|1|2   (2 = the source's own level)
                    // Tangram pays nothing here because it binds the source raster as-is and
                    // extrapolates edges in the shader; ours is CPU re-encoded per DEM tile with a
                    // border ring taken from 8 neighbours.
                    _elevationTextureCache->setDetailLevels(_terrainPaintEnabled && _terrainPaintFullDetail ? terrainPaintDetailLevels() : 0);
                    // The zoom the camera is at, so a tile the LOD left coarser than that can be
                    // shaded from its own zoom's DEM instead of from one coarser again.
                    _elevationTextureCache->setCameraTileZoom(static_cast<int>(viewState.getZoom()));
                    _elevationTextureCache->beginFrame();
                    std::shared_ptr<ElevationTextureCache> elevationTextureCache = _elevationTextureCache;
                    terrainTextureProvider = [elevationTextureCache](const vt::TileId& tileId, vt::GLTileRenderer::TerrainTexture& terrainTexture) {
                        return elevationTextureCache->getTexture(tileId, terrainTexture);
                    };
                    // Every terrain tile layer works in its own depth domain (the vt
                    // renderer clears the depth buffer and renders its reference surface
                    // pre-pass before its content, which then WRITES its real depth -
                    // tangram-style). Cross-layer stacking is pure painter's order, so no
                    // per-layer depth stride is needed - and any constant-NDC stride
                    // would shift the final depth domain away from what vector elements
                    // depth-test against after the tile layers.
                    terrainDepthBias = 0.0f;
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
            });
        } else {
            tileRenderer->setLabelElevationProvider(std::function<double(const cglib::vec3<double>&)>());
        }
        tileRenderer->setTerrainMode(terrainMode, terrainDepthBias);
        // The geometry-vs-surface chord error shrinks quadratically with the mesh
        // resolution (both the tile surfaces and the draped geometry tesselate to
        // tileMeters/meshResolution cells), so the depth slack can shrink with it.
        // The default resolution 32 maps to factor 1 (the calibrated slack).
        float terrainSlackScale = 1.0f;
        if (terrainMode && activeTerrainOptions) {
            float resolutionRatio = 32.0f / std::max(32, activeTerrainOptions->getMeshResolution());
            terrainSlackScale = resolutionRatio * resolutionRatio;
        }
        tileRenderer->setTerrainSlackScale(terrainSlackScale);
        // Painter-order depth model (tangram-style): the surface is the bottom painter
        // layer and content is separated by a per-layer clip delta (no occluder, no slack).
        // Implies the shared regular grid. Only in GPU draping mode.
        bool painterOrder = terrainMode && activeTerrainOptions && activeTerrainOptions->isPainterOrderDepthEnabled() && (bool) terrainTextureProvider;
        // Shared regular grid surfaces (tangram-style): one grid built once and reused for
        // every tile, instead of per-tile adaptive tesselation. Only in GPU draping mode.
        // Maplibre-style RTT draping. It requires the shared regular grid: the drape UV is the
        // grid's tile-local [0,1] vertex position, which only the regular grid provides.
        bool drapeFills = terrainMode && activeTerrainOptions && activeTerrainOptions->isDrapeFillsEnabled() && (bool) terrainTextureProvider;
        bool regularGrid = painterOrder || drapeFills || (terrainMode && activeTerrainOptions && activeTerrainOptions->isRegularGridEnabled() && (bool) terrainTextureProvider);
        tileRenderer->setTerrainRegularGrid(regularGrid, activeTerrainOptions ? activeTerrainOptions->getMeshResolution() : 0);
        tileRenderer->setTerrainPainterOrder(painterOrder);
        // Tangram's content depth shift. polygon.vs/polyline.vs set `depth_shift = 0.0` and leave
        // it "to allow blocks to modify" - and their 3D TERRAIN scene is one of the blocks that
        // does: res/scenes/terrain-3d.yaml sets `depth_shift = -0.02*u_proj[2][3]`, which with
        // glm::perspective's [2][3] = -1 is a flat 0.02. So it is part of the terrain depth model,
        // not an experiment, and it is what keeps un-subdivided content from sinking into the
        // ground it chords over. Overridable for measurement:
        //   adb shell setprop debug.carto.depthshift <value>
        // Their CONSTANT is 0.02, but the quantity that matters is the constant times the ORDER
        // SPAN of the stack - that product is the depth budget the whole stack gets, and it is what
        // has to be reproduced. res/osm-bright.yaml numbers its style layers 1..93, so tangram
        // spends ~1.86 clip units across the scene. Our ordinals are a dense rank, so a style with
        // nine style layers spends 0.18 at their constant - a tenth of the budget, and far too
        // little for a fill's tesselation to clear the surface. Scale to their budget instead: a
        // 93-layer style gets 0.02 back, exactly their number.
        // Measured on device (45.244172/5.760595 z13.2 t26, 9 ordinals): 0.2 is the largest value
        // with no see-through, 0.3 opens pale wedges through the ridges and 0.5 more - and 0.2 is
        // what the budget gives. Their budget IS the leak threshold; do not raise it.
        float contentDepthShift = getTerrainContentDepthShift();
        if (_terrainGroundActive && contentDepthShift == 0.0f) {
            contentDepthShift = TERRAIN_TANGRAM_DEPTH_BUDGET / std::max(1, _terrainStackOrdinalSpan.load());
        }
        tileRenderer->setTerrainContentDepthShift(contentDepthShift);
        tileRenderer->setTerrainEdgeStitching(regularGrid && activeTerrainOptions && activeTerrainOptions->isTileEdgeStitchingEnabled());
        // Draped content is baked FLAT (orthographic, no displacement), so lines need no terrain
        // subdivision either - draping them is strictly cheaper as well as artifact-free. It is
        // also the drape texture's resolution though: a line baked into it is magnified with the
        // texture, which turns dense thin lines (contours on a steep slope) into a blurred wash.
        // DrapeLines is what trades the one for the other, and TileLayer already decodes lines at
        // source density / subdivided to match it.
        bool drapeLines = drapeFills && activeTerrainOptions && activeTerrainOptions->isDrapeLinesEnabled();
        tileRenderer->setTerrainDrapeFills(drapeFills, drapeLines);
        tileRenderer->setTerrainDrapeResolution(activeTerrainOptions ? activeTerrainOptions->getDrapeResolution() : 512);
        // Sun lighting of the draped surface. Once every 2D layer is baked into the drape
        // texture the surface is the only lit ground geometry in the scene, so the whole map
        // is shaded by one directional light that follows the time of day - and the pre-baked
        // hillshade raster layer becomes optional rather than the only way to get relief.
        vt::GLTileRenderer::TerrainLighting terrainLighting;
        if (auto options = _options.lock()) {
            // The style's values win over the options wherever it has an opinion; the rest of the
            // sun stays with LightOptions. Both are re-read every frame, so either may depend on
            // the zoom.
            ResolvedLighting lighting = resolveLighting(options->getLightOptions(), _styleEnvironment);
            // Kept for the 3D lighting shader callback, which runs at DRAW time and used to read
            // LightOptions straight - so a style that had an opinion about the sun moved the
            // terrain and left the buildings lit from the old direction, at the old intensity.
            _sunLightingEnabled = lighting.terrainLightingEnabled;
            _sunIntensity = lighting.sunIntensity;
            _sunAmbient = lighting.ambientIntensity;
            _resolvedSunDir = lighting.sunDir;
            // The terrain surface is what this lights, and it exists whenever the stack draws one:
            // baked under a drape, or the shared ground pass when the drape is off. Gating on the
            // drape alone left the ground AND the hillshade paint over it unlit - and with them the
            // shadow map, since the shadow multiplies the lit colour (the paint is drawn from this
            // layer's own pass, which runs after the owner has set the stack's sun, so it saw the
            // value this line computes).
            if ((drapeFills || _terrainGroundActive) && lighting.terrainLightingEnabled) {
                terrainLighting.enabled = true;
                terrainLighting.sunDir = lighting.sunDir;
                terrainLighting.sunColor = cglib::vec3<float>(lighting.sunColor.getR() / 255.0f, lighting.sunColor.getG() / 255.0f, lighting.sunColor.getB() / 255.0f);
                terrainLighting.sunIntensity = lighting.sunIntensity;
                terrainLighting.ambientIntensity = lighting.ambientIntensity;
            }

            // Distance fog, lit by the same sun as the ground (see resolveFog). Metric in the API
            // and in the style, internal units in the renderer: the conversion is the equator one,
            // the same the shadow distance uses.
            ResolvedFog fog = resolveFog(options->getTerrainOptions(), _styleEnvironment, lighting);
            double metersToInternal = static_cast<double>(Const::WORLD_SIZE) / Const::EARTH_CIRCUMFERENCE;
            tileRenderer->setFog(vt::Color(fog.color.getR() / 255.0f, fog.color.getG() / 255.0f, fog.color.getB() / 255.0f, fog.color.getA() / 255.0f),
                                 static_cast<float>(fog.startDistance * metersToInternal),
                                 static_cast<float>(fog.distance * metersToInternal));
        }
        tileRenderer->setTerrainLighting(terrainLighting);
        tileRenderer->setTerrainDepthWrite(terrainMode && _terrainDepthWriteMode);
        tileRenderer->setDebugWireframe(false); // debug: terrain mesh wireframe + stencil overlay
        tileRenderer->setDebugSurfacePrefill(false); // debug: facing-coded terrain pre-fill (magenta front / cyan back)
        // The terrain base fill (color or the map background bitmap) is rendered
        // globally by MapRenderer BEFORE all tile layers, so it stays visible behind
        // translucent tile layer content regardless of the layer stacking order.
        // The per-layer surface pre-pass here stays depth-only.
        tileRenderer->setTerrainBackgroundColor(vt::Color());
        updateLabelOcclusionTest(tileRenderer, viewState, activeTerrainOptions);


        _mapRotation = viewState.getRotation();
        _viewDir = cglib::unit(viewState.getFocusPosNormal());
        if (auto options = _options.lock()) {
            MapPos internalFocusPos = viewState.getProjectionSurface()->calculateMapPos(viewState.getFocusPos());
            _mainLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, options->getMainLightDirection())));
            // 3D extrusions are lit by this direction. With terrain lighting on, the whole map is
            // supposed to answer to one sun, so the sun replaces the legacy fixed main light -
            // otherwise buildings stay lit from a direction that has nothing to do with the hour.
            // The RESOLVED sun, not LightOptions': a style may set the azimuth, altitude or
            // intensity, and reading the options here left the buildings on a different sun from
            // the ground they stand on.
            if (_sunLightingEnabled) {
                _mainLightDir = _resolvedSunDir;
            }
            MapVec normalIlluminationDir = options->getMainLightDirection();
            if (_normalIlluminationDirection != MapVec(0,0,0)) {
                normalIlluminationDir = _normalIlluminationDirection;
            }
            if (_normalIlluminationMapRotationEnabled) {
                double y = normalIlluminationDir.getY();
                double x = normalIlluminationDir.getX();
                // Compass azimuth (0 = north, clockwise) of the horizontal part, counter-rotated by
                // the map rotation so the light stays anchored to the viewport. The horizontal
                // length is preserved - the previous acos(y) form assumed a unit xy and silently
                // rewrote the horizontal/vertical balance of any other direction.
                double xyLength = std::sqrt(x * x + y * y);
                double azimuthal = std::atan2(x, y) * Const::RAD_TO_DEG - _mapRotation;
                double sin = std::sin(azimuthal * Const::DEG_TO_RAD) * xyLength;
                double cos = std::cos(azimuthal * Const::DEG_TO_RAD) * xyLength;
                normalIlluminationDir = MapVec(sin, cos, normalIlluminationDir.getZ());
            }

            _normalLightDir = cglib::vec3<float>::convert(cglib::unit(viewState.getProjectionSurface()->calculateVector(internalFocusPos, normalIlluminationDir)));
        }

        bool refresh = false;
        try {
            refresh = prepareFrameUnsafe(deltaSeconds, viewState);

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

        // The frame ends here regardless of what follows, so clear the prepare latch up front:
        // leaking it past an early return would make every later frame skip startFrame.
        _framePrepared = false;

        if (!_vtRenderer) {
            return false;
        }
        std::shared_ptr<vt::GLTileRenderer> tileRenderer = _vtRenderer->getTileRenderer();
        if (!tileRenderer) {
            return false;
        }

        bool refresh = false;
        try {
            VT_STAT_CLOCK(passClock);
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(true, false);
            }
            VT_STAT_SPLIT(pass3DLabels2DNs, passClock);
            if (_buildingOrder == 1) {
                tileRenderer->renderGeometry(false, true);
            }
            VT_STAT_SPLIT(pass3DGeometryNs, passClock);
            if (_labelOrder == 1) {
                tileRenderer->renderLabels(false, true);
            }
            VT_STAT_SPLIT(pass3DLabels3DNs, passClock);

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

    void TileRenderer::setStyleEnvironment(const StyleEnvironment& env) {
        std::lock_guard<std::mutex> lock(_mutex);

        _styleEnvironment = env;
    }

    float TileRenderer::evaluateFloatFunc(const vt::FloatFunction& floatFunc, const ViewState& viewState) {
        cglib::mat4x4<double> modelViewMat = viewState.getModelviewMat();
        vt::ViewState vtViewState(viewState.getProjectionMat(), modelViewMat, viewState.getZoom(), viewState.getRotation(), viewState.getTilt(), viewState.getAspectRatio(), viewState.getNormalizedResolution());
        return floatFunc(vtViewState);
    }

    float TileRenderer::getTerrainContentDepthShift() {
#ifdef __ANDROID__
        static const float depthShift = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.depthshift", property) > 0) {
                return static_cast<float>(std::atof(property));
            }
            return 0.0f;
        }();
        return depthShift;
#else
        return 0.0f;
#endif
    }

    int TileRenderer::terrainPaintDetailLevels() {
#ifdef __ANDROID__
        // adb shell setprop debug.carto.paintdetail 0|1|2 - elevation levels beyond the mesh cap.
        static const int levels = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.paintdetail", property) > 0) {
                int value = std::atoi(property);
                if (value >= 0 && value <= 4) {
                    return value;
                }
            }
            return DEFAULT_PAINT_DETAIL_LEVELS;
        }();
        return levels;
#else
        return DEFAULT_PAINT_DETAIL_LEVELS;
#endif
    }

    bool TileRenderer::isTerrainPaintFullDetailAllowed() {
#ifdef __ANDROID__
        static const bool allowed = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            return !(__system_property_get("debug.carto.paintdetail", property) > 0 && property[0] == '0');
        }();
        return allowed;
#else
        return true;
#endif
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
                    float occlusionTolerance = 1.0f + std::max(MIN_OCCLUSION_TOLERANCE, terrainOptions->getBillboardOcclusionTolerance());
                    tileRenderer->setLabelOcclusionTest([mapRendererWeak, mvpMat, screenWidth, screenHeight, occlusionTolerance](const cglib::vec3<double>& pos) {
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
                        // Farthest terrain depth around the anchor rather than the depth of its own
                        // pixel: labels drawn on the ground sit exactly ON the terrain, the depth
                        // buffer is read back downscaled and one frame late, and on a slope the
                        // neighbouring pixel can be a good deal nearer - so an exact comparison
                        // makes a label's own ground occlude it, differently on every frame, which
                        // is what made labels blink while panning.
                        float depthW = terrainRenderer->getDepthW(screenX, screenY);
                        if (depthW < std::numeric_limits<float>::max()) {
                            for (int i = 0; i < 4; i++) {
                                float dx = (i & 1 ? OCCLUSION_SAMPLE_OFFSET : -OCCLUSION_SAMPLE_OFFSET);
                                float dy = (i & 2 ? OCCLUSION_SAMPLE_OFFSET : -OCCLUSION_SAMPLE_OFFSET);
                                float neighbourDepthW = terrainRenderer->getDepthW(screenX + dx, screenY + dy);
                                if (neighbourDepthW < std::numeric_limits<float>::max()) {
                                    depthW = std::max(depthW, neighbourDepthW);
                                }
                            }
                        }
                        // Occluded if clearly behind the terrain there. The tolerance is relative to
                        // distance: at its default it only absorbs the mismatch between the anchor
                        // and the terrain it sits on, and raising it lets partly hidden features
                        // label (the peak-finder case).
                        return static_cast<float>(clipPos(3)) > depthW * occlusionTolerance;
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

        // The ray path lifts the target above the anchor by the same relative tolerance, so
        // both occlusion paths answer the same question.
        double rayTolerance = 0.005 + 0.5 * terrainOptions->getBillboardOcclusionTolerance();
        tileRenderer->setLabelOcclusionTest([state, elevationManager, cameraPos, rayTolerance](const cglib::vec3<double>& pos) -> bool {
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
            cglib::vec3<double> target = pos + cglib::vec3<double>(0, 0, dist * rayTolerance);
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
                    // The RESOLVED sun (style over options), captured by onDrawFrame. Reading
                    // LightOptions here ignored every sun property a style had set, so buildings
                    // and the ground they stand on disagreed about the hour.
                    float sunIntensity = (_sunLightingEnabled ? std::max(0.001f, _sunIntensity) : 0.0f);
                    float sunAmbient = (_sunLightingEnabled ? _sunAmbient : 0.35f);
                    glUniform2f(glGetUniformLocation(shaderProgram, "u_sunParams"), sunIntensity, sunAmbient);
                }
            });
            tileRenderer->setLightingShader3D(lightingShader3D);

            vt::GLTileRenderer::LightingShader lightingShaderNormalMap(false, _normalMapLightingShader, [this](GLuint shaderProgram, const vt::ViewState& viewState) {
                    // Straight (non-premultiplied) colors - the shader premultiplies them before
                    // mixing, which is the form MapLibre's hillshade fragment shader works in.
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_shadowColor"), _normalMapShadowColor.getR() / 255.0f, _normalMapShadowColor.getG() / 255.0f, _normalMapShadowColor.getB() / 255.0f, _normalMapShadowColor.getA() / 255.0f);
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_accentColor"), _normalMapAccentColor.getR() / 255.0f, _normalMapAccentColor.getG() / 255.0f, _normalMapAccentColor.getB() / 255.0f, _normalMapAccentColor.getA() / 255.0f);
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_highlightColor"), _normalMapHighlightColor.getR() / 255.0f, _normalMapHighlightColor.getG() / 255.0f, _normalMapHighlightColor.getB() / 255.0f, _normalMapHighlightColor.getA() / 255.0f);
                    glUniform3fv(glGetUniformLocation(shaderProgram, "u_lightDir"), 1, _normalLightDir.data() );
                    glUniform1i(glGetUniformLocation(shaderProgram, "u_method"), (_hillshadeMethod));
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_exaggeration"), _hillshadeExaggeration);
                    // MapLibre's 'hillshade-exaggeration' (the slope response curve), fed from the
                    // layer's contrast. Kept separate from u_exaggeration, which scales the slope.
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_intensity"), _hillshadeIntensity);
                    // Elevation-encoded normal map + contour lines (opt-in). These uniforms have no
                    // effect unless the normal map was built with elevation encoding (see HillshadeRasterTileLayer).
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_elevationEncoded"), _normalMapElevationEncoded ? 1.0f : 0.0f);
                    glUniform2f(glGetUniformLocation(shaderProgram, "u_elevationDecode"), vt::NormalMapBuilder::ELEVATION_SCALE, vt::NormalMapBuilder::ELEVATION_OFFSET);
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_contrast"), _hillshadeIntensity);
                    glUniform4f(glGetUniformLocation(shaderProgram, "u_contourColor"), _normalMapContourColor.getR() / 255.0f, _normalMapContourColor.getG() / 255.0f, _normalMapContourColor.getB() / 255.0f, _normalMapContourColor.getA() / 255.0f);
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_contourInterval"), _normalMapContourInterval);
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_contourWidth"), _normalMapContourWidth);
                    // Current fractional map zoom, for per-zoom custom normal-map shaders (getMapZoom()).
                    glUniform1f(glGetUniformLocation(shaderProgram, "u_zoom"), viewState.zoom);
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
        uniform vec2 u_sunParams; // x = sun intensity (0 = legacy lighting), y = ambient
        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, highp_opt float height, bool sideVertex) {
            lowp vec3 baseColor = sideVertex ? color.rgb * (1.0 - 0.5 / (1.0 + height * height)) : color.rgb;
            if (u_sunParams.x > 0.0) {
                // Sun lighting: roofs AND walls answer to the light. The legacy path lit roofs by
                // the VIEW direction, so from above - where roofs are most of what you see - the
                // buildings did not react to the sun at all. Same normalised Lambert as the
                // terrain surface, so the two agree.
                mediump float ndl = max(0.0, dot(normal, u_lightDir));
                mediump float lit = u_sunParams.y + (1.0 - u_sunParams.y) * ndl * u_sunParams.x;
                // No u_lightColor here: that is the legacy main-light tint and it darkens the
                // result well below the terrain lit by the same formula. The two must match.
                return vec4(baseColor * lit, color.a);
            }
            if (sideVertex) {
                mediump vec3 lighting = max(0.0, dot(normal, u_lightDir)) * u_lightColor.rgb + u_ambientColor.rgb;
                return vec4(baseColor * lighting, color.a);
            }
            mediump float lighting = max(0.0, dot(normal, u_viewDir)) * 0.5 + 0.5;
            return vec4(baseColor * lighting, color.a);
        }
    )GLSL";

    const std::string TileRenderer::LIGHTING_SHADER_NORMALMAP = R"GLSL(
        uniform vec4 u_shadowColor;
        uniform vec4 u_highlightColor;
        uniform vec4 u_accentColor;
        uniform vec3 u_lightDir;
        uniform int u_method;
        // Vertical relief multiplier applied to the slope (HillshadeRasterTileLayer exaggeration).
        uniform float u_exaggeration;
        // MapLibre's 'hillshade-exaggeration': the slope response curve and the overall strength
        // (HillshadeRasterTileLayer contrast). Default 0.5, matching the MapLibre style spec.
        uniform float u_intensity;

        #define PI 3.141592653589793
        #define STANDARD 0
        #define COMBINED 1
        #define IGOR 2
        #define MULTIDIRECTIONAL 3
        #define BASIC 4

        // All algorithms below composite in premultiplied alpha (the renderer blends normal map
        // tiles with GL_ONE, GL_ONE_MINUS_SRC_ALPHA), so the straight colors coming in from the
        // uniforms are premultiplied first - as MapLibre does before its shader ever runs.
        vec4 premul(vec4 color) {
            return vec4(color.rgb * color.a, color.a);
        }

        float get_aspect(vec2 deriv) {
            return deriv.x != 0.0 ? atan(deriv.y, -deriv.x) : PI / 2.0 * (deriv.y > 0.0 ? 1.0 : -1.0);
        }

        // The GDAL-derived algorithms below scale the slope by the intensity, as MapLibre does
        // (deriv * u_exaggeration * 2.0 in its hillshade.fragment.glsl). standard_hillshade does
        // not - it feeds the intensity into its slope response curve instead.
        vec2 scale_deriv(vec2 deriv) {
            return deriv * u_intensity * 2.0;
        }

        // Based on GDALHillshadeIgorAlg()
        vec4 igor_hillshade(vec2 deriv_in, float azimuth) {
            vec2 deriv = scale_deriv(deriv_in);
            float aspect = get_aspect(deriv);
            float slope_strength = atan(length(deriv)) * 2.0/PI;
            float aspect_strength = 1.0 - abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            float shadow_strength = slope_strength * aspect_strength;
            float highlight_strength = slope_strength * (1.0-aspect_strength);
            return premul(u_shadowColor) * shadow_strength + premul(u_highlightColor) * highlight_strength;
        }

        // Port of MapLibre's hillshade.fragment.glsl. Kept line-for-line comparable so the two
        // renderers can be diffed against each other; the only deliberate difference is that the
        // Mercator scale correction (MapLibre's 'scaleFactor') is baked into the normal map by
        // NormalMapBuilder instead of being recomputed per fragment from a latitude range.
        vec4 standard_hillshade(vec2 deriv, float azimuth) {
            // We also multiply the slope by an arbitrary z-factor of 0.625
            float slope = atan(0.625 * length(deriv));
            float aspect = get_aspect(deriv);

            float intensity = u_intensity;

            // We scale the slope exponentially based on intensity, using the position of the
            // maximum return value of the shade function as the exponent
            float base = 1.875 - intensity * 1.75;
            float maxValue = 0.5 * PI;
            float scaledSlope = intensity != 0.5 ? ((pow(base, slope) - 1.0) / (pow(base, maxValue) - 1.0)) * maxValue : slope;

            // The accent color is calculated with the cosine of the slope while the shade color is
            // calculated with the sine, so that the accent color's rate of change eases in while
            // the shade color's eases out.
            float accent = cos(scaledSlope);
            // Both the accent and shade color are multiplied by a clamped intensity value so that
            // intensities >= 0.5 do not additionally affect the color values, while intensity
            // values < 0.5 make the overall color more transparent.
            vec4 accent_color = (1.0 - accent) * premul(u_accentColor) * clamp(intensity * 2.0, 0.0, 1.0);

            float shade = abs(mod((aspect + azimuth) / PI + 0.5, 2.0) - 1.0);
            vec4 shade_color = mix(premul(u_shadowColor), premul(u_highlightColor), shade) * sin(scaledSlope) * clamp(intensity * 2.0, 0.0, 1.0);

            return accent_color * (1.0 - shade_color.a) + shade_color;
        }

        // Based on GDALHillshadeAlg(). 'altitude' is the light's elevation above the horizon.
        vec4 basic_hillshade(vec2 deriv_in, float azimuth, float altitude) {
            vec2 deriv = scale_deriv(deriv_in);
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = (sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv));

            float shade = clamp(cang, 0.0, 1.0);
            if(shade > 0.5) {
                return premul(u_highlightColor) * (2.0*shade - 1.0);
            }
            return premul(u_shadowColor) * (1.0 - 2.0*shade);
        }

        // Based on GDALHillshadeMultiDirectionalAlg(): four lights at 225/270/315/360 degrees,
        // weighted by the aspect. The user azimuth is unused by design - only the altitude matters.
        // Note MapLibre instead averages basic_hillshade over its illumination-source arrays, which
        // degenerates to plain BASIC for the single light source this layer exposes; GDAL's version
        // is used here so the mode is actually multidirectional.
        vec4 multidirectional_hillshade(vec2 deriv_in, float altitude) {
            vec2 deriv = scale_deriv(deriv_in);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);
            float xx_plus_yy = dot(deriv, deriv);

            float shade;
            if (xx_plus_yy == 0.0) {
                shade = clamp(sin_alt, 0.0, 1.0);
            } else {
                float x = deriv.x;
                float y = deriv.y;
                // cos(225 deg) * cos(altitude), shared by the 225 and 315 degree lights
                float c225 = -0.70710678 * cos_alt;
                float val225 = sin_alt + (x - y) * c225;
                float val270 = sin_alt - x * cos_alt;
                float val315 = sin_alt + (x + y) * c225;
                float val360 = sin_alt - y * cos_alt;

                float weight225 = 0.5 * xx_plus_yy - x * y;
                float weight270 = x * x;
                float weight315 = xx_plus_yy - weight225;
                float weight360 = y * y;

                float cang = (max(0.0, val225) * weight225 + max(0.0, val270) * weight270 +
                              max(0.0, val315) * weight315 + max(0.0, val360) * weight360) / (xx_plus_yy * 2.0);
                shade = clamp(cang / sqrt(1.0 + xx_plus_yy), 0.0, 1.0);
            }

            if(shade > 0.5) {
                return premul(u_highlightColor) * (2.0*shade - 1.0);
            }
            return premul(u_shadowColor) * (1.0 - 2.0*shade);
        }

        // Based on GDALHillshadeCombinedAlg()
        vec4 combined_hillshade(vec2 deriv_in, float azimuth, float altitude) {
            vec2 deriv = scale_deriv(deriv_in);
            float cos_az = cos(azimuth);
            float sin_az = sin(azimuth);
            float cos_alt = cos(altitude);
            float sin_alt = sin(altitude);

            float cang = acos(clamp((sin_alt - (deriv.y*cos_az*cos_alt - deriv.x*sin_az*cos_alt)) / sqrt(1.0 + dot(deriv, deriv)), -1.0, 1.0));

            cang = clamp(cang, 0.0, PI/2.0);

            float shade = cang * atan(length(deriv)) * 4.0/PI/PI;
            float highlight = (PI/2.0-cang) * atan(length(deriv)) * 4.0/PI/PI;

            return premul(u_shadowColor)*shade + premul(u_highlightColor)*highlight;
        }

        vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity) {
            // Recover the height gradient from the perturbed normal. On a planar surface the
            // tangent frame flips x and y, so -normal.xy/normal.z gives (dh/dEast, dh/dNorth).
            // The y component is negated on top of that to match MapLibre, whose DEM texture has
            // north at v = 0 and therefore works with (dh/dEast, -dh/dNorth). Without it the
            // aspect is mirrored about the east-west axis and the light rotates the wrong way.
            vec2 deriv = vec2(-normal.x, normal.y) / max(normal.z, 0.001);

            // Extra vertical exaggeration, a CARTO addition with no MapLibre equivalent. At the
            // default of 1.0 the slope is left exactly as the normal map encoded it.
            deriv *= u_exaggeration;

            // u_lightDir is (sin(compassAzimuth), cos(compassAzimuth), -sin(altitude)): the
            // horizontal part points towards the light, z points down towards the ground.
            // MapLibre adds PI to the compass azimuth for every method, because 0 degrees is north
            // and the original shader was written to accept (-illuminationDirection - 90).
            float azimuth = atan(u_lightDir.x, u_lightDir.y) + PI;
            float altitude = asin(clamp(-u_lightDir.z, -1.0, 1.0));

            if (u_method == BASIC) {
                return basic_hillshade(deriv, azimuth, altitude);
            } else if (u_method == COMBINED) {
                return combined_hillshade(deriv, azimuth, altitude);
            } else if (u_method == IGOR) {
                return igor_hillshade(deriv, azimuth);
            } else if (u_method == MULTIDIRECTIONAL) {
                return multidirectional_hillshade(deriv, altitude);
            }
            // STANDARD (default)
            return standard_hillshade(deriv, azimuth);
        }
    )GLSL";

}
