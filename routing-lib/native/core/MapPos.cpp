#include "MapPos.h"

#include <stdexcept>
#include <sstream>
#include <cmath>

namespace routing {

    MapPos::MapPos() : _x(0), _y(0), _z(0) {}
    MapPos::MapPos(double x, double y) : _x(x), _y(y), _z(0) {}
    MapPos::MapPos(double x, double y, double z) : _x(x), _y(y), _z(z) {}

    double MapPos::getX() const { return _x; }
    void   MapPos::setX(double x) { _x = x; }
    double MapPos::getY() const { return _y; }
    void   MapPos::setY(double y) { _y = y; }
    double MapPos::getZ() const { return _z; }
    void   MapPos::setZ(double z) { _z = z; }

    double MapPos::operator [](std::size_t n) const {
        switch (n) {
            case 0: return _x;
            case 1: return _y;
            case 2: return _z;
            default: throw std::out_of_range("MapPos::operator[]");
        }
    }

    double& MapPos::operator [](std::size_t n) {
        switch (n) {
            case 0: return _x;
            case 1: return _y;
            case 2: return _z;
            default: throw std::out_of_range("MapPos::operator[]");
        }
    }

    void MapPos::setCoords(double x, double y)           { _x = x; _y = y; }
    void MapPos::setCoords(double x, double y, double z) { _x = x; _y = y; _z = z; }

    bool MapPos::operator ==(const MapPos& p) const { return _x == p._x && _y == p._y && _z == p._z; }
    bool MapPos::operator !=(const MapPos& p) const { return !(*this == p); }

    int MapPos::hash() const {
        int h = static_cast<int>(_x * 4194304.0);
        h = h * 31 + static_cast<int>(_y * 4194304.0);
        h = h * 31 + static_cast<int>(_z * 4194304.0);
        return h;
    }

    std::string MapPos::toString() const {
        std::stringstream ss;
        ss << "MapPos [x=" << _x << ", y=" << _y << ", z=" << _z << "]";
        return ss.str();
    }

} // namespace routing
