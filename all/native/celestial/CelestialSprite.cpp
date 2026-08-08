#include "CelestialSprite.h"
#include "graphics/Bitmap.h"

#include <algorithm>

namespace carto {

    CelestialSprite::CelestialSprite() :
        CelestialObject(),
        _angularSize(0.5f),
        _screenSize(0.0f),
        _bitmap(),
        _softness(0.25f),
        _clickRadius(1.0f)
    {
    }

    CelestialSprite::~CelestialSprite() {
    }

    float CelestialSprite::getAngularSize() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _angularSize;
    }

    void CelestialSprite::setAngularSize(float degrees) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _angularSize = std::max(0.0f, degrees);
            _screenSize = 0.0f;
        }
        notifyChanged();
    }

    float CelestialSprite::getScreenSize() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _screenSize;
    }

    void CelestialSprite::setScreenSize(float pixels) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _screenSize = std::max(0.0f, pixels);
            _angularSize = 0.0f;
        }
        notifyChanged();
    }

    std::shared_ptr<Bitmap> CelestialSprite::getBitmap() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _bitmap;
    }

    void CelestialSprite::setBitmap(const std::shared_ptr<Bitmap>& bitmap) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _bitmap = bitmap;
        }
        notifyChanged();
    }

    float CelestialSprite::getSoftness() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _softness;
    }

    void CelestialSprite::setSoftness(float softness) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _softness = std::max(0.0f, std::min(1.0f, softness));
        }
        notifyChanged();
    }

    float CelestialSprite::getClickRadius() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _clickRadius;
    }

    void CelestialSprite::setClickRadius(float degrees) {
        std::lock_guard<std::mutex> lock(_mutex);
        _clickRadius = std::max(0.0f, degrees);
    }

}
