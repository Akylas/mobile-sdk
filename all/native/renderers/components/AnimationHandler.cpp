#include "AnimationHandler.h"
#include "projections/ProjectionSurface.h"
#include "renderers/MapRenderer.h"
#include "core/MapPos.h"
#include "graphics/ViewState.h"

namespace carto {

    AnimationHandler::AnimationHandler(MapRenderer& mapRenderer) :
        _panStarted(false),
        _panDurationSeconds(0),
        _panTarget(),
        _panDelta(),
        _panUseDelta(false),
        _rotationStarted(false),
        _rotationDurationSeconds(0),
        _rotationTarget(),
        _rotationTargetPos(),
        _tiltStarted(false),
        _tiltDurationSeconds(0),
        _tiltTarget(),
        _zoomStarted(false),
        _zoomDurationSeconds(0),
        _zoomTarget(),
        _zoomTargetPos(),
        _mapRenderer(mapRenderer),
        _mutex()
    {
    }
    
    AnimationHandler::~AnimationHandler() {
    }
    
    void AnimationHandler::calculate(const ViewState& viewState, float deltaSeconds) {
        std::optional<CameraPanEvent> cameraPanEvent;
        std::optional<CameraRotationEvent> cameraRotationEvent;
        std::optional<CameraTiltEvent> cameraTiltEvent;
        std::optional<CameraZoomEvent> cameraZoomEvent;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            cameraPanEvent = calculatePan(viewState, deltaSeconds);
            cameraRotationEvent = calculateRotation(viewState, deltaSeconds);
            cameraTiltEvent = calculateTilt(viewState, deltaSeconds);
            cameraZoomEvent = calculateZoom(viewState, deltaSeconds);
        }
        if (cameraPanEvent) {
            _mapRenderer.calculateCameraEvent(*cameraPanEvent, 0, false);
        }
        if (cameraRotationEvent) {
            _mapRenderer.calculateCameraEvent(*cameraRotationEvent, 0, false);
        }
        if (cameraTiltEvent) {
            _mapRenderer.calculateCameraEvent(*cameraTiltEvent, 0, false);
        }
        if (cameraZoomEvent) {
            _mapRenderer.calculateCameraEvent(*cameraZoomEvent, 0, false);
        }
    }
    
    void AnimationHandler::setPanTarget(const MapPos& panTarget, float durationSeconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _panStarted = true;
        _panTarget = panTarget;
        _panDurationSeconds = durationSeconds;
        _panUseDelta = false;
    }
    
    void AnimationHandler::setPanDelta(const std::pair<MapPos, MapPos>& panDelta, float durationSeconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _panStarted = true;
        _panDelta = panDelta;
        _panDurationSeconds = durationSeconds;
        _panUseDelta = true;
    }

    void AnimationHandler::stopPan() {
        std::lock_guard<std::mutex> lock(_mutex);
        _panDurationSeconds = 0;
    }
    
    void AnimationHandler::setRotationTarget(float rotationTarget, const MapPos* targetPos, float durationSeconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _rotationStarted = true;
        _rotationTarget = fmod(rotationTarget, 360.0f);
        if (_rotationTarget > 180) {
            _rotationTarget -= 360;
        } else if (_rotationTarget < -180) {
            _rotationTarget += 360;
        }
        _rotationTargetPos.reset(targetPos ? new MapPos(*targetPos) : nullptr);
        _rotationDurationSeconds = durationSeconds;
    }
        
    void AnimationHandler::stopRotation() {
        std::lock_guard<std::mutex> lock(_mutex);
        _rotationDurationSeconds = 0;
    }
    
    void AnimationHandler::setTiltTarget(float tiltTarget, float durationSeconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _tiltStarted = true;
        _tiltTarget = tiltTarget;
        _tiltDurationSeconds = durationSeconds;
    }
        
    void AnimationHandler::stopTilt() {
        std::lock_guard<std::mutex> lock(_mutex);
        _tiltDurationSeconds = 0;
    }
    
    void AnimationHandler::setZoomTarget(float zoomTarget, const MapPos* targetPos, float durationSeconds) {
        std::lock_guard<std::mutex> lock(_mutex);
        _zoomStarted = true;
        _zoomTarget = zoomTarget;
        _zoomTargetPos.reset(targetPos ? new MapPos(*targetPos) : nullptr);
        _zoomDurationSeconds = durationSeconds;
    }
        
    void AnimationHandler::stopZoom() {
        std::lock_guard<std::mutex> lock(_mutex);
        _zoomDurationSeconds = 0;
    }
    
    std::optional<CameraPanEvent> AnimationHandler::calculatePan(const ViewState& viewState, float deltaSeconds) {
        // Disregard the first calculation event, because the deltaSeconds parameter may be
        // very large. It's caused by on-demand rendering.
        if (_panStarted) {
            _panStarted = false;
            deltaSeconds = 0;
        }
        if (_panDurationSeconds > 0) {
            float ratio = std::min(1.0f, deltaSeconds / _panDurationSeconds);
            _panDurationSeconds -= deltaSeconds;
            MapPos newFocusPos;
            if (_panDurationSeconds <= 0 && !_panUseDelta) {
                newFocusPos = _panTarget;
            } else {
                std::shared_ptr<ProjectionSurface> projectionSurface = _mapRenderer.getProjectionSurface();
                cglib::vec3<double> pos0 = _panUseDelta ? projectionSurface->calculatePosition(_panDelta.first) : viewState.getFocusPos();
                cglib::vec3<double> pos1 = _panUseDelta ? projectionSurface->calculatePosition(_panDelta.second) : projectionSurface->calculatePosition(_panTarget);
                cglib::mat4x4<double> transform = projectionSurface->calculateTranslateMatrix(pos0, pos1, ratio);
                newFocusPos = projectionSurface->calculateMapPos(cglib::transform_point(viewState.getFocusPos(), transform));
            }
    
            CameraPanEvent cameraEvent;
            cameraEvent.setPos(newFocusPos);
            return cameraEvent;
        }
        return std::optional<CameraPanEvent>();
    }
    
    std::optional<CameraRotationEvent> AnimationHandler::calculateRotation(const ViewState& viewState, float deltaSeconds) {
        // Disregard the first calculation event, because the deltaSeconds parameter may be
        // very large. It's caused by on-demand rendering.
        if (_rotationStarted) {
            _rotationStarted = false;
            deltaSeconds = 0;
        }
        if (_rotationDurationSeconds > 0) {
            float ratio = std::min(1.0f, deltaSeconds / _rotationDurationSeconds);
            _rotationDurationSeconds -= deltaSeconds;
            float newRotation;
            if (_rotationDurationSeconds <= 0) {
               newRotation = _rotationTarget;
            } else {
                float delta = _rotationTarget - viewState.getRotation();
                if (delta > 180) {
                    delta -= 360;
                } else if (delta < -180) {
                    delta += 360;
                }
                delta *= ratio;
                newRotation = viewState.getRotation() + delta;
            }
        
            CameraRotationEvent cameraEvent;
            cameraEvent.setRotation(newRotation);
            if (_rotationTargetPos) {
                cameraEvent.setTargetPos(*_rotationTargetPos);
            }
            return cameraEvent;
        }
        return std::optional<CameraRotationEvent>();
    }
    
    std::optional<CameraTiltEvent> AnimationHandler::calculateTilt(const ViewState& viewState, float deltaSeconds) {
        // Disregard the first calculation event, because the deltaSeconds parameter may be
        // very large. It's caused by on-demand rendering.
        if (_tiltStarted) {
            _tiltStarted = false;
            deltaSeconds = 0;
        }
        if (_tiltDurationSeconds > 0) {
            float ratio = std::min(1.0f, deltaSeconds / _tiltDurationSeconds);
            _tiltDurationSeconds -= deltaSeconds;
            float newTilt;
            if (_tiltDurationSeconds <= 0) {
                newTilt = _tiltTarget;
            } else {
                float delta = _tiltTarget - viewState.getTilt();
                delta *= ratio;
                newTilt = viewState.getTilt() + delta;
            }
    
            CameraTiltEvent cameraEvent;
            cameraEvent.setTilt(newTilt);
            return cameraEvent;
        }
        return std::optional<CameraTiltEvent>();
    }
    
    std::optional<CameraZoomEvent> AnimationHandler::calculateZoom(const ViewState& viewState, float deltaSeconds) {
        // Disregard the first calculation event, because the deltaSeconds parameter may be
        // very large. It's caused by on-demand rendering.
        if (_zoomStarted) {
            _zoomStarted = false;
            deltaSeconds = 0;
        }
        if (_zoomDurationSeconds > 0) {
            float ratio = std::min(1.0f, deltaSeconds / _zoomDurationSeconds);
            _zoomDurationSeconds -= deltaSeconds;
            float newZoom;
            if (_zoomDurationSeconds <= 0) {
                newZoom = _zoomTarget;
            } else {
                float delta = _zoomTarget - viewState.getZoom();
                delta *= ratio;
                newZoom = viewState.getZoom() + delta;
            }
    
            CameraZoomEvent cameraEvent;
            cameraEvent.setZoom(newZoom);
            if (_zoomTargetPos) {
                cameraEvent.setTargetPos(*_zoomTargetPos);
            }
            return cameraEvent;
        }
        return std::optional<CameraZoomEvent>();
    }
    
}
