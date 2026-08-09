#include "MapRenderer.h"
#include "components/Exceptions.h"
#include "components/Layers.h"
#include "components/ThreadWorker.h"
#include "core/MapPos.h"
#include "core/ScreenPos.h"
#include "core/ScreenBounds.h"
#include "graphics/Bitmap.h"
#include "layers/Layer.h"
#include "layers/TileLayer.h"
#include "layers/VectorLayer.h"
#include "layers/VectorTileLayer.h"
#include "projections/Projection.h"
#include "projections/ProjectionSurface.h"
#include "renderers/BillboardRenderer.h"
#include "renderers/MapRendererListener.h"
#include "renderers/RendererCaptureListener.h"
#include "renderers/RedrawRequestListener.h"
#include "renderers/TileRenderer.h"
#include "renderers/components/BillboardSorter.h"
#include "renderers/components/RayIntersectedElement.h"
#include "renderers/cameraevents/CameraPanEvent.h"
#include "renderers/cameraevents/CameraRotationEvent.h"
#include "renderers/cameraevents/CameraTiltEvent.h"
#include "renderers/cameraevents/CameraZoomEvent.h"
#include "renderers/drawdatas/BillboardDrawData.h"
#include "renderers/utils/GLContext.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/FrameBuffer.h"
#include "renderers/PostProcessEffect.h"
#include "renderers/TerrainRenderer.h"
#include "renderers/utils/TerrainDrapeCache.h"
#include "renderers/utils/TerrainShadowMap.h"

#include <chrono>
#include <set>
#include "core/MapTile.h"
#include "terrain/ElevationManager.h"
#include "renderers/utils/Shader.h"
#include "renderers/utils/Texture.h"
#include "renderers/workers/BillboardPlacementWorker.h"
#include "renderers/workers/VTLabelPlacementWorker.h"
#include "renderers/workers/CullWorker.h"
#include "utils/Const.h"
#include "utils/FrameProfiler.h"
#include "utils/Log.h"

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif
#include "utils/ThreadUtils.h"

#include <vt/RenderStats.h>

#include <algorithm>
#include <limits>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

#if CARTO_VT_RENDER_STATS
    namespace {
        // Diagnostic dump of the vt label/tile churn counters, compiled in together with the
        // counters themselves (see vt/RenderStats.h - CARTO_VT_RENDER_STATS is the only
        // switch). Everything except 'live' is a per-interval delta. Only called from the GL
        // thread, so the previous values need no synchronization; the counters themselves are
        // atomic because the placement worker and the tile threads also increment them.
        constexpr int RENDER_STATS_INTERVAL = 1000; // ms

        void logRenderStats() {
            using vt::RenderStats;

            static const int COUNT = 17;
            static std::chrono::steady_clock::time_point lastTime = std::chrono::steady_clock::now();
            static long long lastValues[COUNT] = { 0 };

            std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
            if (now - lastTime < std::chrono::milliseconds(RENDER_STATS_INTERVAL)) {
                return;
            }
            lastTime = now;

            const long long values[COUNT] = {
                RenderStats::visibleTileSetChanges.load(),
                RenderStats::tileSurfacesBuilt.load(),
                RenderStats::tileSurfacesInvalidated.load(),
                RenderStats::labelsAllocated.load(),
                RenderStats::labelElevationReanchors.load(),
                RenderStats::placementUpdates.load(),
                RenderStats::placementReanchorsNull.load(),
                RenderStats::placementReanchorsHidden.load(),
                RenderStats::placementReanchorsVisible.load(),
                RenderStats::snapPlacements.load(),
                RenderStats::snapPlacementsMoved.load(),
                RenderStats::labelMapRebuilds.load(),
                RenderStats::labelsReused.load(),
                RenderStats::cullWorkerUpdates.load(),
                RenderStats::tileRecalculations.load(),
                RenderStats::tileLayersSkipped.load(),
                RenderStats::placementSearches.load()
            };
            long long deltas[COUNT];
            for (int i = 0; i < COUNT; i++) {
                deltas[i] = values[i] - lastValues[i];
                lastValues[i] = values[i];
            }
            static long long lastPasses = 0, lastFlips = 0, lastCullerNs = 0;
            long long passes = RenderStats::cullerPasses.load();
            long long flips = RenderStats::cullerVisibilityFlips.load();
            long long cullerNs = RenderStats::cullerNs.load();
            long long deltaPasses = passes - lastPasses;
            long long deltaFlips = flips - lastFlips;
            long long deltaCullerNs = cullerNs - lastCullerNs;
            lastPasses = passes;
            lastFlips = flips;
            lastCullerNs = cullerNs;

            Log::Infof("RenderStats: cullUpd=%lld tileRecalc=%lld tileSkip=%lld tileSets=%lld labelMaps=%lld | surfBuilt=%lld surfInval=%lld | labelsAlloc=%lld reused=%lld live=%lld elevReanchor=%lld | placeUpd=%lld reNull=%lld reHidden=%lld reVisible=%lld search=%lld | snap=%lld snapMoved=%lld | cullPasses=%lld visFlips=%lld cullMs=%.2f",
                       deltas[13], deltas[14], deltas[15], deltas[0], deltas[11],
                       deltas[1], deltas[2],
                       deltas[3], deltas[12], RenderStats::labelsLive.load(), deltas[4],
                       deltas[5], deltas[6], deltas[7], deltas[8], deltas[16],
                       deltas[9], deltas[10], deltaPasses, deltaFlips, deltaCullerNs / 1.0e6);

            // Draw submission, per interval. geomDraws is the number that matters: the frame
            // cost of a style tracks it, not the index count next to it.
            static long long lastDraws = 0, lastIndices = 0, lastLabelDraws = 0, lastTiles = 0, lastStyleLayers = 0;
            long long draws = RenderStats::geometryDraws.load();
            long long indices = RenderStats::geometryIndices.load();
            long long labelDraws = RenderStats::labelDraws.load();
            long long tiles = RenderStats::renderTilesDrawn.load();
            long long styleLayers = RenderStats::styleLayersDrawn.load();
            static long long lastSurfaceDraws = 0, lastSurfaceIndices = 0;
            long long surfaceDraws = RenderStats::surfaceDraws.load();
            long long surfaceIndices = RenderStats::surfaceIndices.load();
            Log::Infof("RenderStats: geomDraws=%lld geomIndices=%lld labelDraws=%lld renderTiles=%lld styleLayers=%lld surfDraws=%lld surfIndices=%lld (per interval)",
                       draws - lastDraws, indices - lastIndices, labelDraws - lastLabelDraws,
                       tiles - lastTiles, styleLayers - lastStyleLayers,
                       surfaceDraws - lastSurfaceDraws, surfaceIndices - lastSurfaceIndices);
            lastSurfaceDraws = surfaceDraws; lastSurfaceIndices = surfaceIndices;

            static long long lastSurfSplit[7] = { 0 };
            const long long surfSplit[7] = {
                RenderStats::surfShadowDraws.load(), RenderStats::surfMaskDraws.load(),
                RenderStats::surfFillDraws.load(), RenderStats::surfBlitDraws.load(),
                RenderStats::surfDrapeDraws.load(), RenderStats::surfBackgroundDraws.load(),
                RenderStats::surfBitmapDraws.load()
            };
            Log::Infof("RenderStats: surfaces shadow=%lld mask=%lld fill=%lld blit=%lld drape=%lld background=%lld bitmap=%lld (per interval)",
                       surfSplit[0] - lastSurfSplit[0], surfSplit[1] - lastSurfSplit[1],
                       surfSplit[2] - lastSurfSplit[2], surfSplit[3] - lastSurfSplit[3],
                       surfSplit[4] - lastSurfSplit[4], surfSplit[5] - lastSurfSplit[5],
                       surfSplit[6] - lastSurfSplit[6]);
            for (int i = 0; i < 7; i++) {
                lastSurfSplit[i] = surfSplit[i];
            }
            static long long lastMaskNs = 0, lastDrapeNs = 0;
            long long maskNs = RenderStats::surfMaskNs.load();
            long long drapeNs = RenderStats::surfDrapeNs.load();
            Log::Infof("RenderStats: surfaces maskMs=%.1f drapeMs=%.1f (per interval)",
                       (maskNs - lastMaskNs) / 1.0e6, (drapeNs - lastDrapeNs) / 1.0e6);
            lastMaskNs = maskNs; lastDrapeNs = drapeNs;

            static long long lastLabelBuild = 0, lastLabelBatch = 0, lastLabelVerts = 0, lastLineLayouts = 0;
            long long labelBuild = RenderStats::labelVertexBuildNs.load();
            long long labelBatch = RenderStats::labelBatchNs.load();
            long long labelVerts = RenderStats::labelsDrawnVertices.load();
            long long lineLayouts = RenderStats::lineLayoutBuilds.load();
            Log::Infof("RenderStats: labels built=%lld lineLayouts=%lld buildMs=%.1f batchMs=%.1f (per interval)",
                       labelVerts - lastLabelVerts, lineLayouts - lastLineLayouts,
                       (labelBuild - lastLabelBuild) / 1.0e6,
                       (labelBatch - lastLabelBatch) / 1.0e6);
            lastLabelBuild = labelBuild; lastLabelBatch = labelBatch; lastLabelVerts = labelVerts;
            lastLineLayouts = lineLayouts;

            static long long lastPrep[4] = { 0 }, lastLabelSplit[2] = { 0 }, lastLabelXf = 0, lastLabelAttr = 0;
            const long long prep[4] = {
                RenderStats::prepTileBlendNs.load(), RenderStats::prepElevDirtyNs.load(),
                RenderStats::prepElevUpdateNs.load(), RenderStats::prepLabelBlendNs.load()
            };
            const long long labelSplit[2] = {
                RenderStats::labelPlacementNs.load(), RenderStats::labelLineBuildNs.load()
            };
            Log::Infof("RenderStats: prepare tileBlendMs=%.1f elevDirtyMs=%.1f elevUpdMs=%.1f labelBlendMs=%.1f | labelBuild placementMs=%.1f lineMs=%.1f transformMs=%.1f attribMs=%.1f (per interval)",
                       (prep[0] - lastPrep[0]) / 1.0e6, (prep[1] - lastPrep[1]) / 1.0e6,
                       (prep[2] - lastPrep[2]) / 1.0e6, (prep[3] - lastPrep[3]) / 1.0e6,
                       (labelSplit[0] - lastLabelSplit[0]) / 1.0e6, (labelSplit[1] - lastLabelSplit[1]) / 1.0e6,
                       (RenderStats::labelTransformNs.load() - lastLabelXf) / 1.0e6,
                       (RenderStats::labelAttribNs.load() - lastLabelAttr) / 1.0e6);
            lastLabelXf = RenderStats::labelTransformNs.load();
            lastLabelAttr = RenderStats::labelAttribNs.load();
            for (int i = 0; i < 4; i++) { lastPrep[i] = prep[i]; }
            for (int i = 0; i < 2; i++) { lastLabelSplit[i] = labelSplit[i]; }

            static long long lastPass3D[3] = { 0 };
            const long long pass3D[3] = {
                RenderStats::pass3DLabels2DNs.load(), RenderStats::pass3DGeometryNs.load(),
                RenderStats::pass3DLabels3DNs.load()
            };
            Log::Infof("RenderStats: pass3D labels2DMs=%.1f geometryMs=%.1f labels3DMs=%.1f (per interval)",
                       (pass3D[0] - lastPass3D[0]) / 1.0e6, (pass3D[1] - lastPass3D[1]) / 1.0e6,
                       (pass3D[2] - lastPass3D[2]) / 1.0e6);
            for (int i = 0; i < 3; i++) { lastPass3D[i] = pass3D[i]; }

            static long long lastEndFrame = 0, lastSwept = 0;
            long long endFrameNs = RenderStats::endFrameNs.load();
            long long swept = RenderStats::endFrameSwept.load();
            static long long lastMutexWait = 0;
            long long mutexWait = RenderStats::mutexWaitNs.load();
            static long long lastBakes = 0, lastBakeNs = 0, lastQueued = 0;
            long long bakes = RenderStats::drapeBakes.load();
            long long bakeNs = RenderStats::drapeBakeNs.load();
            long long queued = RenderStats::drapeBakeQueued.load();
            Log::Infof("RenderStats: drape bakes=%lld queued=%lld totalMs=%.1f msPerBake=%.1f (per interval)",
                       bakes - lastBakes, queued - lastQueued, (bakeNs - lastBakeNs) / 1.0e6,
                       (bakeNs - lastBakeNs) / 1.0e6 / std::max(1LL, bakes - lastBakes));
            lastBakes = bakes; lastBakeNs = bakeNs; lastQueued = queued;
            // The elevation texture pipeline, which is what extra DEM detail multiplies.
            static long long lastDem[6] = { 0 };
            const long long dem[6] = {
                RenderStats::demEncodes.load(), RenderStats::demBorderPatches.load(),
                RenderStats::demEncodeNs.load(), RenderStats::demUploads.load(),
                RenderStats::demUploadNs.load(), RenderStats::demPatchNs.load()
            };
            Log::Infof("RenderStats: dem encodes=%lld patches=%lld encodeMs=%.1f | uploads=%lld uploadMs=%.1f patchMs=%.1f | live=%lld resolved=%lld zoomGap=%lld (per interval)",
                       dem[0] - lastDem[0], dem[1] - lastDem[1], (dem[2] - lastDem[2]) / 1.0e6,
                       dem[3] - lastDem[3], (dem[4] - lastDem[4]) / 1.0e6, (dem[5] - lastDem[5]) / 1.0e6,
                       RenderStats::demTexturesLive.load(), RenderStats::demTexturesResolved.load(), RenderStats::demTileZoomGap.load());
            for (int i = 0; i < 6; i++) { lastDem[i] = dem[i]; }

            Log::Infof("RenderStats: endFrame ms=%.1f swept=%lld labelLockWaitMs=%.1f (per interval)",
                       (endFrameNs - lastEndFrame) / 1.0e6, swept - lastSwept,
                       (mutexWait - lastMutexWait) / 1.0e6);
            lastMutexWait = mutexWait;
            lastEndFrame = endFrameNs; lastSwept = swept;

            // Where one geometry draw goes, in microseconds. 'skips' are calls that set up and
            // then found the style invisible - they pay everything up to their bail-out point.
            static long long lastProgram = 0, lastTerrain = 0, lastStyle = 0, lastStyleEval = 0, lastCompile = 0, lastBind = 0, lastDraw = 0, lastSkips = 0, lastMisses = 0;
            long long program = RenderStats::geomProgramNs.load();
            long long terrain = RenderStats::geomTerrainNs.load();
            long long style = RenderStats::geomStyleNs.load();
            long long styleEval = RenderStats::geomStyleEvalNs.load();
            long long compile = RenderStats::geomCompileNs.load();
            long long bind = RenderStats::geomBindNs.load();
            long long draw = RenderStats::geomDrawNs.load();
            long long skips = RenderStats::geometrySkips.load();
            long long misses = RenderStats::geomCompileMisses.load();
            static long long lastProbe = 0;
            long long probe = RenderStats::geomProbeNs.load();
            long long deltaCalls = std::max(1LL, (draws - lastDraws) + (skips - lastSkips));
            Log::Infof("RenderStats: perDraw us probe=%.2f program=%.1f terrain=%.1f styleEval=%.1f styleUpload=%.1f compile=%.1f bind=%.1f draw=%.1f (calls=%lld skips=%lld vboMisses=%lld)",
                       (probe - lastProbe) / 1000.0 / deltaCalls,
                       (program - lastProgram) / 1000.0 / deltaCalls, (terrain - lastTerrain) / 1000.0 / deltaCalls,
                       (styleEval - lastStyleEval) / 1000.0 / deltaCalls, (style - lastStyle) / 1000.0 / deltaCalls,
                       (compile - lastCompile) / 1000.0 / deltaCalls,
                       (bind - lastBind) / 1000.0 / deltaCalls, (draw - lastDraw) / 1000.0 / deltaCalls,
                       deltaCalls, skips - lastSkips, misses - lastMisses);
            Log::Infof("RenderStats: geomCompileStale=%lld (cumulative)", RenderStats::geomCompileStale.load());
            lastProgram = program; lastTerrain = terrain; lastStyle = style;
            lastStyleEval = styleEval; lastCompile = compile; lastBind = bind; lastDraw = draw;
            lastSkips = skips; lastMisses = misses; lastProbe = probe;

            static long long lastLookups = 0, lastFuncMisses = 0, lastConstants = 0, lastParams = 0;
            long long lookups = RenderStats::styleFuncLookups.load();
            long long funcMisses = RenderStats::styleFuncMisses.load();
            long long constants = RenderStats::styleFuncConstants.load();
            long long params = RenderStats::styleParameters.load();
            static long long lastFuncEval = 0;
            long long funcEval = RenderStats::styleFuncEvalNs.load();
            static long long lastViewStates = 0;
            long long viewStates = RenderStats::viewStateChanges.load();
            Log::Infof("RenderStats: styleFuncs lookups=%lld misses=%lld constants=%lld | params/draw=%.1f evalUsPerDraw=%.1f evalUsPerMiss=%.2f viewStates=%lld",
                       lookups - lastLookups, funcMisses - lastFuncMisses, constants - lastConstants,
                       (params - lastParams) / (double) deltaCalls,
                       (funcEval - lastFuncEval) / 1000.0 / deltaCalls,
                       (funcEval - lastFuncEval) / 1000.0 / std::max(1LL, funcMisses - lastFuncMisses),
                       viewStates - lastViewStates);
            lastViewStates = viewStates;
            lastLookups = lookups; lastFuncMisses = funcMisses; lastConstants = constants; lastParams = params;
            lastFuncEval = funcEval;

            lastDraws = draws;
            lastIndices = indices;
            lastLabelDraws = labelDraws;
            lastTiles = tiles;
            lastStyleLayers = styleLayers;
        }
    }
#endif

    MapRenderer::MapRenderer(const std::shared_ptr<Layers>& layers, const std::shared_ptr<Options>& options) :
        _lastFrameTime(),
        _viewState(),
        _glResourceManager(),
        _cullWorker(std::make_shared<CullWorker>()),
        _cullThread(),
        _vtLabelPlacementWorker(std::make_shared<VTLabelPlacementWorker>()),
        _vtLabelPlacementThread(),
        _optionsListener(),
        _screenBoundFBOs(),
        _screenFrameBuffers(),
        _screenBlendShader(),
        _backgroundRenderer(*options, *layers),
        _skyRenderer(*options),
        _billboardDrawDatas(),
        _billboardDrawDataBuffer(),
        _billboardPlacementWorker(std::make_shared<BillboardPlacementWorker>()),
        _billboardPlacementThread(),
        _animationHandler(*this),
        _kineticEventHandler(*this, *options),
        _layers(layers),
        _options(options),
        _surfaceCreated(false),
        _surfaceChanged(false),
        _billboardsChanged(false),
        _redrawPending(false),
        _redrawRequestListener(),
        _mapRendererListener(),
        _rendererCaptureListeners(),
        _rendererCaptureListenersMutex(),
        _onChangeListeners(),
        _onChangeListenersMutex(),
        _mutex()
    {
    }
        
    MapRenderer::~MapRenderer() {
    }
        
    void MapRenderer::init() {
        _cullWorker->setComponents(shared_from_this(), _cullWorker);
        _cullThread = std::thread(std::ref(*_cullWorker));

        _vtLabelPlacementWorker->setComponents(shared_from_this(), _vtLabelPlacementWorker);
        _vtLabelPlacementThread = std::thread(std::ref(*_vtLabelPlacementWorker));

        _billboardPlacementWorker->setComponents(shared_from_this(), _billboardPlacementWorker);
        _billboardPlacementThread = std::thread(std::ref(*_billboardPlacementWorker));
        
        _optionsListener = std::make_shared<OptionsListener>(shared_from_this());
        _options->registerOnChangeListener(_optionsListener);
    }

    void MapRenderer::deinit() {
        _options->unregisterOnChangeListener(_optionsListener);
        _optionsListener.reset();
        
        _cullWorker->stop();
        _cullThread.detach();

        _vtLabelPlacementWorker->stop();
        _vtLabelPlacementThread.detach();
        
        _billboardPlacementWorker->stop();
        _billboardPlacementThread.detach();
    }
        
    std::shared_ptr<RedrawRequestListener> MapRenderer::getRedrawRequestListener() const {
         return _redrawRequestListener.get();
    }
        
    void MapRenderer::setRedrawRequestListener(const std::shared_ptr<RedrawRequestListener>& listener) {
        _redrawRequestListener.set(listener);
    }
        
    std::shared_ptr<MapRendererListener> MapRenderer::getMapRendererListener() const {
        return _mapRendererListener.get();
    }

    void MapRenderer::setMapRendererListener(const std::shared_ptr<MapRendererListener>& listener) {
        _mapRendererListener.set(listener);
    }

    ViewState MapRenderer::getViewState() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        ViewState viewState = _viewState;
        viewState.calculateViewState(*_options);
        return viewState;
    }

    std::shared_ptr<ProjectionSurface> MapRenderer::getProjectionSurface() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        std::shared_ptr<ProjectionSurface> projectionSurface = _viewState.getProjectionSurface();
        if (!projectionSurface) {
            projectionSurface = _options->getProjectionSurface();
        }
        return projectionSurface;
    }
        
    // Call-site tally for requestRedraw. Requests come from every thread (tile workers, placement
    // workers, the GL thread), so it is guarded; the cost is one short lock per request, against a
    // whole frame of work per request.
    static std::mutex redrawSourceMutex;
    static std::map<std::pair<const char*, int>, int> redrawSourceCounts;

    void MapRenderer::logRedrawSources() {
        std::map<std::pair<const char*, int>, int> counts;
        {
            std::lock_guard<std::mutex> lock(redrawSourceMutex);
            counts.swap(redrawSourceCounts);
        }
        std::vector<std::pair<int, std::pair<const char*, int> > > sorted;
        sorted.reserve(counts.size());
        for (auto it = counts.begin(); it != counts.end(); it++) {
            sorted.emplace_back(it->second, it->first);
        }
        std::sort(sorted.begin(), sorted.end(), [](const std::pair<int, std::pair<const char*, int> >& a, const std::pair<int, std::pair<const char*, int> >& b) {
            return a.first > b.first;
        });
        std::string summary;
        for (std::size_t i = 0; i < sorted.size() && i < 6; i++) {
            const char* file = sorted[i].second.first;
            const char* name = std::strrchr(file, '/');
            summary += (summary.empty() ? "" : ", ") + std::string(name ? name + 1 : file) + ":" + std::to_string(sorted[i].second.second) + " x" + std::to_string(sorted[i].first);
        }
        Log::Infof("MapRenderer: redraw requests by source - %s", summary.empty() ? "none" : summary.c_str());
    }

    void MapRenderer::requestRedraw(const char* callerFile, int callerLine) const {
        {
            std::lock_guard<std::mutex> lock(redrawSourceMutex);
            redrawSourceCounts[std::make_pair(callerFile, callerLine)]++;
        }

        DirectorPtr<RedrawRequestListener> redrawRequestListener = _redrawRequestListener;

        if (redrawRequestListener) {
            _redrawPending = true;
            redrawRequestListener->onRedrawRequested();
        }
    }
    
    void MapRenderer::captureRendering(const std::shared_ptr<RendererCaptureListener>& listener, bool waitWhileUpdating) {
        if (!listener) {
            throw NullArgumentException("Null listener");
        }

        {
            std::lock_guard<std::mutex> lock(_rendererCaptureListenersMutex);
            _rendererCaptureListeners.push_back(std::make_pair(DirectorPtr<RendererCaptureListener>(listener), waitWhileUpdating));
        }
        requestRedraw();
    }

    std::shared_ptr<Layers> MapRenderer::getLayers() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _layers;
    }

    std::shared_ptr<Options> MapRenderer::getOptions() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _options;
    }

    std::shared_ptr<GLResourceManager> MapRenderer::getGLResourceManager() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _glResourceManager;
    }

    std::vector<std::shared_ptr<BillboardDrawData> > MapRenderer::getBillboardDrawDatas() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _billboardDrawDatas;
    }

    AnimationHandler& MapRenderer::getAnimationHandler() {
        return _animationHandler;
    }
    
    KineticEventHandler& MapRenderer::getKineticEventHandler() {
        return _kineticEventHandler;
    }
    
    void MapRenderer::calculateCameraEvent(CameraPanEvent& cameraEvent, float durationSeconds, bool updateKinetic) {
        if (durationSeconds > 0) {
            if (cameraEvent.isUseDelta()) {
                _animationHandler.setPanDelta(cameraEvent.getPosDelta(), durationSeconds);
            } else {
                _animationHandler.setPanTarget(cameraEvent.getPos(), durationSeconds);
            }
    
            // Animation will start on the next frame
            requestRedraw();
            return;
        }
    
        MapPos oldFocusPos;
        MapPos newFocusPos;
        float zoom;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            std::shared_ptr<ProjectionSurface> projectionSurface = getProjectionSurface();

            oldFocusPos = projectionSurface->calculateMapPos(_viewState.getFocusPos());
        
            // Calculate new focusPos, cameraPos and upVec
            cameraEvent.calculate(*_options, _viewState);
    
            // Calculate parameters for kinetic events
            newFocusPos = projectionSurface->calculateMapPos(_viewState.getFocusPos());
            zoom = _viewState.getZoom();
          
            // In case of seamless panning horizontal teleport, offset the delta focus pos
            oldFocusPos.setX(oldFocusPos.getX() + _viewState.getHorizontalLayerOffsetDir() * Const::WORLD_SIZE);
        }
    
        // Delay updating the layers, because view state will be updated only after onDrawFrame is called
        viewChanged(true);
    
        if (updateKinetic) {
            _kineticEventHandler.setPanDelta(std::make_pair(oldFocusPos, newFocusPos), zoom);
        } 
    }
        
    void MapRenderer::calculateCameraEvent(CameraRotationEvent& cameraEvent, float durationSeconds, bool updateKinetic) {
        if (durationSeconds > 0) {
            float oldRotation;
            {
                std::lock_guard<std::recursive_mutex> lock(_mutex);
                oldRotation = _viewState.getRotation();
            }
            _animationHandler.setRotationTarget(cameraEvent.isUseDelta() ? oldRotation + cameraEvent.getRotationDelta() : cameraEvent.getRotation(), cameraEvent.isUseTarget() ? &cameraEvent.getTargetPos() : nullptr, durationSeconds);
    
            // Animation will start on the next frame
            requestRedraw();
            return;
        }

        MapPos focusPos;
        float deltaRotation;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            float oldRotation = _viewState.getRotation();
            
            // Calculate new focusPos, cameraPos and upVec
            cameraEvent.calculate(*_options, _viewState);
            
            // Calculate parameters for kinetic events
            float rotation = _viewState.getRotation();
            deltaRotation = rotation - oldRotation;

            focusPos = getProjectionSurface()->calculateMapPos(_viewState.getFocusPos());
        }
    
        // Delay updating the layers, because view state will be updated only after onDrawFrame is called
        viewChanged(true);
        
        if (updateKinetic) {
            _kineticEventHandler.setRotationDelta(deltaRotation, cameraEvent.isUseTarget() ? cameraEvent.getTargetPos() : focusPos);
        }
    }
        
    void MapRenderer::calculateCameraEvent(CameraTiltEvent& cameraEvent, float durationSeconds, bool updateKinetic) {
        if (durationSeconds > 0) {
            float oldTilt;
            {
                std::lock_guard<std::recursive_mutex> lock(_mutex);
                oldTilt = _viewState.getTilt();
            }
            _animationHandler.setTiltTarget(cameraEvent.isUseDelta() ? oldTilt + cameraEvent.getTiltDelta() : cameraEvent.getTilt(), durationSeconds);
    
            // Animation will start on the next frame
            requestRedraw();
            return;
        }
    
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            
            // Calculate new focusPos, cameraPos and upVec
            cameraEvent.calculate(*_options, _viewState);
        }
    
        // Delay updating the layers, because view state will be updated only after onDrawFrame is called
        viewChanged(true);
    }
    
    void MapRenderer::calculateCameraEvent(CameraZoomEvent& cameraEvent, float durationSeconds, bool updateKinetic) {
        if (durationSeconds > 0) {
            float oldZoom;
            {
                std::lock_guard<std::recursive_mutex> lock(_mutex);
                oldZoom = _viewState.getZoom();
            }
            _animationHandler.setZoomTarget(cameraEvent.isUseDelta() ? oldZoom + cameraEvent.getZoomDelta() : cameraEvent.getZoom(), cameraEvent.isUseTarget() ? &cameraEvent.getTargetPos() : nullptr, durationSeconds);
    
            // Animation will start on the next frame
            requestRedraw();
            return;
        }
    
        MapPos focusPos;
        float deltaZoom;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            float oldZoom = _viewState.getZoom();
            
            // Calculate new focusPos, cameraPos and upVec
            cameraEvent.calculate(*_options, _viewState);
            
            // Calculate parameters for kinetic events
            float zoom = _viewState.getZoom();
            deltaZoom = zoom - oldZoom;

            focusPos = getProjectionSurface()->calculateMapPos(_viewState.getFocusPos());
        }
    
        // Delay updating the layers, because view state will be updated only after onDrawFrame is called
        viewChanged(true);
        
        if (updateKinetic) {
            _kineticEventHandler.setZoomDelta(deltaZoom, cameraEvent.isUseTarget() ? cameraEvent.getTargetPos() : focusPos);
        }
    }
    
    void MapRenderer::moveToFitBounds(const MapBounds& mapBounds, const ScreenBounds& screenBounds, bool integerZoom, bool resetTilt, bool resetRotation, float durationSeconds) {
        CameraPanEvent cameraPanEvent;
        CameraRotationEvent cameraRotationEvent;
        CameraTiltEvent cameraTiltEvent;
        CameraZoomEvent cameraZoomEvent;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            std::shared_ptr<ProjectionSurface> projectionSurface = getProjectionSurface();

            // Find center position
            cglib::vec3<double> centerPos(0, 0, 0);
            {
                cglib::vec3<double> minPos = projectionSurface->calculatePosition(mapBounds.getMin());
                cglib::vec3<double> maxPos = projectionSurface->calculatePosition(mapBounds.getMax());
                cglib::mat4x4<double> transform = projectionSurface->calculateTranslateMatrix(minPos, maxPos, 0.5);
                centerPos = cglib::transform_point(minPos, transform);
                if (std::isnan(cglib::norm(centerPos))) {
                    centerPos = cglib::vec3<double>(0, 0, 0);
                }
            }
            
            // Adjust the camera tilt, rotation and position to the final state of this animation
            cglib::vec3<double> focusPos = centerPos;
            cglib::vec3<double> oldFocusPos = _viewState.getFocusPos();
            cameraPanEvent.setKeepRotation(true);
            cameraPanEvent.setPos(projectionSurface->calculateMapPos(centerPos));
            cameraPanEvent.calculate(*_options, _viewState);
            
            float rotation = 0;
            float oldRotation = _viewState.getRotation();
            if (resetRotation) {
                cameraRotationEvent.setRotation(0);
                cameraRotationEvent.calculate(*_options, _viewState);
            }
    
            float oldTilt = _viewState.getTilt();
            float tilt = 90;
            if (resetTilt) {
                cameraTiltEvent.setKeepRotation(true);
                cameraTiltEvent.setTilt(90);
                cameraTiltEvent.calculate(*_options, _viewState);
            }
            
            // Use binary search to determine what the zoom level of the final state should be, so that all the points
            // would fit in the view
            float oldZoom = _viewState.getZoom();
            MapRange zoomRange(_options->getZoomRange());
            float zoom = _options->getZoomRange().getMin();
            float zoomStep = zoomRange.length() * 0.5f;
            if (mapBounds.getMin() == mapBounds.getMax()) {
                zoom = oldZoom;
                zoomStep = 0;
            }

            // Hack: if view size is zero (view size not known), use given screen bounds for view dimensions
            ViewState viewState(_viewState);
            if (viewState.getWidth() == 0 && viewState.getHeight() == 0) {
                int width = static_cast<int>(screenBounds.getMax().getX() - screenBounds.getMin().getX());
                int height = static_cast<int>(screenBounds.getMax().getY() - screenBounds.getMin().getY());
                Log::Warnf("MapRenderer::moveToFitBounds: Screen size not known yet, using %d, %d", width, height);
                viewState.setScreenSize(width, height);
                viewState.calculateViewState(*_options);
            }

            for (int i = 0; i < 24; i++) {
                cameraZoomEvent.setKeepRotation(true);
                cameraZoomEvent.setZoom(zoom + zoomStep);
                cameraZoomEvent.calculate(*_options, viewState);
                viewState.clampZoom(*_options);

                ScreenPos screenPos = screenBounds.getCenter();
                cglib::vec3<double> pos = viewState.screenToWorld(cglib::vec2<float>(screenPos.getX(), screenPos.getY()), 0, _options);
                if (std::isnan(cglib::norm(pos))) {
                    Log::Error("MapRenderer::moveToFitBounds: Failed to translate screen position!");
                    return;
                }

                cglib::mat4x4<double> transform = projectionSurface->calculateTranslateMatrix(pos, focusPos, 1);
                focusPos = cglib::transform_point(centerPos, transform);
                cameraPanEvent.setPos(projectionSurface->calculateMapPos(focusPos));
                cameraPanEvent.calculate(*_options, viewState);
                viewState.clampFocusPos(*_options);
    
                bool fit = true;
                for (int j = 0; j < 4; j++) {
                    MapPos mapPos(j & 1 ? mapBounds.getMax().getX() : mapBounds.getMin().getX(), j & 2 ? mapBounds.getMax().getY() : mapBounds.getMin().getY());
                    cglib::vec2<float> screenPos = viewState.worldToScreen(projectionSurface->calculatePosition(mapPos), _options);
                    if (!screenBounds.contains(ScreenPos(screenPos(0), screenPos(1)))) {
                        fit = false;
                        break;
                    }
                    cglib::vec3<double> normal = projectionSurface->calculateNormal(mapPos);
                    if (cglib::dot_product(normal, _viewState.getCameraPos() - projectionSurface->calculatePosition(mapPos)) < 0) {
                        fit = false;
                        break;
                    }
                }
                if (fit) {
                    zoom += zoomStep;
                }
                zoomStep /= 2;
            }
            
            if (integerZoom) {
                zoom = (float) std::floor(zoom);
            }
            
            // Reset the camera position, rotation tilt and zoom to the starting state of this animation
            // And then animate them to the final state over time, if needed
            cameraPanEvent.setPos(projectionSurface->calculateMapPos(oldFocusPos));
            cameraPanEvent.calculate(*_options, _viewState);
            cameraPanEvent.setPos(projectionSurface->calculateMapPos(focusPos));
            
            if (resetRotation) {
                cameraRotationEvent.setRotation(oldRotation);
                cameraRotationEvent.calculate(*_options, _viewState);
                cameraRotationEvent.setTargetPos(projectionSurface->calculateMapPos(focusPos));
                cameraRotationEvent.setRotation(rotation);
            }
            
            if (resetTilt) {
                cameraTiltEvent.setTilt(oldTilt);
                cameraTiltEvent.calculate(*_options, _viewState);
                cameraTiltEvent.setTilt(tilt);
            }
            
            cameraZoomEvent.setZoom(oldZoom);
            cameraZoomEvent.calculate(*_options, _viewState);
            cameraZoomEvent.setTargetPos(projectionSurface->calculateMapPos(focusPos));
            cameraZoomEvent.setZoom(zoom);
        }
        
        // Animate the view
        calculateCameraEvent(cameraPanEvent, durationSeconds, false);
        if (resetRotation) {
            calculateCameraEvent(cameraRotationEvent, durationSeconds, false);
        }
        if (resetTilt) {
            calculateCameraEvent(cameraTiltEvent, durationSeconds, false);
        }
        calculateCameraEvent(cameraZoomEvent, durationSeconds, false);
    }
    
    void MapRenderer::onSurfaceCreated() {
        ThreadUtils::SetThreadPriority(ThreadPriority::MAXIMUM);

        GLContext::LoadExtensions();

        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        // One-time GL context diagnostics: the depth/stencil resolution and the vertex
        // texture unit count determine which terrain depth model is in effect and how
        // much depth slack it actually has - essential when debugging device-specific
        // terrain occlusion issues (emulator and device configs often differ).
        {
            GLint depthBits = 0, stencilBits = 0, maxVertexTextureUnits = 0;
            glGetIntegerv(GL_DEPTH_BITS, &depthBits);
            glGetIntegerv(GL_STENCIL_BITS, &stencilBits);
            glGetIntegerv(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, &maxVertexTextureUnits);
            const GLubyte* renderer = glGetString(GL_RENDERER);
            Log::Infof("MapRenderer::onSurfaceCreated: renderer '%s', depth bits %d, stencil bits %d, vertex texture units %d",
                renderer ? reinterpret_cast<const char*>(renderer) : "?", depthBits, stencilBits, maxVertexTextureUnits);
        }

        // If the surface was lost, properly signal about this
        if (_surfaceCreated) {
            onSurfaceDestroyed();
        }
        _surfaceCreated = true;
        _surfaceChanged = true; // should not be needed, do it in any case

        // Reset resource manager
        if (_glResourceManager) {
            _glResourceManager->setGLThreadId(std::thread::id());
        }
        _glResourceManager = std::make_shared<GLResourceManager>();
        _glResourceManager->setGLThreadId(std::this_thread::get_id());

        // Reset screen blending state
        _screenBoundFBOs.clear();
        _screenFrameBuffers.clear();
        _screenBlendShader.reset();

        // Notify renderers about the event
        _backgroundRenderer.onSurfaceCreated(_glResourceManager);
        _skyRenderer.onSurfaceCreated(_glResourceManager);

        GLContext::CheckGLError("MapRenderer::onSurfaceCreated");
    }

    void MapRenderer::onSurfaceChanged(int width, int height) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _viewState.setScreenSize(width, height);
            _viewState.calculateViewState(*_options);
            _viewState.clampZoom(*_options);
            _viewState.clampFocusPos(*_options);
            _screenFrameBuffers.clear(); // reset, as this depends on the surface dimensions
            _surfaceChanged = true;
        }

        DirectorPtr<MapRendererListener> mapRendererListener = _mapRendererListener;
        if (mapRendererListener) {
            mapRendererListener->onSurfaceChanged(width, height);
        }
    }
    
    void MapRenderer::onDrawFrame() {
        if (!_surfaceCreated) {
            Log::Error("MapRenderer::onDrawFrame: Surface not yet created");
            return;
        }

        _redrawPending = false;

        std::vector<std::shared_ptr<OnChangeListener> > onChangeListeners;
        {
            std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
            onChangeListeners = _onChangeListeners;
        }

        DirectorPtr<MapRendererListener> mapRendererListener = _mapRendererListener;

        // Re-set GL thread ids, Windows Phone needs this as onSurfaceCreate/onSurfaceChange may be called from different threads
        _glResourceManager->setGLThreadId(std::this_thread::get_id());

        // Create pending resources
        _glResourceManager->processResources();

        // Check if surface has changed
        if (_surfaceChanged.exchange(false)) {
            int width = 0, height = 0;
            {
                std::lock_guard<std::recursive_mutex> lock(_mutex);
                width = _viewState.getWidth();
                height = _viewState.getHeight();
            }
            glViewport(0, 0, width, height);

            _kineticEventHandler.stopPan();
            _kineticEventHandler.stopRotation();
            _kineticEventHandler.stopZoom();
        
            _lastFrameTime.reset();

            // Perform culling without delay
            viewChanged(false);
        }
        
        // Calculate time from the last frame
        std::chrono::steady_clock::time_point currentTime = std::chrono::steady_clock::now();
        float deltaSeconds = 1.0f / 60.0f;
        if (_lastFrameTime) {
            deltaSeconds = std::max(0.0f, std::chrono::duration_cast<std::chrono::duration<float> >(currentTime - *_lastFrameTime).count());
        }
        _lastFrameTime = currentTime;
    
        // Callback for synchronized rendering
        if (mapRendererListener) {
            mapRendererListener->onBeforeDrawFrame();
        }

        // Calculate camera params and make a synchronized copy of the view state
        ViewState viewState;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);

            // Terrain: extend view distances by the terrain height range and keep
            // the camera above the terrain surface.
            std::shared_ptr<ElevationManager> elevationManager;
            if (_options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
                if (auto terrainOptions = _options->getTerrainOptions()) {
                    if (terrainOptions->isEnabled()) {
                        elevationManager = terrainOptions->getElevationManager();
                    }
                }
            }
            if (elevationManager) {
                cglib::vec3<double> cameraPos = _viewState.getCameraPos();
                double minZ = 0, maxZ = 0;
                elevationManager->getDisplayHeightRange(cameraPos(1), minZ, maxZ);
                _viewState.setTerrainHeightRange(static_cast<float>(minZ), static_cast<float>(maxZ));

                // Note: the camera is deliberately NOT clamped above the terrain here.
                // ViewState maintains the invariant dist(camera, focus) == zoom0Distance/2^zoom;
                // mutating the camera position outside of the camera event system breaks it and
                // corrupts the view state. Flying the camera below terrain is a v1 limitation.

                // Refresh vector layers when the elevation data changes (debounced), so that
                // element draw data gets rebuilt with the new heights
                unsigned int elevationVersion = elevationManager->getVersion();
                if (elevationVersion != _layersElevationVersion) {
                    if (!_lastElevationRefreshTime || currentTime - *_lastElevationRefreshTime > std::chrono::milliseconds(ELEVATION_REFRESH_DELAY)) {
                        _layersElevationVersion = elevationVersion;
                        _lastElevationRefreshTime = currentTime;
                        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
                            if (std::dynamic_pointer_cast<VectorLayer>(layer)) {
                                layer->refresh();
                            }
                        }
                    } else {
                        requestRedraw(); // check again on the next frame
                    }
                }
            } else {
                _viewState.setTerrainHeightRange(0.0f, 0.0f);
            }

            _viewState.calculateViewState(*_options);
            viewState = _viewState;
            _viewState.setHorizontalLayerOffsetDir(0);
        }

        // Calculate map moving animations and kinetic events
        _animationHandler.calculate(viewState, deltaSeconds);
        _kineticEventHandler.calculate(viewState, deltaSeconds);

        // If a post-process effect is set, render the frame into an offscreen buffer
        std::shared_ptr<PostProcessEffect> postProcessEffect = getPostProcessEffect();
        if (postProcessEffect) {
            clearAndBindScreenFBO(_options->getClearColor(), true, false);
        }

        // Render everything
        FRAME_PROF_NOW(profFrameStart);
        FRAME_PROF_RESET();
        FRAME_PROF_GPU_BEGIN(SECTION_SKY);
        initializeRenderState();
        // The shader sky replaces the legacy sky band when it draws.
        bool skyDrawn = _skyRenderer.onDrawFrame(viewState);
        // Timed apart from the sky: both are full-screen-ish draws at the START of the frame, and
        // the first section of a frame also absorbs whatever the GPU idled waiting for the CPU
        // (see GpuFrameProfiler), so one number for the two says nothing about either.
        FRAME_PROF_GPU_BEGIN(SECTION_BACKGROUND);
        // Measurement switch: tangram draws no background geometry at all - their map background
        // is the framebuffer clear colour (core/src/map.cpp) - so this is what that would save.
        //   adb shell setprop debug.carto.background 0
        if (isBackgroundEnabled()) {
            _backgroundRenderer.onDrawFrame(viewState, !skyDrawn);
        }
        FRAME_PROF_ADD(skyMs, profFrameStart);
        drawLayers(deltaSeconds, viewState);
        FRAME_PROF_GPU_END();
        FRAME_PROF_END(profFrameStart);
        if (postProcessEffect) {
            applyPostProcessEffect(postProcessEffect, viewState);
        }

        // Callback for synchronized rendering
        if (mapRendererListener) {
            mapRendererListener->onAfterDrawFrame();
        }

        // Handle renderer capture callbacks as everything is rendered now
        handleRendererCaptureCallbacks();
        
        // Update billboard placements/visibility
        if (_billboardsChanged.exchange(false)) {
            _billboardPlacementWorker->init(BILLBOARD_PLACEMENT_TASK_DELAY);
        }
        
        // Call listener to inform we are idle now, if no redraw request is pending
        if (!_redrawPending) {
            for (const std::shared_ptr<OnChangeListener>& onChangeListener : onChangeListeners) {
                onChangeListener->onMapIdle();
            }
            _lastFrameTime.reset();
        }

#if CARTO_VT_RENDER_STATS
        logRenderStats();
#endif

        GLContext::CheckGLError("MapRenderer::onDrawFrame");
    }

#ifdef __ANDROID__
    bool MapRenderer::isBackgroundEnabled() {
        static const bool enabled = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            return !(__system_property_get("debug.carto.background", property) > 0 && property[0] == '0');
        }();
        return enabled;
    }
#else
    bool MapRenderer::isBackgroundEnabled() {
        return true;
    }
#endif

    void MapRenderer::onSurfaceDestroyed() {
        // This method may never be called (e.x Android)
        _surfaceCreated = false;

        // Reset resource manager. We tell managers to ignore all resource 'release' operations by invalidating manager thread ids
        if (_glResourceManager) {
            _glResourceManager->setGLThreadId(std::thread::id());
            _glResourceManager.reset();
        }

        // Reset screen blending state
        _screenBoundFBOs.clear();
        _screenFrameBuffers.clear();
        _screenBlendShader.reset();

        // Drop the terrain offscreen targets: their handles belong to the dying context, and a
        // recreated context would otherwise draw into and sample from stale names.
        _terrainDrapeCache.reset();
        _terrainShadowMap.reset();
        _shadowMapValid = false;

        // Notify renderers about the event
        _backgroundRenderer.onSurfaceDestroyed();
        _skyRenderer.onSurfaceDestroyed();
    }
    
    void MapRenderer::finishRendering() {
        glFinish();
    }
    
    void MapRenderer::clearAndBindScreenFBO(const Color& color, bool depth, bool stencil) {
        GLint prevBoundFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevBoundFBO);
        GLuint bufferMask = GL_COLOR_BUFFER_BIT | (depth ? GL_DEPTH_BUFFER_BIT : 0) | (stencil ? GL_STENCIL_BUFFER_BIT : 0);
        _screenBoundFBOs.emplace_back(static_cast<GLuint>(prevBoundFBO), bufferMask);

        std::shared_ptr<FrameBuffer>& frameBuffer = _screenFrameBuffers[bufferMask];
        if (!frameBuffer || !frameBuffer->isValid()) {
            frameBuffer = _glResourceManager->create<FrameBuffer>(_viewState.getWidth(), _viewState.getHeight(), true, depth, stencil);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer->getFBOId());

        glClearColor(color.getR() / 255.0f, color.getG() / 255.0f, color.getB() / 255.0f, color.getA() / 255.0f);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        if (depth) {
            glDepthMask(GL_TRUE);
        }
        if (stencil) {
            glStencilMask(255);
        }

        glClear(bufferMask);

        if (depth) {
            glDepthMask(GL_FALSE);
        }
        if (stencil) {
            glStencilMask(0);
        }

        GLContext::CheckGLError("MapRenderer::clearAndBindScreenFBO");
    }

    std::shared_ptr<PostProcessEffect> MapRenderer::getPostProcessEffect() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _postProcessEffect;
    }

    void MapRenderer::setPostProcessEffect(const std::shared_ptr<PostProcessEffect>& postProcessEffect) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            if (_postProcessEffect == postProcessEffect) {
                return;
            }
            _postProcessEffect = postProcessEffect;
            _postProcessStartTime = std::chrono::steady_clock::now();
        }
        requestRedraw();
    }

    void MapRenderer::applyPostProcessEffect(const std::shared_ptr<PostProcessEffect>& effect, const ViewState& viewState) {
        static const GLfloat screenVertices[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

        if (_screenBoundFBOs.empty()) {
            Log::Error("MapRenderer::applyPostProcessEffect: No bound FBOs");
            return;
        }

        // Optional terrain depth pre-pass (renders into its own FBO and restores the binding)
        GLuint terrainDepthTex = 0;
        if (effect->isTerrainDepthRequired()) {
            std::shared_ptr<TerrainOptions> terrainOptions;
            if (_options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
                terrainOptions = _options->getTerrainOptions();
            }
            if (terrainOptions && terrainOptions->isEnabled()) {
                if (!_terrainRenderer) {
                    _terrainRenderer = std::make_unique<TerrainRenderer>();
                }
                if (_terrainRenderer->renderDepthTexture(viewState, terrainOptions, _glResourceManager)) {
                    terrainDepthTex = _terrainRenderer->getDepthTextureId();
                }
            }
        }

        GLuint prevBoundFBO = _screenBoundFBOs.back().first;
        GLuint bufferMask = _screenBoundFBOs.back().second;
        _screenBoundFBOs.pop_back();

        std::shared_ptr<FrameBuffer>& frameBuffer = _screenFrameBuffers[bufferMask];
        if (!frameBuffer || !frameBuffer->isValid()) {
            return; // should not happen, just safety
        }
        if (bufferMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
            frameBuffer->discard(false, (bufferMask & GL_DEPTH_BUFFER_BIT) != 0, (bufferMask & GL_STENCIL_BUFFER_BIT) != 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, prevBoundFBO);

        // Compile the effect shader on demand
        if (!_postProcessShader || !_postProcessShader->isValid() || _postProcessShaderName != effect->getName()) {
            _postProcessShader = _glResourceManager->create<Shader>("postprocess_" + effect->getName(), POST_PROCESS_VERTEX_SHADER, effect->getFragmentShader());
            _postProcessShaderName = effect->getName();
        }
        if (!_postProcessShader) {
            return;
        }

        glDisable(GL_BLEND);

        GLuint progId = _postProcessShader->getProgId();
        glUseProgram(progId);

        glVertexAttribPointer(_postProcessShader->getAttribLoc("a_coord"), 2, GL_FLOAT, GL_FALSE, 0, screenVertices);
        glEnableVertexAttribArray(_postProcessShader->getAttribLoc("a_coord"));

        // Effects declare only the uniforms they use, so query the locations directly
        GLint loc = glGetUniformLocation(progId, "uColorTex");
        if (loc >= 0) {
            glUniform1i(loc, 0);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, frameBuffer->getColorTexId());
        if (terrainDepthTex != 0 && (loc = glGetUniformLocation(progId, "uTerrainDepthTex")) >= 0) {
            glUniform1i(loc, 1);
            glActiveTexture(GL_TEXTURE1);
            glBindTexture(GL_TEXTURE_2D, terrainDepthTex);
            glActiveTexture(GL_TEXTURE0);
        }

        if ((loc = glGetUniformLocation(progId, "uInvScreenSize")) >= 0) {
            glUniform2f(loc, 1.0f / _viewState.getWidth(), 1.0f / _viewState.getHeight());
        }
        if ((loc = glGetUniformLocation(progId, "uNear")) >= 0) {
            glUniform1f(loc, viewState.getNear());
        }
        if ((loc = glGetUniformLocation(progId, "uFar")) >= 0) {
            glUniform1f(loc, viewState.getFar());
        }
        if ((loc = glGetUniformLocation(progId, "uTime")) >= 0) {
            float time = 0;
            if (_postProcessStartTime) {
                time = std::chrono::duration_cast<std::chrono::duration<float> >(std::chrono::steady_clock::now() - *_postProcessStartTime).count();
            }
            glUniform1f(loc, time);
        }

        for (const auto& param : effect->getFloatParameters()) {
            if ((loc = glGetUniformLocation(progId, param.first.c_str())) >= 0) {
                glUniform1f(loc, param.second);
            }
        }

        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindTexture(GL_TEXTURE_2D, 0);
        glDisableVertexAttribArray(_postProcessShader->getAttribLoc("a_coord"));
        glEnable(GL_BLEND);

        GLContext::CheckGLError("MapRenderer::applyPostProcessEffect");
    }

    void MapRenderer::blendAndUnbindScreenFBO(float opacity) {
        static const GLfloat screenVertices[8] = { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };

        if (_screenBoundFBOs.empty()) {
            Log::Error("MapRenderer::blendAndUnbindScreenFBO: No bound FBOs");
            return;
        }

        GLuint prevBoundFBO = _screenBoundFBOs.back().first;
        GLuint bufferMask = _screenBoundFBOs.back().second;
        _screenBoundFBOs.pop_back();
        
        std::shared_ptr<FrameBuffer>& frameBuffer = _screenFrameBuffers[bufferMask];
        if (!frameBuffer || !frameBuffer->isValid()) {
            return; // should not happen, just safety
        }
        if (bufferMask & (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT)) {
            frameBuffer->discard(false, (bufferMask & GL_DEPTH_BUFFER_BIT) != 0, (bufferMask & GL_STENCIL_BUFFER_BIT) != 0);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, prevBoundFBO);

        if (!_screenBlendShader || !_screenBlendShader->isValid()) {
            _screenBlendShader = _glResourceManager->create<Shader>("blend", BLEND_VERTEX_SHADER, BLEND_FRAGMENT_SHADER);
        }
        
        glUseProgram(_screenBlendShader->getProgId());

        glVertexAttribPointer(_screenBlendShader->getAttribLoc("a_coord"), 2, GL_FLOAT, GL_FALSE, 0, screenVertices);
        glEnableVertexAttribArray(_screenBlendShader->getAttribLoc("a_coord"));
        
        cglib::mat4x4<float> mvpMatrix = cglib::mat4x4<float>::identity();
        glUniformMatrix4fv(_screenBlendShader->getUniformLoc("u_mvpMat"), 1, GL_FALSE, mvpMatrix.data());
        
        glUniform1i(_screenBlendShader->getUniformLoc("u_tex"), 0);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, frameBuffer->getColorTexId());

        glUniform4f(_screenBlendShader->getUniformLoc("u_color"), opacity, opacity, opacity, opacity);
        glUniform2f(_screenBlendShader->getUniformLoc("u_invScreenSize"), 1.0f / _viewState.getWidth(), 1.0f / _viewState.getHeight());
        
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        
        glBindTexture(GL_TEXTURE_2D, 0);
        
        glDisableVertexAttribArray(_screenBlendShader->getAttribLoc("a_coord"));

        GLContext::CheckGLError("MapRenderer::blendAndUnbindScreenFBO");
    }

    void MapRenderer::setZBuffering(bool enable) {
        glDepthMask(enable ? GL_TRUE : GL_FALSE);
    }

    void MapRenderer::calculateRayIntersectedElements(const MapPos& targetPos, ViewState& viewState, std::vector<RayIntersectedElement>& results) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            viewState = _viewState;
        }
        if (!viewState.getProjectionSurface()) {
            return;
        }

        cglib::vec3<double> origin = viewState.getCameraPos();
        cglib::vec3<double> target = viewState.getProjectionSurface()->calculatePosition(targetPos);
        cglib::ray3<double> ray(origin, target - origin);
        calculateRayIntersectedElements(ray, viewState, results);
    }

    void MapRenderer::calculateRayIntersectedElements(const cglib::ray3<double>& ray, ViewState& viewState, std::vector<RayIntersectedElement>& results) {
        // Normal layer click detection is done in the layer order
        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
            layer->calculateRayIntersectedElements(ray, viewState, results);
        }
    }
     
    void MapRenderer::billboardsChanged() {
        _billboardsChanged = true;
    }

    void MapRenderer::vtLabelsChanged(const std::shared_ptr<Layer>& layer, bool delay) {
        _vtLabelPlacementWorker->init(layer, delay ? VT_LABEL_PLACEMENT_TASK_DELAY : 0);
    }
    
    void MapRenderer::layerChanged(const std::shared_ptr<Layer>& layer, bool delay) {
        // If screen size has been set, load the layers, otherwise wait for the onSurfaceChanged method
        // which will also start the cull worker
        if (_surfaceCreated) {
            int delayTime = layer->getCullDelay();
            _cullWorker->init(layer, delay ? delayTime : 0);
        }
    }
    
    void MapRenderer::viewChanged(bool delay) {
        std::shared_ptr<Layer> vectorTileLayer;
        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
            int delayTime = layer->getCullDelay();
            _cullWorker->init(layer, delay ? delayTime : 0);
            if (!vectorTileLayer && std::dynamic_pointer_cast<VectorTileLayer>(layer)) {
                vectorTileLayer = layer;
            }
        }

        // Label placement is SCREEN space, so it goes stale when the camera zooms even though the
        // tile set - which is what normally asks for a new pass (VectorTileLayer::calculateDrawData)
        // - has not changed. A label that had to fall back to its icon alone (shield-text-optional)
        // or to a worse side would then keep it until tiles happened to change, however much room
        // the zoom made for it. Panning and rotating leave every label's size alone, and both
        // already change the tile set, so only the zoom is worth a pass of its own.
        if (vectorTileLayer) {
            float zoom = getViewState().getZoom();
            if (std::abs(zoom - _lastLabelPlacementZoom) >= LABEL_PLACEMENT_ZOOM_THRESHOLD) {
                _lastLabelPlacementZoom = zoom;
                _vtLabelPlacementWorker->init(vectorTileLayer, VT_LABEL_PLACEMENT_TASK_DELAY);
            }
        }
    
        billboardsChanged();
    
        std::vector<std::shared_ptr<OnChangeListener> > onChangeListeners;
        {
            std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
            onChangeListeners = _onChangeListeners;
        }
        for (const std::shared_ptr<OnChangeListener>& onChangeListener : onChangeListeners) {
            onChangeListener->onMapChanged();
        }
        
        requestRedraw();
    }
    
    void MapRenderer::registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.push_back(listener);
    }

    void MapRenderer::unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener) {
        std::lock_guard<std::mutex> lock(_onChangeListenersMutex);
        _onChangeListeners.erase(std::remove(_onChangeListeners.begin(), _onChangeListeners.end(), listener), _onChangeListeners.end());
    }

    void MapRenderer::initializeRenderState() const {
        // Enable backface culling
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
    
        // Enable blending, use premultiplied alpha
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    
        // Disable dithering for better performance
        glDisable(GL_DITHER);
    
        // Enable depth testing, disable writing, set up clear color, etc
        Color clearColor = _options->getClearColor();
        glClearColor(clearColor.getR() / 255.0f, clearColor.getG() / 255.0f, clearColor.getB() / 255.0f, clearColor.getA() / 255.0f);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glDisable(GL_STENCIL_TEST);
        glStencilMask(255);
    
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        glDepthMask(GL_FALSE);
        glStencilMask(0);
    }
    
    // A cached shadow map is refreshed at least this often anyway: elevation tiles can stream in
    // without changing the light box or the caster tile list, and a shadow cast by data that has
    // since arrived would otherwise never appear.
    static const int SHADOW_MAP_MAX_AGE = 30;
    // Frames between two refreshes driven by newly arrived tile content.
    static const int SHADOW_MAP_CONTENT_INTERVAL = 4;
    // How far the extrusions may grow before the map is redrawn, in units of one tile's full
    // height: about a dozen refreshes over a whole fade, whatever the frame rate, instead of one
    // per frame.
    static const float SHADOW_MAP_FADE_STEP = 0.08f;

    // Caster-pass counters, cumulative since start: compared with the frame count they say how
    // much of the shadow cost the map cache is saving. File scope because the pass itself and the
    // periodic dump that prints them now live in different functions.
    static int shadowPasses = 0;
    static int shadowCasterDraws = 0;
    static int shadowExtrusionDraws = 0;
    static double shadowMsSum = 0;

    void MapRenderer::applyTerrainShadows(const std::vector<std::shared_ptr<TileLayer> >& tileLayers, const std::vector<vt::TileId>& coverTileIds, const std::shared_ptr<TerrainOptions>& terrainOptions, const ViewState& viewState, int prevFBO, bool contentChanged, bool castShadows, ResolvedLighting& lighting, std::array<double, TerrainShadowMap::MAX_CASCADES>& shadowTexelMeters) {
        // Directional shadows. The caster pass draws exactly the terrain surfaces
        // that are about to be drawn on screen, from the sun, into a packed-depth
        // texture; the surface shader then looks itself up in it. Casters and
        // receivers share one vertex shader and one elevation fetch, so the shadow
        // geometry cannot disagree with the rendered geometry.
        float shadowStrength = 0.0f;
        unsigned int shadowTexture = 0;
        int shadowMapSize = 0, shadowCascades = 1;
        float shadowSoftness = 1.0f;
        
        std::array<float, TerrainShadowMap::MAX_CASCADES> shadowBiases = { };
        std::array<cglib::mat4x4<double>, TerrainShadowMap::MAX_CASCADES> lightViewProjs;
        lightViewProjs.fill(cglib::mat4x4<double>::identity());
        // The styles get a say in every light and shadow property; whatever they do
        // not mention stays with LightOptions. The first layer to define a property
        // wins, and the values are re-read every frame so they may follow the zoom.
        StyleEnvironment styleEnvironment;
        for (const std::shared_ptr<TileLayer>& tileLayer : tileLayers) {
            StyleEnvironment layerEnvironment;
            if (tileLayer->getStyleEnvironment(viewState, layerEnvironment)) {
                styleEnvironment.mergeMissing(layerEnvironment);
            }
        }
        lighting = resolveLighting(_options->getLightOptions(), styleEnvironment);
        // The SHADOW sun, which is not always the lighting sun. A shadow is as long as
        // the caster is tall divided by tan(altitude): at 9 degrees a 700 m hill throws
        // 4.4 km and a 2 km massif 13 km. Two things break there, both measured. The
        // light box is stretched along the light by that same relief/tan(altitude), so
        // the whole cascade ladder goes coarse (31/53/62 m texels at z12.3 tilt 60,
        // against 11 m with the box bounded) - and shadows that long need casters from
        // far outside the drawn cover, so what does reach the screen is a flat grey
        // wash that appears and disappears as the cover changes with the zoom.
        // Flooring the altitude for the shadow pass ALONE caps the shadow length at
        // ~3.7x the relief and keeps the texels usable, while N.L lighting keeps the
        // true sun - so a low sun still reads as a low sun, without the wash.
        cglib::vec3<float> shadowSunDir = lighting.sunDir;
        {
            static const float MIN_SHADOW_SUN_SIN = 0.2588f; // sin(15 degrees)
            if (shadowSunDir(2) < MIN_SHADOW_SUN_SIN) {
                float horizontal = std::sqrt(shadowSunDir(0) * shadowSunDir(0) + shadowSunDir(1) * shadowSunDir(1));
                float scale = std::sqrt(std::max(0.0f, 1.0f - MIN_SHADOW_SUN_SIN * MIN_SHADOW_SUN_SIN));
                if (horizontal > 1.0e-6f) {
                    // Keep the azimuth: only the altitude is raised, so the shadows
                    // still fall in the direction the sun says, just shorter.
                    shadowSunDir(0) *= scale / horizontal;
                    shadowSunDir(1) *= scale / horizontal;
                }
                shadowSunDir(2) = MIN_SHADOW_SUN_SIN;
            }
        }
        bool shadowsWanted = false;
        {
            shadowsWanted = castShadows && lighting.terrainLightingEnabled && lighting.shadowStrength > 0.0f && !coverTileIds.empty();
            if (shadowsWanted) {
                if (!_terrainShadowMap) {
                    _terrainShadowMap = std::make_unique<TerrainShadowMap>();
                }
                _terrainShadowMap->setSize(lighting.shadowMapSize, lighting.shadowCascades);
                // Fit the light box to the elevation the shadowed ground actually
                // spans, plus headroom for what stands on it. With a low sun the box
                // is stretched by this range divided by tan(altitude), so a generous
                // slab is the difference between half-metre and ten-metre texels.
                double minHeight = 0, maxHeight = 0;
                // Per tile as well as overall: a cascade covering a small piece of
                // ground can then fit its box to THAT piece's relief instead of to
                // the whole scene's, which at a low sun is what sets the box size.
                std::vector<std::pair<double, double> > tileHeights;
                if (std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager()) {
                    bool first = true;
                    tileHeights.reserve(coverTileIds.size());
                    for (const vt::TileId& tileId : coverTileIds) {
                        double tileMin = 0, tileMax = 0;
                        elevationManager->getMinMaxDisplayHeightExact(MapTile(tileId.x, tileId.y, tileId.zoom, 0), tileMin, tileMax);
                        double tileHeadroom = std::max(1.0e-5, (tileMax - tileMin) * 0.25);
                        tileHeights.emplace_back(tileMin - tileHeadroom, tileMax + tileHeadroom);
                        if (first) {
                            minHeight = tileMin;
                            maxHeight = tileMax;
                            first = false;
                        } else {
                            minHeight = std::min(minHeight, tileMin);
                            maxHeight = std::max(maxHeight, tileMax);
                        }
                    }
                    if (!first) {
                        double headroom = std::max(1.0e-5, (maxHeight - minHeight) * 0.25);
                        minHeight -= headroom;
                        maxHeight += headroom;
                    }
                }
                // Casters reach beyond the visible tiles: a mountain just off screen
                // still throws its shadow into the view, and without this its shadow
                // vanishes as you zoom in and it leaves the visible set.
                std::vector<vt::TileId> casterTileIds = coverTileIds;
                int casterMargin = lighting.shadowCasterMargin;
                if (casterMargin > 0) {
                    std::set<std::pair<int, std::pair<int, int> > > seen;
                    for (const vt::TileId& tileId : coverTileIds) {
                        seen.insert({ tileId.zoom, { tileId.x, tileId.y } });
                    }
                    for (const vt::TileId& tileId : coverTileIds) {
                        for (int dy = -casterMargin; dy <= casterMargin; dy++) {
                            for (int dx = -casterMargin; dx <= casterMargin; dx++) {
                                vt::TileId neighbour(tileId.zoom, tileId.x + dx, tileId.y + dy);
                                if (seen.insert({ neighbour.zoom, { neighbour.x, neighbour.y } }).second) {
                                    casterTileIds.push_back(neighbour);
                                }
                            }
                        }
                    }
                }
                // One light box per cascade, near slice first. A single box has to
                // span everything visible, so at a tilt its texels are metres of
                // ground and every shadow edge is a staircase; the near cascade
                // spends the same texels on a much smaller region.
                int cascades = _terrainShadowMap->getCascades();
                bool boxesValid = true;
                // The tiles that can cast into each cascade, which for a near cascade
                // is a fraction of the cover: drawing the rest into it is pure cost.
                std::array<std::vector<vt::TileId>, TerrainShadowMap::MAX_CASCADES> cascadeCasterTiles;
                for (int cascade = 0; cascade < cascades; cascade++) {
                    double depthRangeMeters = 1.0, texelMeters = 0;
                    if (tileLayers.front()->calculateShadowViewProj(coverTileIds, casterTileIds, shadowSunDir, tileHeights, minHeight, maxHeight, lighting.shadowDistance, _terrainShadowMap->getSize(), cascade, cascades, cascadeCasterTiles[cascade], depthRangeMeters, texelMeters, lightViewProjs[cascade])) {
                        // The bias is metric; the shader wants a fraction of the
                        // normalised light depth, and each cascade's box spans its
                        // own depth. Dividing per cascade is what keeps the shadow
                        // attached to its caster at every zoom and margin.
                        shadowBiases[cascade] = static_cast<float>(lighting.shadowBias / std::max(1.0, depthRangeMeters));
                        shadowTexelMeters[cascade] = texelMeters;
                    } else if (cascade > 0) {
                        // No ground in this cascade's distance slice - looking down,
                        // everything visible can be nearer than the first split.
                        // Repeating the near box keeps the atlas layout intact and
                        // costs one redundant page; leaving it stale would shadow
                        // with a box from another frame.
                        lightViewProjs[cascade] = lightViewProjs[cascade - 1];
                        shadowBiases[cascade] = shadowBiases[cascade - 1];
                        cascadeCasterTiles[cascade] = cascadeCasterTiles[cascade - 1];
                    } else {
                        boxesValid = false;
                        static int lastFitFailure = 0;
                        if (static_cast<int>(texelMeters) != lastFitFailure) {
                            lastFitFailure = static_cast<int>(texelMeters);
                            Log::Infof("MapRenderer: shadow light box could not be fitted, reason %d (1 no tiles, 2 tile bbox empty, 3 no elevation texture, 4 empty cascade slice, 5 slice misses the tiles, 6 sun below horizon)", lastFitFailure);
                        }
                        break;
                    }
                }
                if (boxesValid) {
                    // The caster pass draws the whole terrain a second time, so it is
                    // worth as much as the on-screen draw - and on a still view it
                    // produces the same texture every frame. The light box is snapped
                    // to a world-anchored texel lattice, so its matrix repeats exactly
                    // while the camera moves inside one step: recompute only when that
                    // matrix, the caster set or the tile content actually changed.
                    // The age cap picks up elevation that streamed in without either.
                    // An extrusion that is still growing into place changes the
                    // caster geometry without changing the tile list, so a cached map
                    // would hold the shadow of a building that is not that shape yet.
                    // Tracked by how far the geometry has MOVED, not by whether it is
                    // moving: a fade is tens of frames and a caster pass per frame of
                    // it is what makes tiles crawl into view.
                    float fadeSignature = 0.0f;
                    for (const std::shared_ptr<TileLayer>& tileLayer : tileLayers) {
                        fadeSignature = std::max(fadeSignature, tileLayer->shadowCasterFadeSignature());
                    }
                    bool refresh = !_shadowMapValid
                        || _shadowMapSize != _terrainShadowMap->getSize()
                        || _shadowMapCascades != cascades
                        || _shadowMapCasterTiles != casterTileIds;
                    for (int cascade = 0; cascade < cascades && !refresh; cascade++) {
                        refresh = !(_shadowMapViewProjs[cascade] == lightViewProjs[cascade]);
                    }
                    _shadowMapAge++;
                    // Content-driven refreshes are RATIONED, camera-driven ones are
                    // not. A shadow left behind by a moving camera is in the wrong
                    // place and unmissable; a building whose shadow is a step behind
                    // its own growth is not.
                    if (!refresh && std::abs(fadeSignature - _shadowMapFadeSignature) > SHADOW_MAP_FADE_STEP) {
                        refresh = true;
                    }
                    if (!refresh && contentChanged && _shadowMapAge >= SHADOW_MAP_CONTENT_INTERVAL) {
                        refresh = true;
                    }
                    if (!refresh && _shadowMapAge >= SHADOW_MAP_MAX_AGE) {
                        refresh = true;
                    }
                    if (refresh) {
                        _shadowMapValid = false;
                        std::chrono::steady_clock::time_point shadowStart = std::chrono::steady_clock::now();
                        if (_terrainShadowMap->beginPass()) {
                            for (int cascade = 0; cascade < cascades; cascade++) {
                                // The cascades are pages of one texture, so the pass
                                // is cleared once and each cascade draws into its own
                                // viewport.
                                _terrainShadowMap->setCascadeViewport(cascade);
                                // EVERY drape layer casts, not just the first. The terrain
                                // surface is shared, but 3D extrusions belong to whichever
                                // layer holds them - in a composite that is a later style
                                // group, so casting from the front layer alone means
                                // buildings never cast a shadow at all.
                                for (const std::shared_ptr<TileLayer>& tileLayer : tileLayers) {
                                    bool castGround = (tileLayer == tileLayers.front());
                                    int draws = tileLayer->renderShadowCasters(cascadeCasterTiles[cascade], lightViewProjs[cascade], castGround);
                                    // Ground casters are one draw per tile; anything
                                    // beyond that is an extrusion. Counted separately
                                    // because "buildings cast no shadow" has two very
                                    // different causes - not drawn into the map at all,
                                    // or drawn and then clipped by the light box - and
                                    // only this tells them apart.
                                    shadowExtrusionDraws += draws - (castGround ? static_cast<int>(cascadeCasterTiles[cascade].size()) : 0);
                                }
                            }
                            _terrainShadowMap->endPass(prevFBO, viewState.getWidth(), viewState.getHeight());
                            shadowMsSum += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - shadowStart).count();
                            _shadowMapValid = true;
                            _shadowMapViewProjs = lightViewProjs;
                            _shadowMapBiases = shadowBiases;
                            _shadowMapCasterTiles = casterTileIds;
                            _shadowMapSize = _terrainShadowMap->getSize();
                            _shadowMapCascades = cascades;
                            _shadowMapFadeSignature = fadeSignature;
                            _shadowMapAge = 0;
                            for (int cascade = 0; cascade < cascades; cascade++) {
                                shadowCasterDraws += static_cast<int>(cascadeCasterTiles[cascade].size());
                            }
                            shadowPasses++;
                        }
                    }
                    if (_shadowMapValid) {
                        shadowTexture = _terrainShadowMap->getTexture();
                        shadowMapSize = _terrainShadowMap->getSize();
                        shadowCascades = cascades;
                        shadowStrength = lighting.shadowStrength;
                        shadowSoftness = lighting.shadowSoftness;
                    }
                } else if (_shadowMapValid) {
                    // A frame whose light box could not be fitted (a cascade with no
                    // ground in its slice, a cover with no decoded elevation yet) used
                    // to drop the shadows entirely for that frame - every shadow on
                    // screen blinking out and back. The last good map with the matrices
                    // it was rendered with is a far better answer than none: it is at
                    // worst one camera step stale, and the next good fit replaces it.
                    lightViewProjs = _shadowMapViewProjs;
                    shadowBiases = _shadowMapBiases;
                    shadowTexture = _terrainShadowMap->getTexture();
                    shadowMapSize = _shadowMapSize;
                    shadowCascades = _shadowMapCascades;
                    shadowStrength = lighting.shadowStrength;
                    shadowSoftness = lighting.shadowSoftness;
                    _shadowMapAge++;
                }
            }
        }
        if (!shadowsWanted) {
            _shadowMapValid = false; // shadows off: whatever the map holds is stale
        }
        // Shadows going away is otherwise indistinguishable from shadows being drawn
        // badly. Logged on CHANGE only, so it is one line per transition, not spam.
        {
            int shadowState = (!shadowsWanted ? 0 : (shadowTexture == 0 ? 1 : 2));
            static int lastShadowState = -1;
            if (shadowState != lastShadowState) {
                lastShadowState = shadowState;
                Log::Infof("MapRenderer: shadows %s (strength %.2f, requested map %d x %d cascades, terrain lighting %d, cover tiles %d)",
                    shadowState == 2 ? "ACTIVE" : shadowState == 1 ? "WANTED BUT UNAVAILABLE - no light box could be fitted, or the atlas failed to allocate" : "off",
                    lighting.shadowStrength, lighting.shadowMapSize, lighting.shadowCascades,
                    lighting.terrainLightingEnabled ? 1 : 0, static_cast<int>(coverTileIds.size()));
            }
        }
        for (const std::shared_ptr<TileLayer>& tileLayer : tileLayers) {
            tileLayer->setTerrainShadowMap(shadowTexture, shadowMapSize, shadowCascades, shadowBiases, shadowStrength, shadowSoftness, lightViewProjs);
            // The sun goes with it, and for the same reason: the surface is drawn a few
            // lines below, while each layer's own onDrawFrame - which also sets this -
            // runs later in the frame. The surface would light itself with the previous
            // frame's sun, so toggling the light did nothing until something else
            // happened to force another frame.
            tileLayer->setTerrainSunLighting(lighting.terrainLightingEnabled, lighting.sunDir, lighting.sunColor, lighting.sunIntensity, lighting.ambientIntensity);
        }
    }

    bool MapRenderer::coversTile(const vt::TileId& tileId, const vt::TileId& other) {
        if (tileId.zoom >= other.zoom) {
            return false; // strict ancestor only
        }
        int deltaZoom = other.zoom - tileId.zoom;
        return (other.x >> deltaZoom) == tileId.x && (other.y >> deltaZoom) == tileId.y;
    }

    void MapRenderer::collectTerrainCover(const std::vector<std::shared_ptr<TileLayer> >& tileLayers, const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::vector<vt::TileId>& seedTileIds, std::vector<std::map<vt::TileId, std::size_t> >& layerTiles, std::map<vt::TileId, std::size_t>& collectedTiles, std::vector<vt::TileId>& leaves, int& coverZoom, int& maxCollectedZoom) {
        // Collected PER LAYER, then merged. The union is what the cover is built from,
        // but which layers actually have something to bake for a tile is what tells a
        // texture baked from the full stack apart from one baked while only the
        // hillshade had arrived - and the second kind must not sit around looking
        // finished. A layer with nothing for a tile reports fingerprint 0.
        layerTiles.assign(tileLayers.size(), std::map<vt::TileId, std::size_t>());
        for (std::size_t i = 0; i < tileLayers.size(); i++) {
            tileLayers[i]->collectDrapeTiles(layerTiles[i]);
        }
        for (std::size_t i = 0; i < layerTiles.size(); i++) {
            for (auto it = layerTiles[i].begin(); it != layerTiles[i].end(); it++) {
                std::size_t& fingerprint = collectedTiles[it->first];
                fingerprint ^= it->second + 0x9e3779b9 + (fingerprint << 6) + (fingerprint >> 2);
            }
        }
        // Ground the cover has to cover, whatever the layers happen to hold. The layers' tiles
        // follow their own fetching, so right after a zoom OUT they still describe the small area
        // the previous zoom showed - and a cover built from them alone leaves most of the screen
        // with no terrain at all, falling through to the flat background plane. That is the
        // "tiles blink white while zooming" report. The terrain's own cover is camera-driven and
        // always covers the view, which is where tangram takes its ground tiles from.
        for (const vt::TileId& tileId : seedTileIds) {
            collectedTiles.emplace(tileId, static_cast<std::size_t>(0));
        }
        // A layer that bakes something not made of tiles - a terrain paint - cannot
        // contribute a cover, so a stack of nothing but such layers (a hillshade-only
        // map) would have no ground to paint on at all. The terrain's own tile cover
        // is the right one there: it is what the surface would be drawn from anyway.
        if (collectedTiles.empty()) {
            bool wantsCover = false;
            for (const std::shared_ptr<TileLayer>& tileLayer : tileLayers) {
                wantsCover = wantsCover || tileLayer->paintsEveryDrapeTile();
            }
            if (wantsCover && _terrainRenderer) {
                std::vector<MapTile> terrainTiles;
                _terrainRenderer->collectVisibleTiles(viewState, terrainOptions, terrainTiles);
                std::shared_ptr<ElevationManager> coverElevationManager = terrainOptions->getElevationManager();
                for (const MapTile& terrainTile : terrainTiles) {
                    collectedTiles[vt::TileId(terrainTile.getZoom(), terrainTile.getX(), terrainTile.getY())] = 0;
                    // Nothing else asks for elevation in this stack: the layers that
                    // normally drive it are the ones with tiles, and a paint has none.
                    // Without this the terrain stays flat and the paint has nothing to
                    // shade - the map is an empty grid.
                    if (coverElevationManager) {
                        MapTile dataTile = coverElevationManager->getDataTile(terrainTile);
                        coverElevationManager->prefetchTileGrid(dataTile, 2);
                        // And keep the frames coming until it arrives: in a stack with
                        // no tile layer nothing else asks for a redraw, so the map goes
                        // idle on a flat, unpainted terrain and never comes back.
                        if (!coverElevationManager->getDataTileGrid(dataTile, ElevationManager::LoadMode::CACHED_ONLY)) {
                            requestRedraw();
                        }
                    }
                }
            }
        }

        // The collected set is a UNION across layers, and layers do not agree on a
        // zoom level - a hillshade limited by its DEM max zoom yields coarser tiles
        // than a vector tile layer. Drawing a surface for every tile in that union
        // would stack a coarse surface and the finer ones covering the same ground on
        // top of each other, and they fight. Normalize to a single non-overlapping
        // cover, keeping the FINEST tile for any given ground area; coarser layers
        // still contribute to it through the ancestor sub-rect bake.
        // Dropping a coarse tile outright is wrong: a single fine tile inside it covers
        // 1/4^n of its ground, and the rest would then have no surface at all - the
        // terrain there falls back to whatever is behind (the layer's flat background
        // plane), which reads as a hole. Split instead: a coarse tile that contains a
        // finer one is replaced by its four children, recursively, so the result is a
        // true quadtree partition. Leaves that are descendants of a coarse tile carry
        // its content through the sub-rect bake.
        std::vector<vt::TileId> pending;
        for (auto it = collectedTiles.begin(); it != collectedTiles.end(); it++) {
            bool hasCoarserTile = false;
            for (auto it2 = collectedTiles.begin(); it2 != collectedTiles.end() && !hasCoarserTile; it2++) {
                hasCoarserTile = coversTile(it2->first, it->first);
            }
            if (!hasCoarserTile) {
                pending.push_back(it->first); // top of a subtree; its descendants follow from the split
            }
        }
        static const std::size_t MAX_DRAPE_TILES = 256; // splitting is bounded; a runaway cover is not worth drawing
        int minTopZoom = 99;
        maxCollectedZoom = 0;
        for (auto it = collectedTiles.begin(); it != collectedTiles.end(); it++) {
            maxCollectedZoom = std::max(maxCollectedZoom, it->first.zoom);
        }
        for (const vt::TileId& tileId : pending) {
            minTopZoom = std::min(minTopZoom, tileId.zoom);
        }
        // The split level. The plain maximum over collected tiles is wrong: zooming out, a render
        // tile from before the gesture is still 'visible' while it blends away and would drag the
        // whole cover several levels finer than the camera. Cap it at what the camera can show.
        // TRIED AND REVERTED for the shared ground, where there is no texture budget to respect:
        // following the finest collected tile instead makes a zoom OUT explode the split, hit the
        // leaf cap, and truncate the cover - most of the screen then falls through to the flat
        // background plane, which is the "tiles blink white" report. The cap is not about textures.
        int viewZoomCap = static_cast<int>(std::ceil(viewState.getZoom())) + 1;
        coverZoom = std::min(maxCollectedZoom, std::max(viewZoomCap, minTopZoom));
        // Split a tile ONLY where a finer collected tile actually sits inside it.
        // Splitting every subtree down to one global level looks stable on paper, but
        // a layer that is showing a coarse proxy - one z6 tile standing in for the
        // whole view while its data loads - was then chopped into hundreds of leaves,
        // most of them off-screen ground nothing asked for. Measured: 16 collected
        // tiles became 127 leaves. That is fatal rather than merely wasteful, because
        // every leaf acquires a cache entry: two such covers exceed the cache and the
        // eviction pass drops the ENTIRE previous generation, which is what the seed
        // and the stand-in both read from. Hence 'seeded 0, blank 16' with a screen
        // of flat fills. Where there is no finer content, one coarse surface is not
        // just cheaper, it is the same picture.
        std::vector<vt::TileId> tops = pending;
        auto buildLeaves = [&](int zoomLimit) {
            leaves.clear();
            std::vector<vt::TileId> stack = tops;
            while (!stack.empty() && leaves.size() + stack.size() <= MAX_DRAPE_TILES) {
                vt::TileId tileId = stack.back();
                stack.pop_back();
                bool finerInside = false;
                for (auto it = collectedTiles.begin(); it != collectedTiles.end() && !finerInside; it++) {
                    finerInside = coversTile(tileId, it->first);
                }
                if (!finerInside || tileId.zoom >= zoomLimit) {
                    leaves.push_back(tileId);
                    continue;
                }
                for (int dy = 0; dy < 2; dy++) {
                    for (int dx = 0; dx < 2; dx++) {
                        stack.push_back(tileId.getChild(dx, dy));
                    }
                }
            }
            // Cap hit: keep what is left coarse rather than lose the ground.
            leaves.insert(leaves.end(), stack.begin(), stack.end());
            return leaves.size();
        };
        while (buildLeaves(coverZoom) > MAX_DRAPE_TILES && coverZoom > minTopZoom) {
            coverZoom--; // one level coarser everywhere beats a half-split cover
        }
    }

    void MapRenderer::drawLayers(float deltaSeconds, const ViewState& viewState) {
        FRAME_PROF_NOW(profDrawStart);
        FRAME_PROF_GPU_BEGIN(SECTION_PRELUDE);
        std::vector<std::shared_ptr<Layer> > layers = _layers->getAll();

        // Terrain depth source: the FIRST suitable tile layer writes the depth of its
        // draped background/raster surfaces - the depth source is then bit-exact with the
        // rendered terrain, so draped geometry, other layers and vector elements can
        // depth-test against it without mesh-mismatch artifacts (sinking/see-through).
        // Only when no tile layer is available, a separate approximate terrain depth
        // pre-pass is rendered instead.
        bool terrainMode = false;
        if (_options->getRenderProjectionMode() == RenderProjectionMode::RENDER_PROJECTION_MODE_PLANAR) {
            if (auto terrainOptions = _options->getTerrainOptions()) {
                if (terrainOptions->isEnabled()) {
                    terrainMode = true;
                    bool depthWriteAssigned = false;
                    int terrainRenderOrder = 0;
                    for (const std::shared_ptr<Layer>& layer : layers) {
                        if (auto tileLayer = std::dynamic_pointer_cast<TileLayer>(layer)) {
                            bool depthWrite = !depthWriteAssigned && tileLayer->isVisible() && tileLayer->getOpacity() >= 1.0f;
                            tileLayer->setTerrainDepthWriteMode(depthWrite);
                            // stacking order for the fixed per-layer depth separation in GPU draping mode
                            tileLayer->setTerrainRenderOrder(terrainRenderOrder++);
                            depthWriteAssigned = depthWriteAssigned || depthWrite;
                        }
                    }
                    // Terrain base fill, rendered GLOBALLY before all tile layers: this
                    // way it shows through translucent tile layer content (e.g. a
                    // semi-transparent vector tile style or a hillshade layer)
                    // regardless of the layer stacking order, and guarantees the terrain
                    // is always painted with a solid base. When enabled, the map
                    // background bitmap is draped over the terrain instead of the solid
                    // color. With a depth-write tile layer, the fill is COLOR-ONLY (its
                    // depth is discarded): the tile layer surface pre-passes provide the
                    // terrain depth with their own meshes, and any kept fill depth would
                    // clip the differently-tesselated tile content in triangle-shaped
                    // patches. Without one, the fill (or the depth-only pre-pass) is the
                    // terrain depth source for element/billboard occlusion.
                    bool depthSourceRendered = false;
                    {
                        if (!_terrainRenderer) {
                            _terrainRenderer = std::make_unique<TerrainRenderer>();
                        }
                        bool keepDepth = !depthWriteAssigned;
                        bool backgroundRendered = false;
                        if (terrainOptions->isBackgroundBitmapEnabled()) {
                            if (std::shared_ptr<Bitmap> backgroundBitmap = _options->getBackgroundBitmap()) {
                                backgroundRendered = _terrainRenderer->renderBackground(viewState, terrainOptions, _glResourceManager, backgroundBitmap, keepDepth);
                            }
                        }
                        if (!backgroundRendered) {
                            Color terrainBackgroundColor = terrainOptions->getBackgroundColor();
                            if (terrainBackgroundColor.getA() > 0) {
                                backgroundRendered = _terrainRenderer->renderBackground(viewState, terrainOptions, _glResourceManager, terrainBackgroundColor, keepDepth);
                            }
                        }
                        depthSourceRendered = backgroundRendered && keepDepth;
                        if (!depthSourceRendered && !depthWriteAssigned) {
                            _terrainRenderer->renderDepthPrepass(viewState, terrainOptions, _glResourceManager);
                        }
                    }
                    if (terrainOptions->isBillboardOcclusionEnabled()) {
                        // Pixel-exact terrain depth buffer for label/billboard occlusion tests
                        if (!_terrainRenderer) {
                            _terrainRenderer = std::make_unique<TerrainRenderer>();
                        }
                        _terrainRenderer->updateDepthBuffer(viewState, terrainOptions, _glResourceManager);
                        if (_terrainRenderer->isDepthBufferStale()) {
                            // The refresh was deferred to keep the read-back stall out of a
                            // moving frame; keep asking for frames so it happens once the
                            // camera settles rather than on the next unrelated redraw.
                            requestRedraw();
                        }
                    }

                    // Camera terrain-following. The clearance is expressed as a BOUND on the
                    // zoom (ViewState::setTerrainMinCameraZ -> getTerrainMaxZoom), which every
                    // zoom path clamps against, rather than as a corrective zoom-out issued
                    // after the camera has already broken through. A corrective event fights
                    // whatever is driving the camera down - the pinch gesture, the double-tap
                    // zoom animation, a kinetic fling - so the camera oscillates for as long as
                    // the gesture lasts, and an animation that keeps its absolute target snaps
                    // back to it on its final tick (the "double tap jumps back" symptom). As a
                    // bound, the gesture simply comes to rest against the terrain.
                    // A correction is still needed for the paths that change the camera height
                    // WITHOUT going through a zoom event - panning into a hillside, tilting, or
                    // new elevation tiles raising the ground under a stationary camera. Like
                    // tangram (View::updateMatrices), that correction is strictly
                    // one-directional (it only ever zooms out, never back in) and lands exactly
                    // on the clearance shell, so it cannot oscillate.
                    float cameraClearance = terrainOptions->getCameraClearance();
                    float clampDuration = terrainOptions->getCameraClampDuration();
                    if (cameraClearance > 0) {
                        std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager();
                        cglib::vec3<double> cameraPos = viewState.getCameraPos();
                        double terrainZ = elevationManager->getDisplayHeight(cameraPos(0), cameraPos(1), ElevationManager::LoadMode::CACHED_ONLY);
                        double minCameraZ = terrainZ + cameraClearance * elevationManager->getDisplayScale(cameraPos(1));
                        {
                            std::lock_guard<std::recursive_mutex> lock(_mutex);
                            _viewState.setTerrainMinCameraZ(minCameraZ);
                        }
                        // The correction has to LAND on the shell, and it has to have a dead band.
                        // Zooming out scales the camera-to-focus vector, so the camera height it
                        // reaches is focusZ + (cameraZ - focusZ) * scale - not cameraZ * scale.
                        // Scaling by minCameraZ/cameraZ (which ignores the focus height) always
                        // lands SHORT of the shell whenever the focus is above sea level, so the
                        // next frame is still below it and issues another correction: the camera
                        // creeps towards the shell for ever and every frame requests a redraw, so
                        // a completely still 3D map never stops rendering. Solving for the scale
                        // that reaches the shell exactly, plus a dead band, ends the sequence
                        // after one correction.
                        double focusZ = viewState.getFocusPos()(2);
                        double deadBand = 0.005 * cameraClearance * elevationManager->getDisplayScale(cameraPos(1));
                        // At the bottom of the zoom range there is no correction left to make, and
                        // issuing one anyway is another per-frame redraw that changes nothing.
                        bool zoomExhausted = viewState.getZoom() <= _options->getZoomRange().getMin() + 1.0e-4f;
                        // A camera at or below the focus height - a horizontal view, or one looking
                        // above the horizon - cannot be raised by zooming out at all: the zoom
                        // scales the camera-to-focus vector, which is then horizontal. Correcting
                        // anyway asks for a zoom that never arrives, every frame (see
                        // ViewState::getTerrainMaxZoom, which drops its bound for the same reason).
                        bool cameraAboveFocus = cameraPos(2) > focusZ + deadBand;
                        if (!zoomExhausted && cameraAboveFocus && cameraPos(2) > 0 && cameraPos(2) < minCameraZ - deadBand) {
                            double scale = (minCameraZ > focusZ
                                ? (minCameraZ - focusZ) / (cameraPos(2) - focusZ)   // exact: the focus stays put
                                : minCameraZ / cameraPos(2));                        // degenerate (focus above the shell)
                            CameraZoomEvent zoomEvent;
                            zoomEvent.setZoomDelta(static_cast<float>(-std::log2(scale))); // negative: zoom out onto the clearance
                            calculateCameraEvent(zoomEvent, clampDuration, false);
                        }
                    } else {
                        std::lock_guard<std::recursive_mutex> lock(_mutex);
                        _viewState.setTerrainMinCameraZ(0);
                    }
                }
            }
        }
        if (!terrainMode) {
            for (const std::shared_ptr<Layer>& layer : layers) {
                if (auto tileLayer = std::dynamic_pointer_cast<TileLayer>(layer)) {
                    tileLayer->setTerrainDepthWriteMode(false);
                }
            }
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _viewState.setTerrainMinCameraZ(0); // release the terrain zoom bound
        }

        // Cross-layer terrain draping. Every drapeable tile layer bakes into ONE texture per
        // terrain tile, in layer order, and a single surface draw puts that texture on the
        // terrain - so a hillshade layer and a vector tile layer share one drape, one surface and
        // one depth domain instead of each keeping its own. Content that is draped never enters
        // the 3D scene at all, which is what removes the whole content-vs-surface depth problem.
        std::vector<std::shared_ptr<TileLayer> > drapeLayers;
        bool sharedGroundActive = false;
        if (terrainMode) {
            // A terrain paint has no tile set: without a drape to bake into it draws itself, on
            // the terrain's own cover. Pushed every frame, before any layer draws, and harmless
            // for a paint that does bake (it ignores the list).
            if (auto paintTerrainOptions = _options->getTerrainOptions()) {
                std::vector<std::shared_ptr<TileLayer> > paintLayers;
                for (const std::shared_ptr<Layer>& layer : layers) {
                    layer->collectDrapeLayers(paintLayers, viewState);
                }
                bool anyPaint = false;
                for (const std::shared_ptr<TileLayer>& tileLayer : paintLayers) {
                    anyPaint = anyPaint || tileLayer->paintsEveryDrapeTile();
                }
                if (anyPaint && _terrainRenderer) {
                    std::vector<MapTile> terrainTiles;
                    _terrainRenderer->collectVisibleTiles(viewState, paintTerrainOptions, terrainTiles);
                    std::vector<vt::TileId> paintTileIds;
                    paintTileIds.reserve(terrainTiles.size());
                    for (const MapTile& terrainTile : terrainTiles) {
                        paintTileIds.emplace_back(terrainTile.getZoom(), terrainTile.getX(), terrainTile.getY());
                    }
                    for (const std::shared_ptr<TileLayer>& tileLayer : paintLayers) {
                        if (tileLayer->paintsEveryDrapeTile()) {
                            tileLayer->setTerrainPaintTiles(paintTileIds);
                        }
                    }
                }
            }
            if (auto terrainOptions = _options->getTerrainOptions()) {
                if (terrainOptions->isDrapeFillsEnabled()) {
                    // Layers report their own drapeable tile layers, so a composite layer can
                    // contribute its children (hillshade/raster slots, style-layer groups) in
                    // draw order rather than only its own group-0 renderer.
                    for (const std::shared_ptr<Layer>& layer : layers) {
                        layer->collectDrapeLayers(drapeLayers, viewState);
                    }
                } else {
                    // NO DRAPE: the tangram arrangement. The stack still shares ONE cover, but
                    // nothing is baked - the ground is drawn once for that cover, here, before any
                    // layer, and every layer then composites straight onto it in layer order. What
                    // that removes is the whole bake (textures, budgets, stand-ins) AND, in every
                    // layer, its private depth domain and its stencil tile masks: one ground draw
                    // per tile instead of a pre-pass plus a mask per tile PER LAYER.
                    std::vector<std::shared_ptr<TileLayer> > groundLayers;
                    for (const std::shared_ptr<Layer>& layer : layers) {
                        layer->collectDrapeLayers(groundLayers, viewState);
                    }
                    if (!groundLayers.empty()) {
                        // Every layer's render tiles must exist before the cover is read from them.
                        FRAME_PROF_ADD(preludeMs, profDrawStart);
                        FRAME_PROF_NOW(profPrepareStart);
                        FRAME_PROF_GPU_BEGIN(SECTION_PREPARE);
                        for (const std::shared_ptr<TileLayer>& tileLayer : groundLayers) {
                            tileLayer->prepareTerrainDrapeFrame(deltaSeconds, viewState);
                        }
                        FRAME_PROF_ADD(prepareMs, profPrepareStart);
                        FRAME_PROF_NOW(profCoverStart);
                        FRAME_PROF_GPU_BEGIN(SECTION_COVER);

                        // The terrain's own visible cover seeds the ground: it is what the camera
                        // can see, not what the layers happen to have fetched.
                        std::vector<vt::TileId> terrainCoverTileIds;
                        if (_terrainRenderer) {
                            std::vector<MapTile> terrainTiles;
                            _terrainRenderer->collectVisibleTiles(viewState, terrainOptions, terrainTiles);
                            terrainCoverTileIds.reserve(terrainTiles.size());
                            for (const MapTile& terrainTile : terrainTiles) {
                                terrainCoverTileIds.emplace_back(terrainTile.getZoom(), terrainTile.getX(), terrainTile.getY());
                            }
                        }
                        std::vector<std::map<vt::TileId, std::size_t> > groundLayerTiles;
                        std::map<vt::TileId, std::size_t> groundCollectedTiles;
                        std::vector<vt::TileId> groundTileIds;
                        std::vector<int> groundProxyDepths;
                        std::vector<bool> groundStandingIn; // parallel: this tile is drawn in place of a finer one
                        int groundZoom = 0, groundMaxCollectedZoom = 0;
                        collectTerrainCover(groundLayers, viewState, terrainOptions, terrainCoverTileIds, groundLayerTiles, groundCollectedTiles, groundTileIds, groundZoom, groundMaxCollectedZoom);

                        // A leaf whose DEM has not arrived is drawn FLAT, and the paint skips it
                        // (it has nothing to shade), so it flashes in the bare ground colour until
                        // the elevation lands - which during a zoom is every tile on screen, and
                        // reads as tiles blinking white then filling in. The drape hid this behind
                        // a stand-in texture from an ancestor; without textures the equivalent is
                        // to STAND ON the ancestor: keep the coarsest displaced ground that is
                        // actually loaded rather than introduce a flat one.
                        if (std::shared_ptr<ElevationManager> groundElevationManager = terrainOptions->getElevationManager()) {
                            auto hasElevation = [&groundElevationManager](const vt::TileId& tileId) {
                                int tileMask = (1 << tileId.zoom) - 1;
                                MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
                                return static_cast<bool>(groundElevationManager->getTileGrid(mapTile, ElevationManager::LoadMode::CACHED_ONLY));
                            };
                            std::vector<vt::TileId> loadedTileIds;
                            std::vector<bool> standingIn;
                            loadedTileIds.reserve(groundTileIds.size());
                            standingIn.reserve(groundTileIds.size());
                            for (const vt::TileId& tileId : groundTileIds) {
                                vt::TileId standIn = tileId;
                                while (standIn.zoom > 0 && !hasElevation(standIn)) {
                                    standIn = standIn.getParent();
                                }
                                // The walk can bring several leaves onto one ancestor; drawing it
                                // once is both correct and cheaper.
                                auto it = std::find(loadedTileIds.begin(), loadedTileIds.end(), standIn);
                                if (it == loadedTileIds.end()) {
                                    loadedTileIds.push_back(standIn);
                                    standingIn.push_back(standIn != tileId);
                                } else if (standIn != tileId) {
                                    standingIn[it - loadedTileIds.begin()] = true;
                                }
                            }
                            groundTileIds = std::move(loadedTileIds);
                            groundStandingIn = std::move(standingIn);
                        }

                        // Tangram's proxy depth for the ground, their formula
                        // (core/src/tile/tileManager.cpp):
                        //     setProxyDepth(m_proxyCounter > 0 ? std::max(maxVisS - tileId.s, 1) : 0)
                        // The `m_proxyCounter > 0` is a GUARD, and it is the whole point: the depth
                        // applies ONLY to a tile drawn in place of one that has no data of its own.
                        // A legitimately coarse tile of a mixed-LOD cover is a live tile and takes
                        // ZERO however far it is - give it a depth and 48 units per level pushes
                        // most of a tilted view's far field back by clip units, taking the hillshade
                        // paint (which carries the same push) behind the ground it shades: far
                        // hillshade missing, blinking as the cover's deepest level moves.
                        int groundCoverZoom = 0;
                        for (const vt::TileId& tileId : groundTileIds) {
                            groundCoverZoom = std::max(groundCoverZoom, tileId.zoom);
                        }
                        groundStandingIn.resize(groundTileIds.size(), false);
                        groundProxyDepths.clear();
                        groundProxyDepths.reserve(groundTileIds.size());
                        for (std::size_t i = 0; i < groundTileIds.size(); i++) {
                            groundProxyDepths.push_back(groundStandingIn[i] ? std::max(groundCoverZoom - groundTileIds[i].zoom, 1) : 0);
                        }

                        // What the ground is painted with where no layer paints anything. Ground
                        // with a hole in it shows the flat map background plane BEHIND the terrain
                        // - the "landcover holes" - so it follows the drape's clear colour rule:
                        // the terrain's own background, else the first layer that defines one.
                        Color groundColor = terrainOptions->getBackgroundColor();
                        if (groundColor.getA() == 0) {
                            for (const std::shared_ptr<TileLayer>& tileLayer : groundLayers) {
                                Color layerColor = tileLayer->getBackgroundColor(viewState);
                                if (layerColor.getA() != 0) {
                                    groundColor = layerColor;
                                    break;
                                }
                            }
                        }

                        // Number the stack in DRAW order, one range per layer: tangram separates
                        // style layers in depth by their order in one global list, and our stack is
                        // several renderers - a composite's children included. The stride leaves
                        // room for a layer's own style layers before the next layer starts.
                        // Numbered DENSELY, as a running sum of what each layer actually drew last
                        // frame - not a fixed stride. The ordinal feeds a constant-NDC pull whose
                        // eye tolerance grows as distance^2, so its TOTAL is what decides whether
                        // far content leaks over a near ridge; rounds 45-56 saw that start in the
                        // low hundreds, and a stride of 32 per layer reaches it with five layers.
                        // A style layer count is tens. One frame of lag in the counts is harmless:
                        // they only have to be consistent, not current.
                        // Starting at ONE, not zero: the ground is a numbered draw in the same list
                        // and it is the bottom of it. Tangram draws the terrain raster at the earth
                        // layer's `order` (res/scenes/hillshade.yaml, `order: global.earth_order`)
                        // and every content layer sits above it - so content at ordinal 0 would
                        // share the ground's term exactly and have no clearance over ground it
                        // chords across, which is the bottom style layer of the first layer.
                        int ordinalBase = 1;
                        for (const std::shared_ptr<TileLayer>& tileLayer : groundLayers) {
                            tileLayer->setExternalDrapeTarget(false);
                            tileLayer->setExternalDrapeTiles(std::vector<vt::TileId>());
                            tileLayer->setTerrainGroundTiles(groundTileIds, groundProxyDepths);
                            tileLayer->setTerrainLayerOrdinalBase(ordinalBase);
                            ordinalBase += std::max(1, tileLayer->getStyleLayerCount());
                        }
                        // The caster pass and the sun, over the same cover. Both have to be set
                        // BEFORE the ground is drawn: each layer normally sets the sun from its own
                        // onDrawFrame, which runs later in the frame, so the ground would light
                        // itself with the previous frame's sun. There is no bake here, so the
                        // content-driven refresh rides on the cover changing instead.
                        GLint groundPrevFBO = 0;
                        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &groundPrevFBO);
                        ResolvedLighting lighting;
                        std::array<double, TerrainShadowMap::MAX_CASCADES> shadowTexelMeters = { };
                        bool coverChanged = (_groundCoverTileIds != groundTileIds);
                        _groundCoverTileIds = groundTileIds;
                        // Shadows OFF for now, deliberately. The caster pass and the light boxes
                        // are wired to this cover and the sun does reach the ground and the paint,
                        // but on the emulator the map reads as scattered acne instead of the
                        // drape's cast shadows - same scene, same map, one path clean and the other
                        // not. Half-working shadows are worse than none; flip this to true to work
                        // on it, with the drape path as the reference to diff against.
                        applyTerrainShadows(groundLayers, groundTileIds, terrainOptions, viewState, groundPrevFBO, coverChanged, false, lighting, shadowTexelMeters);

                        FRAME_PROF_ADD(coverMs, profCoverStart);
                        FRAME_PROF_NOW(profGroundStart);
                        FRAME_PROF_GPU_BEGIN(SECTION_DRAPE);
                        // The ground is drawn by the layer that PAINTS it when there is one: the
                        // paint and its lighting shader live on that layer's renderer, and in
                        // tangram's arrangement the shading is part of the ground draw rather than
                        // a surface over it. Any layer can draw a plain ground, so the first one
                        // does when nothing paints.
                        std::shared_ptr<TileLayer> groundDrawer = groundLayers.front();
                        for (const std::shared_ptr<TileLayer>& tileLayer : groundLayers) {
                            if (tileLayer->paintsEveryDrapeTile()) {
                                groundDrawer = tileLayer;
                                break;
                            }
                        }
                        int groundDraws = groundDrawer->renderTerrainGround(groundColor);
                        FRAME_PROF_ADD(drapeMs, profGroundStart);
                        FRAME_PROF_GPU_END();

                        sharedGroundActive = true;
                        // Periodically, and once for the first frame that actually has a cover: a
                        // map settles and stops drawing frames, so a plain frame counter can leave
                        // the only line in the log being the empty startup one.
                        static int groundStateFrame = 0;
                        static bool groundCoverLogged = false;
                        bool firstCover = !groundCoverLogged && !groundTileIds.empty();
                        groundCoverLogged = groundCoverLogged || firstCover;
                        if ((groundStateFrame++ % 600) == 1 || firstCover) {
                            Log::Infof("MapRenderer: shared terrain ground - %d layers, %d cover tiles (split level %d, collected up to %d, camera zoom %.2f), %d ground draws",
                                static_cast<int>(groundLayers.size()), static_cast<int>(groundTileIds.size()),
                                groundZoom, groundMaxCollectedZoom, viewState.getZoom(), groundDraws);
                        }
                    }
                }
                // A single stack for now: the usual configuration (hillshade under vector tiles)
                // is contiguous and entirely drapeable. Splitting into several stacks only
                // matters once a non-drapeable layer sits between drapeable ones.
                if (!drapeLayers.empty()) {
                    if (!_terrainDrapeCache) {
                        _terrainDrapeCache = std::make_unique<TerrainDrapeCache>();
                    }
                    _terrainDrapeCache->setResolution(TileRenderer::resolveDrapeResolution(terrainOptions->getDrapeResolution(), viewState, _options));
                    // WHICH layers bake, not what is in them. Switching the base map's style
                    // builds a new layer object, so the cached textures - including the ones held
                    // off screen for panning - are pictures of the previous style. They are only
                    // ever noticed through the per-tile fingerprint, which is re-baked at one tile
                    // per frame, so panning kept bringing the old style back a tile at a time.
                    // A layer that bakes something which is NOT made of its tiles - a terrain paint
                    // shading the elevation texture - has no per-tile fingerprint to be noticed
                    // through, so it folds its own appearance into this signature instead.
                    std::size_t stackSignature = 0;
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        std::size_t layerHash = tileLayer->drapeStackSignature();
                        stackSignature ^= layerHash + 0x9e3779b9 + (stackSignature << 6) + (stackSignature >> 2);
                    }
                    _terrainDrapeCache->setStackSignature(stackSignature);

                    // Every participating layer's render tiles must exist before any of them
                    // bakes, so start their frames first.
                    FRAME_PROF_ADD(preludeMs, profDrawStart);
                    FRAME_PROF_NOW(profPrepareStart);
                    FRAME_PROF_GPU_BEGIN(SECTION_PREPARE);
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        tileLayer->prepareTerrainDrapeFrame(deltaSeconds, viewState);
                    }
                    FRAME_PROF_ADD(prepareMs, profPrepareStart);
                    FRAME_PROF_NOW(profCoverStart);
                    FRAME_PROF_GPU_BEGIN(SECTION_COVER);

                    std::vector<std::map<vt::TileId, std::size_t> > layerTiles;
                    std::map<vt::TileId, std::size_t> collectedTiles;
                    std::vector<vt::TileId> leaves;
                    int drapeZoom = 0, maxCollectedZoom = 0;
                    collectTerrainCover(drapeLayers, viewState, terrainOptions, std::vector<vt::TileId>(), layerTiles, collectedTiles, leaves, drapeZoom, maxCollectedZoom);
                    std::map<vt::TileId, std::size_t> drapeTiles;
                    // Which drape layers have content to bake into each leaf right now. Compared
                    // against what the cached texture was actually baked from, this separates
                    // "the picture moved on a little" from "a whole layer is missing here".
                    std::map<vt::TileId, std::size_t> drapeTileLayerMasks;
                    // Whether the DEM for a tile is decoded YET. A terrain paint can only paint a
                    // tile that has elevation, so this decides both whether the tile is expected to
                    // carry the paint and - through the fingerprint below - that it is baked again
                    // once the elevation does arrive.
                    std::shared_ptr<ElevationManager> drapeElevationManager = terrainOptions->getElevationManager();
                    auto hasElevationData = [&drapeElevationManager](const vt::TileId& tileId) {
                        if (!drapeElevationManager) {
                            return true; // no elevation source at all: nothing is displaced
                        }
                        int tileMask = (1 << tileId.zoom) - 1;
                        MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
                        return static_cast<bool>(drapeElevationManager->getTileGrid(mapTile, ElevationManager::LoadMode::CACHED_ONLY));
                    };
                    std::map<vt::TileId, bool> leafElevation;
                    bool anyPaintLayer = false;
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        anyPaintLayer = anyPaintLayer || tileLayer->paintsEveryDrapeTile();
                    }
                    for (const vt::TileId& tileId : leaves) {
                        bool paintable = hasElevationData(tileId);
                        leafElevation[tileId] = paintable;
                        std::size_t layerMask = 0;
                        for (std::size_t i = 0; i < layerTiles.size() && i < sizeof(std::size_t) * 8; i++) {
                            // A layer whose content is not made of tiles - a terrain paint - paints
                            // every tile it can and reports none of them, so it cannot be part of
                            // this mask: an incomplete tile is NOT DRAWN, and the paint is the
                            // layer most likely to be a frame late (its textures are prepared
                            // asynchronously). Gating on it blanked the top of the screen and the
                            // whole map during a zoom, where every tile is new. It gets a re-bake
                            // instead - through the fingerprint below when the elevation arrives,
                            // and through the bake itself when it could not paint (see bakeTile).
                            if (drapeLayers[i]->paintsEveryDrapeTile()) {
                                continue;
                            }
                            for (auto it = layerTiles[i].begin(); it != layerTiles[i].end(); it++) {
                                if (it->second == 0) {
                                    continue; // reported for the cover, but nothing drapeable in it
                                }
                                // Only contributors the bake will actually draw: its own tile, or a
                                // COARSER one covering it. GLTileRenderer::bakeDrapeTile skips
                                // render tiles FINER than the terrain tile on purpose (they are the
                                // generation being replaced, and minifying them into a sub-rect
                                // turns the drape into aliasing noise), so counting them here made
                                // the leaf permanently incomplete: its own bake could never satisfy
                                // a layer whose only contribution was a finer tile. The stand-in
                                // path then kept drawing that layer's PREVIOUS, finer textures over
                                // the leaf for as long as they stayed cached - which is the patch of
                                // stale z11 map (satellite, roads) that survives a zoom out to z10.
                                if (it->first == tileId || coversTile(it->first, tileId)) {
                                    layerMask |= static_cast<std::size_t>(1) << i;
                                    break;
                                }
                            }
                        }
                        drapeTileLayerMasks[tileId] = layerMask;
                        // Fold in every collected tile that will bake here - the leaf itself, every
                        // coarser tile covering it, and (when the split hit the cap and left this
                        // leaf coarse) the finer tiles inside it, which bake into their sub-rect.
                        // A contributor left out here is content whose change never invalidates
                        // the texture, i.e. a tile that stays stale for as long as it is cached.
                        std::size_t fingerprint = 0;
                        auto exactIt = collectedTiles.find(tileId);
                        if (exactIt != collectedTiles.end()) {
                            fingerprint = exactIt->second;
                        }
                        for (auto it = collectedTiles.begin(); it != collectedTiles.end(); it++) {
                            if (coversTile(it->first, tileId) || coversTile(tileId, it->first)) {
                                fingerprint ^= it->second + 0x9e3779b9 + (fingerprint << 6) + (fingerprint >> 2);
                            }
                        }
                        if (anyPaintLayer) {
                            // The paint has no per-tile content to fingerprint, but whether it can
                            // paint this tile at all is per-tile: fold it in, so the tile is baked
                            // again the moment its elevation arrives.
                            std::size_t elevationTerm = (paintable ? 0x9e3779b9u : 0x85ebca6bu);
                            fingerprint ^= elevationTerm + 0x9e3779b9 + (fingerprint << 6) + (fingerprint >> 2);
                        }
                        drapeTiles[tileId] = fingerprint;
                    }

                    // Only take the surface away from the per-layer path once we know this frame
                    // actually has tiles to drape. Enabling external targets unconditionally
                    // suppresses each layer's pre-pass AND its own drape surface, so a frame that
                    // then draws no shared surface leaves the terrain with no surface at all -
                    // worse than not draping.
                    bool drapeActive = !drapeTiles.empty();
                    std::vector<vt::TileId> drapeTileIds;
                    drapeTileIds.reserve(drapeTiles.size());
                    for (auto it = drapeTiles.begin(); it != drapeTiles.end(); it++) {
                        drapeTileIds.push_back(it->first);
                    }
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        tileLayer->setExternalDrapeTarget(drapeActive);
                        // Tell every participating layer which ground is draped BEFORE it draws.
                        // This has to be an explicit per-frame hand-off: deriving it inside the
                        // renderer from the surface draw is fragile, because the layer's own
                        // startFrame runs between the two and resets frame state.
                        tileLayer->setExternalDrapeTiles(drapeActive ? drapeTileIds : std::vector<vt::TileId>());
                    }
                    if (!drapeActive) {
                        static bool emptyDrapeLogged = false;
                        if (!emptyDrapeLogged) {
                            emptyDrapeLogged = true;
                            Log::Info("MapRenderer: RTT drape has no tiles this frame - per-layer path retained");
                        }
                    }

                    // What the bake starts from. A texel no layer paints is a hole: the terrain
                    // surface is translucent there and the map background plane - which in terrain
                    // mode lies BEHIND the terrain - shows through, which is exactly what the
                    // "landcover holes" look like. Style layers only paint their own features, so
                    // the ground between them has to come from the background colour, baked in.
                    Color drapeClearColor = terrainOptions->getBackgroundColor();
                    if (drapeClearColor.getA() == 0) {
                        for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                            Color layerColor = tileLayer->getBackgroundColor(viewState);
                            if (layerColor.getA() != 0) {
                                drapeClearColor = layerColor;
                                break;
                            }
                        }
                    }

                    GLint prevFBO = 0;
                    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
                    std::chrono::steady_clock::time_point drapeStart = std::chrono::steady_clock::now();
                    FRAME_PROF_ADD(coverMs, profCoverStart);
                    FRAME_PROF_GPU_BEGIN(SECTION_DRAPE);
                    try {
                    _terrainDrapeCache->beginFrame();
                    struct DrapedTile { vt::TileId tileId; unsigned int texture; float uvOffsetX, uvOffsetY, uvScale; };
                    std::vector<DrapedTile> drapedTiles;
                    drapedTiles.reserve(drapeTiles.size());
                    int resolution = _terrainDrapeCache->getResolution();
                    bool bakeStarted = false;
                    // Offscreen state is entered once per frame and only when something actually
                    // has to be drawn into a drape texture.
                    auto beginOffscreen = [&]() {
                        if (bakeStarted) {
                            return;
                        }
                        glBindFramebuffer(GL_FRAMEBUFFER, _terrainDrapeCache->getFrameBuffer());
                        glViewport(0, 0, resolution, resolution);
                        glDisable(GL_DEPTH_TEST);
                        glDepthMask(GL_FALSE);
                        glDisable(GL_STENCIL_TEST);
                        bakeStarted = true;
                    };
                    // Cumulative since start: bakes are cached, so a per-frame count is 0 on most
                    // frames and says nothing about whether baking ever produced anything.
                    static int bakedTiles = 0, bakedPrimitives = 0;
                    int surfaceDraws = 0, filledSurfaces = 0, skippedSurfaces = 0;
                    // Per-frame, unlike the cumulative counter above: the shadow cache below needs
                    // to know whether THIS frame produced new tile content, not whether any frame
                    // ever did.
                    int bakedThisFrame = 0;
                    // Baking a tile re-renders every layer of it into a full-resolution texture,
                    // so an unbounded loop over a churning tile cover is a per-frame re-render of
                    // the whole map - which is what made panning and zooming stall. Bake a few
                    // tiles per frame; the rest keep whatever they have (an older picture, or a
                    // flat fill if they have nothing yet) and catch up over the next frames.
                    // Two budgets, because the two cases are not equally urgent. A tile that can
                    // show nothing at all is a visible hole, so those are baked almost freely; a
                    // tile that is merely out of date already shows something plausible, so a
                    // couple per frame is enough and the cost stays off the critical path.
                    // Three classes, not two, because "has no texture of its own" covers two very
                    // different pictures: a tile standing in on an ancestor shows the right ground
                    // at half the sharpness, while a tile with no stand-in at all shows a flat
                    // fill - a hole. An integer zoom step renames the whole cover at once, so the
                    // second class is exactly what has to be cleared fast.
                    // Raising this to bake a whole renamed cover in one frame was measured on
                    // device: worst frame 128 ms -> 300 ms, with no visible difference in the
                    // stand-ins it was meant to remove. A bake is ~16 ms per tile at 1024, so the
                    // budget IS the frame time here; keep it low and let the cover catch up.
                    static const int DRAPE_BAKE_BUDGET_BLANK = 8;
                    static const int DRAPE_BAKE_BUDGET_STANDIN = 3;
                    // A tile MISSING A WHOLE LAYER is a fourth case, and it is not the mild one the
                    // stale budget was sized for. Raster layers are ready as soon as their tile
                    // decodes while vector tiles take a style pass, so a tile baked mid-load holds
                    // the hillshade and nothing else - and at one re-bake per frame a zoom leaves
                    // dozens of them showing bare hillshade over the map for the best part of a
                    // second, which reads as the hillshade layer flashing on top of everything.
                    static const int DRAPE_BAKE_BUDGET_PARTIAL = 6;
                    static const int DRAPE_BAKE_BUDGET_STALE = 1;
                    // A tile baked from a layer stack that no longer exists (the base map's style
                    // was switched, a layer was turned off) shows the PREVIOUS MAP. One per frame
                    // is the budget for a tile that is merely out of date; here the picture is
                    // wrong, and since the cache keeps a generation of tiles alive off screen,
                    // panning kept walking back over them and flashing the old style tile by tile.
                    // Cleared at the blank-tile rate instead: a couple of frames, like a zoom.
                    static const int DRAPE_BAKE_BUDGET_RESTACK = 8;
                    // Wall-clock ceiling for all of the classes above together, per frame.
                    // Measured on an Adreno 610 from a cold start in 3D: 5-11 bakes a second get
                    // through against 10-51 queued, while the bakes themselves cost 12-31 ms a
                    // SECOND - one to three percent of the wall clock. The budget, not the work,
                    // is what makes roads and fills crawl into view in 3D when 2D shows them at
                    // once. Give a frame room for several bakes, and much more room when the
                    // camera is still: a longer frame is invisible on a map that is not moving,
                    // a tile that takes seconds to appear is not.
                    static const double DRAPE_BAKE_TIME_BUDGET = 16.0;       // ms, camera moving
                    static const double DRAPE_BAKE_TIME_BUDGET_STILL = 60.0; // ms, camera at rest
                    struct BakeRequest { vt::TileId tileId; std::size_t fingerprint; std::size_t drapedIndex; };
                    std::vector<BakeRequest> blankTiles, standInTiles, partialTiles, staleTiles, restackTiles;

                    // A tile whose DEM has not been decoded yet is drawn FLAT, at sea level.
                    // Next to tiles that ARE displaced that is a slab of map hanging in space
                    // below the terrain - out of place vertically, in the right place on the
                    // ground, which is exactly what it looks like. Zooming out is when it
                    // happens wholesale: the elevation cache holds the FINER grids of the
                    // previous generation and its lookup only ever walks UP to ancestors, so
                    // every new coarse tile misses until its own DEM tile loads.
                    // Stand on the previous generation's tiles there instead - they still have
                    // their elevation - and if there is nothing to stand in with, draw nothing.
                    // No terrain known for that ground is the honest answer; a false ground at
                    // zero is not, and it also writes depth, so it hides what is behind it.
                    // When NOTHING has elevation (cold start, or ground the DEM does not cover)
                    // the whole scene is flat and internally consistent, so the flat draw stays.
                    // Already resolved above, where the paint's per-tile expectation was decided.
                    int displacedLeaves = 0;
                    for (auto it = drapeTiles.begin(); it != drapeTiles.end(); it++) {
                        displacedLeaves += leafElevation[it->first] ? 1 : 0;
                    }
                    bool sceneDisplaced = displacedLeaves > 0;

                    // SEEDING. A tile that has just entered the cover has no texture, and until its
                    // bake is budgeted in it can only be shown as a flat fill - which is the white
                    // sheet over the terrain on every zoom out, and the reason the map appears to
                    // be rebuilt from nothing each time. But the cache already holds this ground:
                    // the finer tiles this one replaces (zooming out) or a coarser one covering it
                    // (zooming in). Copy them into the new texture straight away - a few textured
                    // quads, nothing like the cost of a bake - and the tile is showing the map from
                    // the frame it appears. Its real bake then replaces the seed when it lands, and
                    // because it is a bake of the same ground the swap is invisible.
                    // Seeds are never SOURCES (findBaked returns baked entries only), so nothing
                    // degrades through repeated copying.
                    static const int DRAPE_SEED_BUDGET = 16;
                    int seedBudget = DRAPE_SEED_BUDGET;
                    int seededTiles = 0;
                    struct SeedSource { unsigned int texture; float dstX, dstY, dstScale, uvX, uvY, uvScale; };
                    auto seedTile = [&](const vt::TileId& tileId, unsigned int texture) {
                        if (seedBudget <= 0 || texture == 0) {
                            return false;
                        }
                        std::vector<SeedSource> sources;
                        // Finer tiles first: they are the ones just replaced, at full detail, and
                        // together they tile this one exactly.
                        std::function<void(const vt::TileId&, int)> collectDescendants = [&](const vt::TileId& parent, int depth) {
                            for (int dy = 0; dy < 2; dy++) {
                                for (int dx = 0; dx < 2; dx++) {
                                    vt::TileId child = parent.getChild(dx, dy);
                                    unsigned int childTexture = _terrainDrapeCache->findBaked(child, 0);
                                    if (childTexture != 0) {
                                        int levels = child.zoom - tileId.zoom;
                                        int span = 1 << levels;
                                        int ix = child.x - (tileId.x << levels);
                                        int iy = child.y - (tileId.y << levels);
                                        // Mirrored y: texture v runs north, the XYZ tile y runs south.
                                        sources.push_back(SeedSource { childTexture, static_cast<float>(ix) / span, static_cast<float>(span - 1 - iy) / span, 1.0f / span, 0.0f, 0.0f, 1.0f });
                                    } else if (depth > 0) {
                                        collectDescendants(child, depth - 1);
                                    }
                                }
                            }
                        };
                        collectDescendants(tileId, 2);
                        if (sources.empty()) {
                            vt::TileId ancestor = tileId;
                            float offsetX = 0.0f, offsetY = 0.0f, scale = 1.0f;
                            for (int level = 0; level < 6 && ancestor.zoom > 0; level++) {
                                int childX = ancestor.x & 1;
                                int childY = 1 - (ancestor.y & 1);
                                ancestor = vt::TileId(ancestor.zoom - 1, ancestor.x >> 1, ancestor.y >> 1);
                                scale *= 0.5f;
                                offsetX = offsetX * 0.5f + childX * 0.5f;
                                offsetY = offsetY * 0.5f + childY * 0.5f;
                                unsigned int ancestorTexture = _terrainDrapeCache->findBaked(ancestor, 0);
                                if (ancestorTexture != 0) {
                                    sources.push_back(SeedSource { ancestorTexture, 0.0f, 0.0f, 1.0f, offsetX, offsetY, scale });
                                    break;
                                }
                            }
                        }
                        if (sources.empty()) {
                            return false; // genuinely new ground: nothing in the cache covers it
                        }
                        beginOffscreen();
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                        glClearColor(drapeClearColor.getR() / 255.0f, drapeClearColor.getG() / 255.0f, drapeClearColor.getB() / 255.0f, drapeClearColor.getA() / 255.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        for (const SeedSource& source : sources) {
                            drapeLayers.front()->blitDrapeTexture(source.texture, source.dstX, source.dstY, source.dstScale, source.uvX, source.uvY, source.uvScale);
                        }
                        seedBudget--;
                        seededTiles++;
                        TerrainDrapeCache::generateMipmaps(texture);
                        return true;
                    };

                    for (auto it = drapeTiles.begin(); it != drapeTiles.end(); it++) {
                        bool needsBake = false;
                        bool hasContent = false;
                        unsigned int texture = _terrainDrapeCache->acquire(it->first, 0, it->second, needsBake, hasContent);
                        if (!hasContent && seedTile(it->first, texture)) {
                            _terrainDrapeCache->markSeeded(it->first, 0);
                            hasContent = true;
                        }
                        bool baked = _terrainDrapeCache->isBaked(it->first, 0);
                        // "Has a texture" is not "shows the map". A zoom out reaches the new
                        // coarse tiles raster-first - the hillshade decodes in one step while the
                        // vector tiles still have a style pass to run - so the first bake of a
                        // tile holds the hillshade and the background fill and nothing else.
                        // Baked, cached, and by the old rule good enough to replace the previous
                        // generation still on screen: the whole map turns into bare relief for
                        // the half-second it takes the vector layers to catch up. That is the
                        // flash. A tile counts as usable only when every layer that has
                        // something for it is actually IN it.
                        std::size_t wantedMask = drapeTileLayerMasks[it->first];
                        std::size_t bakedMask = _terrainDrapeCache->bakedLayerMask(it->first, 0);
                        bool complete = baked && (wantedMask & ~bakedMask) == 0;
                        // A seed already IS the finer generation, composited into this tile's own
                        // texture, so nothing has to be drawn over it.
                        bool showsStandIn = hasContent && !baked;
                        // A tile with no content yet must not be sampled: its texture came from
                        // the recycle pool and still holds another tile's picture. Stand in on the
                        // nearest baked ancestor instead - a flat fill here is a white block, and
                        // during a zoom a whole screen of them flashes on and off.
                        DrapedTile draped { it->first, hasContent ? texture : 0u, 0.0f, 0.0f, 1.0f };
                        if (!hasContent) {
                            vt::TileId ancestor = it->first;
                            float offsetX = 0.0f, offsetY = 0.0f, scale = 1.0f;
                            for (int level = 0; level < 6 && ancestor.zoom > 0; level++) {
                                // Mirror the y index: tile-local y runs northward while the XYZ
                                // tile y runs southward (same convention as the bake sub-rect).
                                int childX = ancestor.x & 1;
                                int childY = 1 - (ancestor.y & 1);
                                ancestor = vt::TileId(ancestor.zoom - 1, ancestor.x >> 1, ancestor.y >> 1);
                                scale *= 0.5f;
                                offsetX = offsetX * 0.5f + childX * 0.5f;
                                offsetY = offsetY * 0.5f + childY * 0.5f;
                                unsigned int ancestorTexture = _terrainDrapeCache->findBaked(ancestor, 0);
                                if (ancestorTexture != 0) {
                                    draped.texture = ancestorTexture;
                                    draped.uvOffsetX = offsetX;
                                    draped.uvOffsetY = offsetY;
                                    draped.uvScale = scale;
                                    break;
                                }
                            }
                        }
                        // No DEM for this tile while the rest of the scene is displaced: its own
                        // surface would be the false flat ground, so it is not drawn at all. Its
                        // descendants still are - they are the generation that HAS elevation.
                        bool skipSurface = sceneDisplaced && !leafElevation[it->first];
                        std::size_t drapedIndex = std::numeric_limits<std::size_t>::max(); // never indexes the list
                        if (!skipSurface) {
                            drapedIndex = drapedTiles.size(); // before the stand-in draws below extend the list
                            drapedTiles.push_back(draped);
                        } else {
                            skippedSurfaces++;
                        }
                        // An ancestor sub-rect already covers a contentless tile, and it is the
                        // better stand-in: the descendants are separate surfaces at a finer tile
                        // zoom, so their meshes coincide with this leaf's but are not the same
                        // triangles and drawn over it they read as tiles sitting slightly off the
                        // terrain - while the ancestor is the SAME mesh with a blurrier texture,
                        // which merely looks soft. Prefer the soft one. A tile whose own surface
                        // is skipped has no ancestor draw at all, so it still needs them.
                        bool showsAncestor = !hasContent && draped.texture != 0 && !skipSurface;
                        if (((!complete && !showsStandIn) || skipSurface) && !showsAncestor) {
                            // Zooming OUT there is no baked ancestor - the cached tiles are the
                            // finer ones underneath. Draw those over the top: same meshes, same
                            // depth, so the ground shows real content instead of a flat fill.
                            // Also for a tile that IS baked but only partly: the previous
                            // generation underneath is a complete picture of the same ground, and
                            // it stays until this tile's own bake is finished.
                            // They MUST come after this tile's own entry, not before it: the
                            // surfaces coincide and the later draw wins, so pushed first they
                            // were buried under the fill they were meant to replace - which is
                            // the whole screen turning white for a moment on every zoom out.
                            // Several levels deep, because one gesture crosses several zooms and
                            // the cache then holds tiles two or three levels finer than this one.
                            std::function<void(const vt::TileId&, int)> drawBakedDescendants = [&](const vt::TileId& tileId, int depth) {
                                for (int dy = 0; dy < 2; dy++) {
                                    for (int dx = 0; dx < 2; dx++) {
                                        vt::TileId child = tileId.getChild(dx, dy);
                                        unsigned int childTexture = _terrainDrapeCache->findBaked(child, 0);
                                        if (childTexture != 0 && sceneDisplaced && !hasElevationData(child)) {
                                            childTexture = 0; // cached picture, but no ground to put it on
                                        }
                                        if (childTexture != 0 && (wantedMask & ~_terrainDrapeCache->bakedLayerMask(child, 0)) != 0) {
                                            childTexture = 0; // as incomplete as the tile it would stand in for
                                        }
                                        if (childTexture != 0) {
                                            drapedTiles.push_back(DrapedTile { child, childTexture, 0.0f, 0.0f, 1.0f });
                                        } else if (depth > 0) {
                                            drawBakedDescendants(child, depth - 1);
                                        }
                                    }
                                }
                            };
                            drawBakedDescendants(it->first, 2);
                        }
                        if (!needsBake) {
                            continue;
                        }
                        BakeRequest request { it->first, it->second, drapedIndex };
                        if (baked && _terrainDrapeCache->isStale(it->first, 0)) {
                            restackTiles.push_back(request);     // shows the previous layer stack
                        } else if (baked && !complete) {
                            partialTiles.push_back(request);     // shows part of the stack: a layer is simply absent
                        } else if (baked) {
                            staleTiles.push_back(request);       // shows its own, older, picture
                        } else if (hasContent || draped.texture != 0) {
                            standInTiles.push_back(request);     // seeded, or standing in on an ancestor
                        } else {
                            blankTiles.push_back(request);       // shows a flat fill: a hole
                        }
                    }
                    auto bakeTile = [&](const BakeRequest& request) {
                        bool needsBake = false, hasContent = false;
                        unsigned int texture = _terrainDrapeCache->acquire(request.tileId, 0, request.fingerprint, needsBake, hasContent);
                        beginOffscreen();
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                        glClearColor(drapeClearColor.getR() / 255.0f, drapeClearColor.getG() / 255.0f, drapeClearColor.getB() / 255.0f, drapeClearColor.getA() / 255.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        // Layer order matters: later layers composite over earlier ones, which is
                        // why the owner clears and the bakers do not.
                        std::size_t bakedMask = 0;
                        for (std::size_t i = 0; i < drapeLayers.size(); i++) {
                            int primitives = drapeLayers[i]->bakeDrapeTile(request.tileId);
                            bakedPrimitives += primitives;
                            if (primitives > 0 && i < sizeof(std::size_t) * 8) {
                                bakedMask |= static_cast<std::size_t>(1) << i;
                            }
                        }
                        // What went in, not what was asked for: a layer that turned out to have
                        // nothing stays missing from the mask, so the tile is re-baked as soon as
                        // that layer does have something.
                        // A terrain paint is not in that mask (it reports no tiles), so a tile it
                        // could not paint - its elevation texture was still being prepared - would
                        // otherwise keep its unshaded picture for as long as it stays cached. Mark
                        // such a tile with a fingerprint that cannot match, which makes it STALE:
                        // it is drawn, with what it has, and baked again at the first opportunity.
                        std::size_t bakedFingerprint = request.fingerprint;
                        for (std::size_t i = 0; i < drapeLayers.size() && i < sizeof(std::size_t) * 8; i++) {
                            if (drapeLayers[i]->paintsEveryDrapeTile() && (bakedMask & (static_cast<std::size_t>(1) << i)) == 0) {
                                bakedFingerprint = ~request.fingerprint;
                                break;
                            }
                        }
                        _terrainDrapeCache->markBaked(request.tileId, 0, bakedFingerprint, bakedMask);
                        TerrainDrapeCache::generateMipmaps(texture);
                        bakedTiles++;
                        bakedThisFrame++;
                        VT_STAT_INC(drapeBakes);
                        if (request.drapedIndex < drapedTiles.size()) {
                            drapedTiles[request.drapedIndex] = DrapedTile { request.tileId, texture, 0.0f, 0.0f, 1.0f }; // baked now, safe to sample
                        }
                    };
                    // The counts above say how URGENT each class is; how many of them a frame can
                    // actually afford is a time question, and the answer is not the same on two
                    // devices: the same bake was measured at ~2 ms on the emulator and 25+ ms on an
                    // Adreno 610, so eight of them are a hiccup on one and a third of a second on
                    // the other. Bake in priority order until the frame's bake time is spent (one
                    // bake always goes through, or a slow device would never fill a tile in).
                    // Same "did the camera move since the previous frame" test the occlusion
                    // read-back throttle uses (TerrainRenderer::updateDepthBuffer).
                    const cglib::mat4x4<double>& bakeMVPMatrix = viewState.getModelviewProjectionMat();
                    bool bakeCameraMoving = !(_drapeBakeLastMVPMatrix == bakeMVPMatrix);
                    _drapeBakeLastMVPMatrix = bakeMVPMatrix;
                    double bakeTimeBudget = (bakeCameraMoving ? DRAPE_BAKE_TIME_BUDGET : DRAPE_BAKE_TIME_BUDGET_STILL);
                    std::chrono::steady_clock::time_point bakeStart = std::chrono::steady_clock::now();
                    VT_STAT_ADD(drapeBakeQueued, static_cast<long long>(blankTiles.size() + restackTiles.size() + standInTiles.size() + partialTiles.size() + staleTiles.size()));
                    auto bakeTimeLeft = [&bakeStart, &bakedThisFrame, bakeTimeBudget]() {
                        if (bakedThisFrame == 0) {
                            return true; // always make progress
                        }
                        return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - bakeStart).count() < bakeTimeBudget;
                    };
                    int budget = DRAPE_BAKE_BUDGET_BLANK;
                    for (auto it = blankTiles.begin(); it != blankTiles.end() && budget > 0 && bakeTimeLeft(); it++, budget--) {
                        bakeTile(*it);
                    }
                    budget = DRAPE_BAKE_BUDGET_RESTACK;
                    for (auto it = restackTiles.begin(); it != restackTiles.end() && budget > 0 && bakeTimeLeft(); it++, budget--) {
                        bakeTile(*it);
                    }
                    budget = DRAPE_BAKE_BUDGET_STANDIN;
                    for (auto it = standInTiles.begin(); it != standInTiles.end() && budget > 0 && bakeTimeLeft(); it++, budget--) {
                        bakeTile(*it);
                    }
                    budget = DRAPE_BAKE_BUDGET_PARTIAL;
                    for (auto it = partialTiles.begin(); it != partialTiles.end() && budget > 0 && bakeTimeLeft(); it++, budget--) {
                        bakeTile(*it);
                    }
                    budget = DRAPE_BAKE_BUDGET_STALE;
                    for (auto it = staleTiles.begin(); it != staleTiles.end() && budget > 0 && bakeTimeLeft(); it++, budget--) {
                        bakeTile(*it);
                    }
                    // Baking is rationed over several frames, so it only finishes if those frames
                    // happen. Nothing else asks for them: the tile arrived, its layer redrew once,
                    // and the budget took the first few tiles. On a map that now goes idle the rest
                    // of the queue simply stopped - a layer switched on (a hillshade, a raster)
                    // appeared a few tiles at a time and only while the user kept panning. Keep
                    // asking for frames while there is baking left to do, and stop when there is not.
                    if (blankTiles.size() > DRAPE_BAKE_BUDGET_BLANK
                        || standInTiles.size() > DRAPE_BAKE_BUDGET_STANDIN
                        || partialTiles.size() > DRAPE_BAKE_BUDGET_PARTIAL
                        || staleTiles.size() > DRAPE_BAKE_BUDGET_STALE) {
                        requestRedraw();
                    }
                    VT_STAT_ADD(drapeBakeNs, std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - bakeStart).count());
                    if (bakeStarted) {
                        // Detach before sampling: a texture left attached to a framebuffer counts
                        // as a render target, and sampling it in the same frame is undefined - on
                        // the emulator every drape texture then reads back black.
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
                        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
                        glViewport(0, 0, viewState.getWidth(), viewState.getHeight());
                    }

                    // Directional shadows over the drape cover.
                    ResolvedLighting lighting;
                    std::array<double, TerrainShadowMap::MAX_CASCADES> shadowTexelMeters = { };
                    applyTerrainShadows(drapeLayers, drapeTileIds, terrainOptions, viewState, prevFBO, bakedThisFrame > 0, true, lighting, shadowTexelMeters);

                    // The shared surface is the only depth-writing terrain geometry.
                    // GL_LEQUAL, not the default GL_LESS: the global terrain background drawn
                    // just above uses the SAME meshes and has already written their depth, so a
                    // GL_LESS surface draw is rejected everywhere and the drape never reaches
                    // the screen - the terrain then shows the background colour and nothing else.
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(GL_LEQUAL);
                    glDepthMask(GL_TRUE);
                    glDisable(GL_CULL_FACE); // displaced surfaces can face away near ridge crests
                    for (auto it = drapedTiles.begin(); it != drapedTiles.end(); it++) {
                        // Every drape tile gets a surface, always. The surface is the terrain's
                        // only depth writer, so a tile skipped because its bake has not landed
                        // yet leaves a depth hole - and vector elements and billboards behind the
                        // terrain there pop into view for exactly those frames.
                        if (it->texture != 0) {
                            surfaceDraws += drapeLayers.front()->renderDrapedSurface(it->tileId, it->texture, it->uvOffsetX, it->uvOffsetY, it->uvScale);
                        } else {
                            surfaceDraws += drapeLayers.front()->renderDrapedSurfaceFill(it->tileId, drapeClearColor);
                            filledSurfaces++;
                        }
                    }
                    glEnable(GL_CULL_FACE);
                    glDepthFunc(GL_LESS);
                    glDepthMask(GL_FALSE);
                    _terrainDrapeCache->endFrame();

                    // Ground with nothing on it is the one failure the user sees immediately, and
                    // it has two quite different causes - a tile drawn in the flat clear colour,
                    // or a tile not drawn at all because it has no elevation yet. Log the frame it
                    // happens in, with the state that produced it, rather than in the periodic
                    // dump that a half-second flash never coincides with.
                    if (filledSurfaces > 0 || skippedSurfaces > 0) {
                        static int emptyGroundFrame = 0, lastEmptyGroundLog = -1000;
                        emptyGroundFrame++;
                        if (emptyGroundFrame - lastEmptyGroundLog > 30) {
                            lastEmptyGroundLog = emptyGroundFrame;
                            Log::Infof("MapRenderer: RTT drape EMPTY GROUND - %d flat fills, %d tiles skipped for missing elevation, of %d drawn (%d leaves, split level %d, camera zoom %.2f); seeded %d, blank %d, stand-in %d, partial %d, stale %d",
                                filledSurfaces, skippedSurfaces, static_cast<int>(drapedTiles.size()),
                                static_cast<int>(drapeTiles.size()), drapeZoom, viewState.getZoom(), seededTiles,
                                static_cast<int>(blankTiles.size()), static_cast<int>(standInTiles.size()),
                                static_cast<int>(partialTiles.size()), static_cast<int>(staleTiles.size()));
                        }
                    }

                    // One-time state dump: confirms whether the RTT path is actually live, and
                    // with how many layers/tiles, rather than being inferred from symptoms.
                    static double drapeMsSum = 0;
                    static double drapeMsMax = 0;
                    static int drapeMsCount = 0;
                    double drapeMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - drapeStart).count();
                    FRAME_PROF_SET(drapeMs, drapeMs);
                    FRAME_PROF_GPU_END();
                    drapeMsSum += drapeMs;
                    drapeMsMax = std::max(drapeMsMax, drapeMs);
                    drapeMsCount++;
                    static int drapeStateFrame = 0;
                    if ((drapeStateFrame++ % 60) == 0 && drapedTiles.size() > 0) {
                        Log::Infof("MapRenderer: RTT drape cost avg %.1f ms, max %.1f ms over %d frames", drapeMsSum / std::max(1, drapeMsCount), drapeMsMax, drapeMsCount);
                        drapeMsSum = 0; drapeMsMax = 0; drapeMsCount = 0;
                    }
                    if ((drapeStateFrame % 600) == 1 && drapedTiles.size() > 0) {
                        int minZoom = 99, maxZoom = -1;
                        for (auto it2 = drapedTiles.begin(); it2 != drapedTiles.end(); it2++) {
                            minZoom = std::min(minZoom, it2->tileId.zoom);
                            maxZoom = std::max(maxZoom, it2->tileId.zoom);
                        }
                        Log::Infof("MapRenderer: RTT drape tiles zoom %d..%d, count %d", minZoom, maxZoom, static_cast<int>(drapedTiles.size()));
                        // Queue sizes say which of the four states the cover is actually in - a
                        // standing 'partial' backlog means the bake never catches up with the
                        // layers, which looks like the whole map stuck on bare hillshade.
                        Log::Infof("MapRenderer: RTT drape cover - split level %d (collected up to %d, camera zoom %.2f), leaves %d",
                            drapeZoom, maxCollectedZoom, viewState.getZoom(), static_cast<int>(drapeTiles.size()));
                        Log::Infof("MapRenderer: RTT drape seeded %d tiles from cache this frame", seededTiles);
                        Log::Infof("MapRenderer: RTT drape queues - blank %d, stand-in %d, partial %d, stale %d, tiles without elevation %d of %d",
                            static_cast<int>(blankTiles.size()), static_cast<int>(standInTiles.size()),
                            static_cast<int>(partialTiles.size()), static_cast<int>(staleTiles.size()),
                            static_cast<int>(drapeTiles.size()) - displacedLeaves, static_cast<int>(drapeTiles.size()));
                        Log::Infof("MapRenderer: RTT drape ACTIVE - layers %d, collected tiles %d, drawn tiles %d, resolution %d, baked %d tiles / %d primitives, surface draws %d (%d unbaked fills)",
                            static_cast<int>(drapeLayers.size()), static_cast<int>(collectedTiles.size()),
                            static_cast<int>(drapedTiles.size()), resolution, bakedTiles, bakedPrimitives, surfaceDraws, filledSurfaces);
                        Log::Infof("MapRenderer: shadow caster passes %d over %d frames, %d cascades, %d caster tiles per pass, %.1f ms per pass, %d extrusion draws per pass, texels per cascade %.1f/%.1f/%.1f/%.1f m (camera zoom %.2f tilt %.1f)", shadowPasses, drapeStateFrame, _shadowMapCascades, shadowCasterDraws / std::max(1, shadowPasses), shadowMsSum / std::max(1, shadowPasses), shadowExtrusionDraws / std::max(1, shadowPasses), shadowTexelMeters[0], shadowTexelMeters[1], shadowTexelMeters[2], shadowTexelMeters[3], viewState.getZoom(), viewState.getTilt());
                    }
                    }
                    catch (const std::exception& ex) {
                        // A shader that fails to compile or link throws from the render thread.
                        // Losing the drape is bad; taking the process down with it is worse.
                        Log::Errorf("MapRenderer: RTT drape failed: %s", ex.what());
                        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
                        glViewport(0, 0, viewState.getWidth(), viewState.getHeight());
                    }
                }
            }
        }
        if (drapeLayers.empty() && !sharedGroundActive) {
            std::vector<std::shared_ptr<TileLayer> > allTileLayers;
            for (const std::shared_ptr<Layer>& layer : layers) {
                layer->collectDrapeLayers(allTileLayers, viewState);
            }
            for (const std::shared_ptr<TileLayer>& tileLayer : allTileLayers) {
                tileLayer->setExternalDrapeTarget(false);
                // No shared ground either (terrain off, or a stack with no drapeable layer):
                // release the cover so a layer left holding one from a terrain frame does not
                // keep suppressing its own depth pre-pass and drawing on tiles nobody covers.
                tileLayer->setTerrainGroundTiles(std::vector<vt::TileId>(), std::vector<int>());
            }
            if (terrainMode) {
                static bool noDrapeLogged = false;
                if (!noDrapeLogged) {
                    noDrapeLogged = true;
                    Log::Info("MapRenderer: neither the RTT drape nor a shared ground is active in terrain mode - falling back to the per-layer depth path");
                }
            }
        }

        // Create new billboard sorter instance
        std::vector<std::shared_ptr<BillboardDrawData> > billboardDrawDatas;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            billboardDrawDatas.reserve(_billboardDrawDatas.size());
        }
        BillboardSorter billboardSorter(billboardDrawDatas);

        // Do base drawing pass
        bool needRedraw = false;
        FRAME_PROF_NOW(profLayerStart);
        FRAME_PROF_GPU_BEGIN(SECTION_LAYERS);
        unsigned int redrawMask = 0; // which layer asked, so a map that never settles can be traced
        for (std::size_t i = 0; i < layers.size(); i++) {
            const std::shared_ptr<Layer>& layer = layers[i];
            if (viewState.getHorizontalLayerOffsetDir() != 0) {
                layer->offsetLayerHorizontally(viewState.getHorizontalLayerOffsetDir() * Const::WORLD_SIZE);
            }

            if (layer->onDrawFrame(deltaSeconds, billboardSorter, viewState)) {
                needRedraw = true;
                redrawMask |= 1u << std::min<std::size_t>(i, 15);
            }
        }

        FRAME_PROF_ADD(layerMs, profLayerStart);

        // Do 3D drawing pass
        FRAME_PROF_NOW(profLayer3DStart);
        FRAME_PROF_GPU_BEGIN(SECTION_LAYERS3D);
        for (std::size_t i = 0; i < layers.size(); i++) {
            if (layers[i]->onDrawFrame3D(deltaSeconds, billboardSorter, viewState)) {
                needRedraw = true;
                redrawMask |= 1u << (16 + std::min<std::size_t>(i, 15));
            }
        }

        FRAME_PROF_ADD(layer3DMs, profLayer3DStart);

        // Sort billboards, calculate rotation state
        FRAME_PROF_NOW(profBillboardStart);
        FRAME_PROF_GPU_BEGIN(SECTION_BILLBOARDS);
        billboardSorter.sort(viewState);
        
        // Draw billboards, grouped by layer renderer
        if (!billboardDrawDatas.empty()) {
            glDisable(GL_DEPTH_TEST);

            _billboardDrawDataBuffer.clear();
            std::shared_ptr<BillboardRenderer> prevRenderer;
            for (const std::shared_ptr<BillboardDrawData>& drawData : billboardDrawDatas) {
                if (std::shared_ptr<BillboardRenderer> renderer = drawData->getRenderer().lock()) {
                    if (prevRenderer && prevRenderer != renderer) {
                        prevRenderer->onDrawFrameSorted(deltaSeconds, _billboardDrawDataBuffer, viewState);
                        _billboardDrawDataBuffer.clear();
                    }
            
                    _billboardDrawDataBuffer.push_back(drawData);
                    prevRenderer = renderer;
                }
            }
            if (prevRenderer) {
                prevRenderer->onDrawFrameSorted(deltaSeconds, _billboardDrawDataBuffer, viewState);
            }

            glEnable(GL_DEPTH_TEST);
        }

        FRAME_PROF_ADD(billboardMs, profBillboardStart);
        FRAME_PROF_GPU_END();

        // Store the active billboard draw data list
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _billboardDrawDatas = std::move(billboardDrawDatas);
        }
    
        // Redraw, if needed
        if (needRedraw) {
            requestRedraw();
        }
        // A map that is standing still should stop asking for frames. When it does not - which is
        // invisible except as battery drain and a log line every 60 frames - say who is asking:
        // the low half of the mask is the base pass, the high half the 3D pass, one bit per layer.
        // If the frames keep coming but almost none of them came from a layer, the driver is an
        // external requestRedraw (a tile finishing, the elevation version moving, a camera event)
        // rather than an animation, which is a different bug with a different fix.
        {
            static int frames = 0;
            static int layerRedrawFrames = 0;
            static unsigned int redrawMaskSum = 0;
            frames++;
            if (needRedraw) {
                layerRedrawFrames++;
                redrawMaskSum |= redrawMask;
            }
            if (frames >= 300) {
                Log::Infof("MapRenderer: %d frames drawn, %d asked for by a layer, layer mask 0x%08x (low 16 bits base pass, high 16 bits 3D pass)", frames, layerRedrawFrames, redrawMaskSum);
                logRedrawSources();
                frames = 0;
                layerRedrawFrames = 0;
                redrawMaskSum = 0;
            }
        }
    }
    
    void MapRenderer::handleRendererCaptureCallbacks() {
        int width, height;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            width = _viewState.getWidth();
            height = _viewState.getHeight();
        }
        std::shared_ptr<Bitmap> captureBitmap;
        
        std::vector<std::pair<DirectorPtr<RendererCaptureListener>, bool> > rendererCaptureListeners;
        {
            std::lock_guard<std::mutex> lock(_rendererCaptureListenersMutex);
            _rendererCaptureListeners.swap(rendererCaptureListeners);
        }

        bool callbacksPending = false;
        for (std::size_t i = 0; i < rendererCaptureListeners.size(); i++) {
            const DirectorPtr<RendererCaptureListener>& listener = rendererCaptureListeners[i].first;
            bool waitWhileUpdating = rendererCaptureListeners[i].second;
            if (waitWhileUpdating) {
                bool layersUpdating = false;
                for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
                    if (layer->isUpdateInProgress()) {
                        layersUpdating = true;
                        break;
                    }
                }
                if (_redrawPending || layersUpdating || !_cullWorker->isIdle() || !_billboardPlacementWorker->isIdle() || !_vtLabelPlacementWorker->isIdle()) {
                    std::lock_guard<std::mutex> lock(_rendererCaptureListenersMutex);
                    _rendererCaptureListeners.push_back(rendererCaptureListeners[i]);
                    callbacksPending = true;
                    continue;
                }
            }
            
            if (!captureBitmap) {
                std::vector<unsigned char> data(4 * width * height);
                glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, &data[0]);
                captureBitmap = std::make_shared<Bitmap>(data.data(), width, height, ColorFormat::COLOR_FORMAT_RGBA, -4 * width);
            }
            
            listener->onMapRendered(captureBitmap);
        }
        if (callbacksPending) {
            requestRedraw();
        }
    }

    MapRenderer::OptionsListener::OptionsListener(const std::shared_ptr<MapRenderer>& mapRenderer) : _mapRenderer(mapRenderer)
    {
    }

    void MapRenderer::OptionsListener::onOptionChanged(const std::string& optionName) {
        if (auto mapRenderer = _mapRenderer.lock()) {
            bool updateView = false;

            if (optionName == "AmbientLightColor" || optionName == "MainLightColor" || optionName == "MainLightDirection" || optionName == "ClearColor" || optionName == "SkyColor") {
                updateView = true;
            }
            
            if (optionName == "RenderProjectionMode" || optionName == "BaseProjection" || optionName == "ZoomRange" || optionName == "PanBounds" || optionName == "RestrictedPanning") {
                std::lock_guard<std::recursive_mutex> lock(mapRenderer->_mutex);
                mapRenderer->_viewState.calculateViewState(*mapRenderer->_options);
                mapRenderer->_viewState.clampZoom(*mapRenderer->_options);
                mapRenderer->_viewState.clampFocusPos(*mapRenderer->_options);
                updateView = true;
            }

            if (optionName == "TileDrawSize" || optionName == "DPI" || optionName == "DrawDistance" || optionName == "FieldOfViewY" || optionName == "FocusPointOffset") {
                std::lock_guard<std::recursive_mutex> lock(mapRenderer->_mutex);
                mapRenderer->_viewState.calculateViewState(*mapRenderer->_options);
                updateView = true;
            }

            if (optionName.substr(0, 14) == "TerrainOptions") {
                // Terrain changes (enabled state, exaggeration, mesh resolution, min zoom)
                // require a new cull pass so that tile layers detect the configuration change
                // and rebuild their tiles with/without terrain displacement
                updateView = true;
            }

            if (updateView) {
                mapRenderer->viewChanged(false);
            } else {
                mapRenderer->requestRedraw();
            }
        }
    }

    const int MapRenderer::BILLBOARD_PLACEMENT_TASK_DELAY = 200;

    const int MapRenderer::VT_LABEL_PLACEMENT_TASK_DELAY = 200;
    // A quarter of a zoom level: labels are drawn at the same pixel size at every zoom, so what
    // changes is how much MAP is under them - a quarter level is about 20% more room, which is
    // enough to fit a name that did not fit before.
    const float MapRenderer::LABEL_PLACEMENT_ZOOM_THRESHOLD = 0.25f;

    const int MapRenderer::ELEVATION_REFRESH_DELAY = 500;

    const std::string MapRenderer::BLEND_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec2 a_coord;
        uniform mat4 u_mvpMat;
        void main() {
            gl_Position = u_mvpMat * vec4(a_coord, 0.0, 1.0);
        }
    )GLSL";

    const std::string MapRenderer::POST_PROCESS_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec2 a_coord;
        void main() {
            gl_Position = vec4(a_coord, 0.0, 1.0);
        }
    )GLSL";

    const std::string MapRenderer::BLEND_FRAGMENT_SHADER = R"GLSL(
        #version 100
        precision mediump float;
        uniform sampler2D u_tex;
        uniform lowp vec4 u_color;
        uniform mediump vec2 u_invScreenSize;
        void main() {
            vec4 texColor = texture2D(u_tex, gl_FragCoord.xy * u_invScreenSize);
            gl_FragColor = texColor * u_color;
        }
    )GLSL";
}
