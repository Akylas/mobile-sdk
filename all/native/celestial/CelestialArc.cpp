#include "CelestialArc.h"
#include "utils/Const.h"

#include <algorithm>
#include <cmath>

namespace carto {

    const int CelestialArc::CIRCLE_SEGMENTS = 180;

    CelestialArc::CelestialArc() :
        CelestialObject(),
        _circular(true),
        _axisAzimuth(0.0f),
        _axisAltitude(90.0f),
        _radius(45.0f),
        _directions(),
        _width(2.0f),
        _belowHorizonVisible(false)
    {
    }

    CelestialArc::~CelestialArc() {
    }

    void CelestialArc::setCircle(float axisAzimuth, float axisAltitude, float radius) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _circular = true;
            _axisAzimuth = axisAzimuth;
            _axisAltitude = axisAltitude;
            _radius = std::max(0.0f, std::min(180.0f, radius));
            _directions.clear();
        }
        notifyChanged();
    }

    void CelestialArc::setDirections(const std::vector<double>& directions) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _circular = false;
            _directions = directions;
        }
        notifyChanged();
    }

    std::vector<double> CelestialArc::getDirections() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _directions;
    }

    float CelestialArc::getRadius() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _radius;
    }

    float CelestialArc::getWidth() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _width;
    }

    void CelestialArc::setWidth(float pixels) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _width = std::max(0.0f, pixels);
        }
        notifyChanged();
    }

    bool CelestialArc::isBelowHorizonVisible() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _belowHorizonVisible;
    }

    void CelestialArc::setBelowHorizonVisible(bool visible) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _belowHorizonVisible = visible;
        }
        notifyChanged();
    }

    std::vector<cglib::vec3<double> > CelestialArc::buildDirections() const {
        std::lock_guard<std::mutex> lock(_mutex);

        std::vector<cglib::vec3<double> > result;
        auto toVector = [](double azimuthDeg, double altitudeDeg) {
            double az = azimuthDeg * Const::DEG_TO_RAD;
            double alt = altitudeDeg * Const::DEG_TO_RAD;
            double cosAlt = std::cos(alt);
            return cglib::vec3<double>(cosAlt * std::sin(az), cosAlt * std::cos(az), std::sin(alt));
        };

        if (!_circular) {
            result.reserve(_directions.size() / 2);
            for (std::size_t i = 0; i + 1 < _directions.size(); i += 2) {
                result.push_back(toVector(_directions[i], _directions[i + 1]));
            }
            return result;
        }

        // A circle about an axis: take any two unit vectors perpendicular to the axis and sweep
        // them. The whole curve is then axis*cos(radius) + (u*cos t + v*sin t)*sin(radius), which
        // is a closed loop of constant angle to the axis - the path of a body at fixed declination.
        cglib::vec3<double> axis = toVector(_axisAzimuth, _axisAltitude);
        cglib::vec3<double> reference(0, 0, 1);
        if (std::abs(cglib::dot_product(axis, reference)) > 0.99) {
            reference = cglib::vec3<double>(0, 1, 0);
        }
        cglib::vec3<double> u = cglib::unit(cglib::vector_product(axis, reference));
        cglib::vec3<double> v = cglib::vector_product(axis, u);

        double radius = _radius * Const::DEG_TO_RAD;
        double cosR = std::cos(radius);
        double sinR = std::sin(radius);
        result.reserve(CIRCLE_SEGMENTS + 1);
        for (int i = 0; i <= CIRCLE_SEGMENTS; i++) {
            double t = 2.0 * Const::PI * i / CIRCLE_SEGMENTS;
            result.push_back(axis * cosR + (u * std::cos(t) + v * std::sin(t)) * sinR);
        }
        return result;
    }

}
