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
#include "projections/Projection.h"
#include "projections/ProjectionSurface.h"
#include "renderers/BillboardRenderer.h"
#include "renderers/MapRendererListener.h"
#include "renderers/RendererCaptureListener.h"
#include "renderers/RedrawRequestListener.h"
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
#include "terrain/ElevationManager.h"
#include "renderers/utils/Shader.h"
#include "renderers/utils/Texture.h"
#include "renderers/workers/BillboardPlacementWorker.h"
#include "renderers/workers/VTLabelPlacementWorker.h"
#include "renderers/workers/CullWorker.h"
#include "utils/Const.h"
#include "utils/Log.h"
#include "utils/ThreadUtils.h"

#include <algorithm>
#include <cstdio>
#include <vector>

namespace carto {

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
        
    void MapRenderer::requestRedraw() const {
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
        initializeRenderState();
        // The shader sky replaces the legacy sky band when it draws.
        bool skyDrawn = _skyRenderer.onDrawFrame(viewState);
        _backgroundRenderer.onDrawFrame(viewState, !skyDrawn);
        drawLayers(deltaSeconds, viewState);

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

        GLContext::CheckGLError("MapRenderer::onDrawFrame");
    }
    
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
        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
            int delayTime = layer->getCullDelay();
            _cullWorker->init(layer, delay ? delayTime : 0);
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
    
    void MapRenderer::drawLayers(float deltaSeconds, const ViewState& viewState) {
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
                        if (cameraPos(2) > 0 && cameraPos(2) < minCameraZ) {
                            CameraZoomEvent zoomEvent;
                            zoomEvent.setZoomDelta(static_cast<float>(std::log2(cameraPos(2) / minCameraZ))); // negative: zoom out onto the clearance
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
        if (terrainMode) {
            if (auto terrainOptions = _options->getTerrainOptions()) {
                if (terrainOptions->isDrapeFillsEnabled()) {
                    // Layers report their own drapeable tile layers, so a composite layer can
                    // contribute its children (hillshade/raster slots, style-layer groups) in
                    // draw order rather than only its own group-0 renderer.
                    for (const std::shared_ptr<Layer>& layer : layers) {
                        layer->collectDrapeLayers(drapeLayers);
                    }
                }
                // A single stack for now: the usual configuration (hillshade under vector tiles)
                // is contiguous and entirely drapeable. Splitting into several stacks only
                // matters once a non-drapeable layer sits between drapeable ones.
                if (!drapeLayers.empty()) {
                    if (!_terrainDrapeCache) {
                        _terrainDrapeCache = std::make_unique<TerrainDrapeCache>();
                    }
                    _terrainDrapeCache->setResolution(terrainOptions->getDrapeResolution());

                    // Every participating layer's render tiles must exist before any of them
                    // bakes, so start their frames first.
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        tileLayer->prepareTerrainDrapeFrame(deltaSeconds, viewState);
                    }

                    std::map<vt::TileId, std::size_t> collectedTiles;
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        tileLayer->collectDrapeTiles(collectedTiles);
                    }

                    // The collected set is a UNION across layers, and layers do not agree on a
                    // zoom level - a hillshade limited by its DEM max zoom yields coarser tiles
                    // than a vector tile layer. Drawing a surface for every tile in that union
                    // would stack a coarse surface and the finer ones covering the same ground on
                    // top of each other, and they fight. Normalize to a single non-overlapping
                    // cover, keeping the FINEST tile for any given ground area; coarser layers
                    // still contribute to it through the ancestor sub-rect bake.
                    auto covers = [](const vt::TileId& tileId, const vt::TileId& other) {
                        if (tileId.zoom >= other.zoom) {
                            return false; // strict ancestor only
                        }
                        int deltaZoom = other.zoom - tileId.zoom;
                        return (other.x >> deltaZoom) == tileId.x && (other.y >> deltaZoom) == tileId.y;
                    };
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
                            hasCoarserTile = covers(it2->first, it->first);
                        }
                        if (!hasCoarserTile) {
                            pending.push_back(it->first); // top of a subtree; its descendants follow from the split
                        }
                    }
                    static const std::size_t MAX_DRAPE_TILES = 256; // splitting is bounded; a runaway cover is not worth drawing
                    std::vector<vt::TileId> leaves;
                    while (!pending.empty() && leaves.size() + pending.size() <= MAX_DRAPE_TILES) {
                        vt::TileId tileId = pending.back();
                        pending.pop_back();
                        bool hasFinerTile = false;
                        for (auto it = collectedTiles.begin(); it != collectedTiles.end() && !hasFinerTile; it++) {
                            hasFinerTile = covers(tileId, it->first);
                        }
                        if (!hasFinerTile) {
                            leaves.push_back(tileId);
                            continue;
                        }
                        for (int dy = 0; dy < 2; dy++) {
                            for (int dx = 0; dx < 2; dx++) {
                                pending.push_back(tileId.getChild(dx, dy));
                            }
                        }
                    }
                    leaves.insert(leaves.end(), pending.begin(), pending.end()); // cap hit: keep them coarse rather than lose the ground
                    std::map<vt::TileId, std::size_t> drapeTiles;
                    for (const vt::TileId& tileId : leaves) {
                        // Fold in every collected tile that will bake here - the leaf itself and
                        // every coarser tile covering it - so the fingerprint tracks its content.
                        std::size_t fingerprint = 0;
                        auto exactIt = collectedTiles.find(tileId);
                        if (exactIt != collectedTiles.end()) {
                            fingerprint = exactIt->second;
                        }
                        for (auto it = collectedTiles.begin(); it != collectedTiles.end(); it++) {
                            if (covers(it->first, tileId)) {
                                fingerprint ^= it->second + 0x9e3779b9 + (fingerprint << 6) + (fingerprint >> 2);
                            }
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
                    try {
                    _terrainDrapeCache->beginFrame();
                    std::vector<std::pair<vt::TileId, unsigned int> > drapedTiles;
                    drapedTiles.reserve(drapeTiles.size());
                    int resolution = _terrainDrapeCache->getResolution();
                    bool bakeStarted = false;
                    // Cumulative since start: bakes are cached, so a per-frame count is 0 on most
                    // frames and says nothing about whether baking ever produced anything.
                    static int bakedTiles = 0, bakedPrimitives = 0;
                    int surfaceDraws = 0;
                    for (auto it = drapeTiles.begin(); it != drapeTiles.end(); it++) {
                        bool needsBake = false;
                        unsigned int texture = _terrainDrapeCache->acquire(it->first, 0, it->second, needsBake);
                        drapedTiles.emplace_back(it->first, texture);
                        if (!needsBake) {
                            continue;
                        }
                        if (!bakeStarted) {
                            glBindFramebuffer(GL_FRAMEBUFFER, _terrainDrapeCache->getFrameBuffer());
                            glViewport(0, 0, resolution, resolution);
                            glDisable(GL_DEPTH_TEST);
                            glDepthMask(GL_FALSE);
                            glDisable(GL_STENCIL_TEST);
                            bakeStarted = true;
                        }
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
                        glClearColor(drapeClearColor.getR() / 255.0f, drapeClearColor.getG() / 255.0f, drapeClearColor.getB() / 255.0f, drapeClearColor.getA() / 255.0f);
                        glClear(GL_COLOR_BUFFER_BIT);
                        // Layer order matters: later layers composite over earlier ones, which is
                        // why the owner clears and the bakers do not.
                        for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                            bakedPrimitives += tileLayer->bakeDrapeTile(it->first);
                        }
                        bakedTiles++;
                    }
                    if (bakeStarted) {
                        // Detach before sampling: a texture left attached to a framebuffer counts
                        // as a render target, and sampling it in the same frame is undefined - on
                        // the emulator every drape texture then reads back black.
                        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
                        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
                        glViewport(0, 0, viewState.getWidth(), viewState.getHeight());
                    }

                    // Directional shadows. The caster pass draws exactly the terrain surfaces
                    // that are about to be drawn on screen, from the sun, into a packed-depth
                    // texture; the surface shader then looks itself up in it. Casters and
                    // receivers share one vertex shader and one elevation fetch, so the shadow
                    // geometry cannot disagree with the rendered geometry.
                    float shadowStrength = 0.0f;
                    unsigned int shadowTexture = 0;
                    int shadowMapSize = 0;
                    float shadowBias = 0.0f;
                    cglib::mat4x4<double> lightViewProj = cglib::mat4x4<double>::identity();
                    if (std::shared_ptr<LightOptions> lightOptions = _options->getLightOptions()) {
                        if (lightOptions->isTerrainLightingEnabled() && lightOptions->getShadowStrength() > 0.0f && !drapeTileIds.empty()) {
                            if (!_terrainShadowMap) {
                                _terrainShadowMap = std::make_unique<TerrainShadowMap>();
                            }
                            _terrainShadowMap->setSize(lightOptions->getShadowMapSize());
                            if (drapeLayers.front()->calculateShadowViewProj(drapeTileIds, lightOptions->getSunDirection(), lightViewProj)) {
                                if (_terrainShadowMap->beginPass()) {
                                    for (const vt::TileId& tileId : drapeTileIds) {
                                        drapeLayers.front()->renderShadowCasters(tileId, lightViewProj);
                                    }
                                    _terrainShadowMap->endPass(prevFBO, viewState.getWidth(), viewState.getHeight());
                                    shadowTexture = _terrainShadowMap->getTexture();
                                    shadowMapSize = _terrainShadowMap->getSize();
                                    shadowStrength = lightOptions->getShadowStrength();
                                    shadowBias = lightOptions->getShadowBias();
                                }
                            }
                        }
                    }
                    for (const std::shared_ptr<TileLayer>& tileLayer : drapeLayers) {
                        tileLayer->setTerrainShadowMap(shadowTexture, shadowMapSize, shadowBias, shadowStrength, lightViewProj);
                    }

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
                        surfaceDraws += drapeLayers.front()->renderDrapedSurface(it->first, it->second);
                    }
                    glEnable(GL_CULL_FACE);
                    glDepthFunc(GL_LESS);
                    glDepthMask(GL_FALSE);
                    _terrainDrapeCache->endFrame();

                    // One-time state dump: confirms whether the RTT path is actually live, and
                    // with how many layers/tiles, rather than being inferred from symptoms.
                    static int drapeStateFrame = 0;
                    if ((drapeStateFrame++ % 120) == 0 && drapedTiles.size() > 0) {
                        int minZoom = 99, maxZoom = -1;
                        for (auto it2 = drapedTiles.begin(); it2 != drapedTiles.end(); it2++) {
                            minZoom = std::min(minZoom, it2->first.zoom);
                            maxZoom = std::max(maxZoom, it2->first.zoom);
                        }
                        Log::Infof("MapRenderer: RTT drape tiles zoom %d..%d, count %d", minZoom, maxZoom, static_cast<int>(drapedTiles.size()));
                        Log::Infof("MapRenderer: RTT drape ACTIVE - layers %d, collected tiles %d, drawn tiles %d, resolution %d, baked %d tiles / %d primitives, surface draws %d",
                            static_cast<int>(drapeLayers.size()), static_cast<int>(collectedTiles.size()),
                            static_cast<int>(drapedTiles.size()), resolution, bakedTiles, bakedPrimitives, surfaceDraws);
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
        if (drapeLayers.empty()) {
            std::vector<std::shared_ptr<TileLayer> > allTileLayers;
            for (const std::shared_ptr<Layer>& layer : layers) {
                layer->collectDrapeLayers(allTileLayers);
            }
            for (const std::shared_ptr<TileLayer>& tileLayer : allTileLayers) {
                tileLayer->setExternalDrapeTarget(false);
            }
            if (terrainMode) {
                static bool noDrapeLogged = false;
                if (!noDrapeLogged) {
                    noDrapeLogged = true;
                    Log::Info("MapRenderer: RTT drape INACTIVE in terrain mode - falling back to the per-layer depth path");
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
        for (const std::shared_ptr<Layer>& layer : layers) {
            if (viewState.getHorizontalLayerOffsetDir() != 0) {
                layer->offsetLayerHorizontally(viewState.getHorizontalLayerOffsetDir() * Const::WORLD_SIZE);
            }

            needRedraw = layer->onDrawFrame(deltaSeconds, billboardSorter, viewState) || needRedraw;
        }
        
        // Do 3D drawing pass
        for (const std::shared_ptr<Layer>& layer : layers) {
            needRedraw = layer->onDrawFrame3D(deltaSeconds, billboardSorter, viewState) || needRedraw;
        }
        
        // Sort billboards, calculate rotation state
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

        // Store the active billboard draw data list
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            _billboardDrawDatas = std::move(billboardDrawDatas);
        }
    
        // Redraw, if needed
        if (needRedraw) {
            requestRedraw();
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
