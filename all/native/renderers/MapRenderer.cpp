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
        _backgroundRenderer.onDrawFrame(viewState);
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

        // Notify renderers about the event
        _backgroundRenderer.onSurfaceDestroyed();
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
                    // Terrain background: opaque base fill (color + depth) under all layers.
                    // Keeps the terrain visible - and depth-occluding for vector elements
                    // and billboards - even without any tile layer content above.
                    bool depthSourceRendered = false;
                    Color terrainBackgroundColor = terrainOptions->getBackgroundColor();
                    if (terrainBackgroundColor.getA() > 0) {
                        if (!_terrainRenderer) {
                            _terrainRenderer = std::make_unique<TerrainRenderer>();
                        }
                        depthSourceRendered = _terrainRenderer->renderBackground(viewState, terrainOptions, _glResourceManager, terrainBackgroundColor);
                    }
                    if (!depthWriteAssigned && !depthSourceRendered) {
                        if (!_terrainRenderer) {
                            _terrainRenderer = std::make_unique<TerrainRenderer>();
                        }
                        _terrainRenderer->renderDepthPrepass(viewState, terrainOptions, _glResourceManager);
                    }
                    if (terrainOptions->isBillboardOcclusionEnabled()) {
                        // Pixel-exact terrain depth buffer for label/billboard occlusion tests
                        if (!_terrainRenderer) {
                            _terrainRenderer = std::make_unique<TerrainRenderer>();
                        }
                        _terrainRenderer->updateDepthBuffer(viewState, terrainOptions, _glResourceManager);
                    }

                    // Camera terrain-following: keep the camera at least the configured
                    // clearance above the terrain surface. The correction goes through the
                    // normal camera event path (a zoom-out; instant by default, optionally
                    // animated), never by mutating the view state directly.
                    float cameraClearance = terrainOptions->getCameraClearance();
                    float clampDuration = terrainOptions->getCameraClampDuration();
                    auto now = std::chrono::steady_clock::now();
                    bool debounced = (clampDuration > 0 && now - _lastTerrainCameraClampTime < std::chrono::milliseconds(static_cast<int>(clampDuration * 1000)));
                    if (cameraClearance > 0 && !debounced) {
                        std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager();
                        cglib::vec3<double> cameraPos = viewState.getCameraPos();
                        double terrainZ = elevationManager->getDisplayHeight(cameraPos(0), cameraPos(1), ElevationManager::LoadMode::CACHED_ONLY);
                        double minCameraZ = terrainZ + cameraClearance * elevationManager->getDisplayScale(cameraPos(1));
                        if (cameraPos(2) > 0 && cameraPos(2) < minCameraZ) {
                            _lastTerrainCameraClampTime = now;
                            CameraZoomEvent zoomEvent;
                            zoomEvent.setZoomDelta(static_cast<float>(std::log2(cameraPos(2) / minCameraZ)) * 1.05f); // negative: zoom out just past the clearance
                            calculateCameraEvent(zoomEvent, clampDuration, false);
                        }
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
