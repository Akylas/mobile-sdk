#include "BaseMapView.h"
#include "components/CancelableThreadPool.h"
#include "components/Layers.h"
#include "core/MapPos.h"
#include "core/MapBounds.h"
#include "core/ScreenPos.h"
#include "core/ScreenBounds.h"
#include "layers/Layer.h"
#include "layers/TileLayer.h"
#include "projections/Projection.h"
#include "projections/ProjectionSurface.h"
#include "renderers/MapRenderer.h"
#include "renderers/cameraevents/CameraPanEvent.h"
#include "renderers/cameraevents/CameraRotationEvent.h"
#include "renderers/cameraevents/CameraTiltEvent.h"
#include "renderers/cameraevents/CameraZoomEvent.h"
#include "renderers/cameraevents/CameraPanEvent.h"
#include "ui/TouchHandler.h"
#include "utils/PlatformUtils.h"
#include "utils/Log.h"

#include <list>
#include <unordered_map>
#include <vector>
#include <sstream>

namespace carto {

    std::string BaseMapView::GetSDKVersion() {
        std::stringstream ss;
        ss << "Build: " << PlatformUtils::GetPlatformId() << "-" << PlatformUtils::GetSDKVersion();
        ss << ", time: " << __DATE__ << " " << __TIME__;
        ss << ", device type: " << PlatformUtils::GetDeviceType();
        ss << ", device OS: " << PlatformUtils::GetDeviceOS();
        return ss.str();
    }
    
    BaseMapView::BaseMapView() :
        _envelopeThreadPool(std::make_shared<CancelableThreadPool>()),
        _tileThreadPool(std::make_shared<CancelableThreadPool>()),
        _options(std::make_shared<Options>(_envelopeThreadPool, _tileThreadPool)),
        _layers(std::make_shared<Layers>(_envelopeThreadPool, _tileThreadPool, _options)),
        _mapRenderer(std::make_shared<MapRenderer>(_layers, _options)),
        _touchHandler(std::make_shared<TouchHandler>(_mapRenderer, _options)),
        _mutex()
    {
        _mapRenderer->init();
        _touchHandler->init();
        _layers->setComponents(_mapRenderer, _touchHandler);
        
        setFocusPos(MapPos(), 0);
        setRotation(0, 0);
        setTilt(90, 0);
        setZoom(0, 0);

        Log::Infof("BaseMapView: %s", GetSDKVersion().c_str());
    }
    
    BaseMapView::~BaseMapView() {
        // Set stop flag and detach every thread, once the thread quits
        // all objects they hold will be released
        _envelopeThreadPool->deinit();
        _tileThreadPool->deinit();
        _mapRenderer->deinit();
        _touchHandler->deinit();
    }
    
    void BaseMapView::onSurfaceCreated() {
        Log::Info("BaseMapView::onSurfaceCreated()");
        _mapRenderer->onSurfaceCreated();
    }
    
    void BaseMapView::onSurfaceChanged(int width, int height) {
        Log::Infof("BaseMapView::onSurfaceChanged(): width: %d, height: %d", width, height);
        _mapRenderer->onSurfaceChanged(width, height);
    }
    
    void BaseMapView::onDrawFrame() {
        _mapRenderer->onDrawFrame();
    }
    
    void BaseMapView::onSurfaceDestroyed() {
        Log::Info("BaseMapView::onSurfaceDestroyed()");
        _mapRenderer->onSurfaceDestroyed();
    }

    void BaseMapView::finishRendering() {
        _mapRenderer->finishRendering();
    }
    
    void BaseMapView::onInputEvent(int event, float x1, float y1, float x2, float y2) {
        _touchHandler->onTouchEvent(event, ScreenPos(x1, y1), ScreenPos(x2, y2));
    }
    
    void BaseMapView::onWheelEvent(int delta, float x, float y) {
        _touchHandler->onWheelEvent(delta, ScreenPos(x, y));
    }
    
    MapPos BaseMapView::getFocusPos() const {
        MapPos mapPosInternal = _options->getProjectionSurface()->calculateMapPos(_mapRenderer->getViewState().getFocusPos());
        return _options->getBaseProjection()->fromInternal(mapPosInternal);
    }
    
    float BaseMapView::getRotation() const {
        return _mapRenderer->getViewState().getRotation();
    }
    
    float BaseMapView::getTilt() const {
        return _mapRenderer->getViewState().getTilt();
    }
    
    float BaseMapView::getZoom() const {
        return _mapRenderer->getViewState().getZoom();
    }
    
    void BaseMapView::pan(const MapVec& deltaPos, float durationSeconds) {
        MapPos focusPos0Internal = _options->getBaseProjection()->toInternal(getFocusPos());
        MapPos focusPos1Internal = _options->getBaseProjection()->toInternal(getFocusPos() + deltaPos);

        _mapRenderer->getAnimationHandler().stopPan();
        _mapRenderer->getKineticEventHandler().stopPan();
        
        CameraPanEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setPosDelta(std::make_pair(focusPos0Internal, focusPos1Internal));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::setFocusPos(const MapPos& pos, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopPan();
        _mapRenderer->getKineticEventHandler().stopPan();
        
        CameraPanEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setPos(_options->getBaseProjection()->toInternal(pos));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::rotate(float rotationDelta, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopRotation();
        _mapRenderer->getKineticEventHandler().stopRotation();
        
        CameraRotationEvent cameraEvent;
        cameraEvent.setRotationDelta(rotationDelta);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
        
    void BaseMapView::rotate(float rotationDelta, const MapPos& targetPos, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopRotation();
        _mapRenderer->getKineticEventHandler().stopRotation();
        
        CameraRotationEvent cameraEvent;
        cameraEvent.setRotationDelta(rotationDelta);
        cameraEvent.setTargetPos(_options->getBaseProjection()->toInternal(targetPos));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::setRotation(float rotation, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopRotation();
        _mapRenderer->getKineticEventHandler().stopRotation();
        
        CameraRotationEvent cameraEvent;
        cameraEvent.setRotation(rotation);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
        
    void BaseMapView::setRotation(float rotation, const MapPos& targetPos, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopRotation();
        _mapRenderer->getKineticEventHandler().stopRotation();
        
        CameraRotationEvent cameraEvent;
        cameraEvent.setRotation(rotation);
        cameraEvent.setTargetPos(_options->getBaseProjection()->toInternal(targetPos));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::tilt(float tiltDelta, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopTilt();
        
        CameraTiltEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setTiltDelta(tiltDelta);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::setTilt(float tilt, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopTilt();
        
        CameraTiltEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setTilt(tilt);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::zoom(float zoomDelta, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopZoom();
        _mapRenderer->getKineticEventHandler().stopZoom();
        
        CameraZoomEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setZoomDelta(zoomDelta);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
        
    void BaseMapView::zoom(float zoomDelta, const MapPos& targetPos, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopZoom();
        _mapRenderer->getKineticEventHandler().stopZoom();
        
        CameraZoomEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setZoomDelta(zoomDelta);
        cameraEvent.setTargetPos(_options->getBaseProjection()->toInternal(targetPos));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
    
    void BaseMapView::setZoom(float zoom, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopZoom();
        _mapRenderer->getKineticEventHandler().stopZoom();
        
        CameraZoomEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setZoom(zoom);
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
        
    void BaseMapView::setZoom(float zoom, const MapPos& targetPos, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopZoom();
        _mapRenderer->getKineticEventHandler().stopZoom();
        
        CameraZoomEvent cameraEvent;
        cameraEvent.setKeepRotation(true);
        cameraEvent.setZoom(zoom);
        cameraEvent.setTargetPos(_options->getBaseProjection()->toInternal(targetPos));
        _mapRenderer->calculateCameraEvent(cameraEvent, durationSeconds, false);
    }
        
    void BaseMapView::moveToFitBounds(const MapBounds& mapBounds, const ScreenBounds& screenBounds, bool integerZoom, float durationSeconds) {
        moveToFitBounds(mapBounds, screenBounds, integerZoom, false, false, durationSeconds);
    }
        
    void BaseMapView::moveToFitBounds(const MapBounds& mapBounds, const ScreenBounds& screenBounds, bool integerZoom, bool resetRotation, bool resetTilt, float durationSeconds) {
        _mapRenderer->getAnimationHandler().stopZoom();
        _mapRenderer->getKineticEventHandler().stopZoom();
        
        MapBounds mapBoundsInternal(_options->getBaseProjection()->toInternal(mapBounds.getMin()), _options->getBaseProjection()->toInternal(mapBounds.getMax()));
        _mapRenderer->moveToFitBounds(mapBoundsInternal, screenBounds, integerZoom, resetTilt, resetRotation, durationSeconds);
    }
    
    std::shared_ptr<MapEventListener> BaseMapView::getMapEventListener() const {
        return _touchHandler->getMapEventListener();
    }
        
    void BaseMapView::setMapEventListener(const std::shared_ptr<MapEventListener>& mapEventListener) {
        _touchHandler->setMapEventListener(mapEventListener);
    }
        
    std::shared_ptr<RedrawRequestListener> BaseMapView::getRedrawRequestListener() const{
        return _mapRenderer->getRedrawRequestListener();
    }
        
    void BaseMapView::setRedrawRequestListener(const std::shared_ptr<RedrawRequestListener>& listener) {
        _mapRenderer->setRedrawRequestListener(listener);
    }
        
    MapPos BaseMapView::screenToMap(const ScreenPos& screenPos) {
        ViewState viewState = _mapRenderer->getViewState();
        MapPos mapPosInternal = _options->getProjectionSurface()->calculateMapPos(viewState.screenToWorld(cglib::vec2<float>(screenPos.getX(), screenPos.getY()), 0, _options));
        return _options->getBaseProjection()->fromInternal(mapPosInternal);
    }
    
    ScreenPos BaseMapView::mapToScreen(const MapPos& mapPos) {
        ViewState viewState = _mapRenderer->getViewState();
        MapPos mapPosInternal = _options->getBaseProjection()->toInternal(mapPos);
        cglib::vec2<float> screenPos = viewState.worldToScreen(_options->getProjectionSurface()->calculatePosition(mapPosInternal), _options);
        return ScreenPos(screenPos(0), screenPos(1));
    }
    
    void BaseMapView::cancelAllTasks() {
        _envelopeThreadPool->cancelAll();
        _tileThreadPool->cancelAll();
    }
    
    void BaseMapView::clearPreloadingCaches() {
        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
            if (const std::shared_ptr<TileLayer>& tileLayer = std::dynamic_pointer_cast<TileLayer>(layer)) {
                tileLayer->clearTileCaches(false);
            }
        }
    }
    
    void BaseMapView::clearAllCaches() {
        for (const std::shared_ptr<Layer>& layer : _layers->getAll()) {
            if (const std::shared_ptr<TileLayer>& tileLayer = std::dynamic_pointer_cast<TileLayer>(layer)) {
                tileLayer->clearTileCaches(true);
            }
        }
    }
    
    const std::shared_ptr<Layers>& BaseMapView::getLayers() const {
        return _layers;
    }
    
    const std::shared_ptr<Options>& BaseMapView::getOptions() const {
        return _options;
    }

    const std::shared_ptr<MapRenderer>& BaseMapView::getMapRenderer() const {
        return _mapRenderer;
    }
        
}
