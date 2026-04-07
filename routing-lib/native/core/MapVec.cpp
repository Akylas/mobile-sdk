#include "MapVec.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <functional>

namespace routing {

    MapVec::MapVec() : _x(0), _y(0), _z(0) {}
    MapVec::MapVec(double x, double y) : _x(x), _y(y), _z(0) {}
    MapVec::MapVec(double x, double y, double z) : _x(x), _y(y), _z(z) {}

    double MapVec::getX() const { return _x; }
    void MapVec::setX(double x) { _x = x; }
    double MapVec::getY() const { return _y; }
    void MapVec::setY(double y) { _y = y; }
    double MapVec::getZ() const { return _z; }
    void MapVec::setZ(double z) { _z = z; }

    double MapVec::operator [](std::size_t n) const {
        switch (n) {
        case 0: return _x;
        case 1: return _y;
        case 2: return _z;
        }
        throw std::out_of_range("MapVec::operator[]");
    }

    double& MapVec::operator [](std::size_t n) {
        switch (n) {
        case 0: return _x;
        case 1: return _y;
        case 2: return _z;
        }
        throw std::out_of_range("MapVec::operator[]");
    }

    void MapVec::setCoords(double x, double y) { _x = x; _y = y; }
    void MapVec::setCoords(double x, double y, double z) { _x = x; _y = y; _z = z; }

    bool MapVec::operator ==(const MapVec& v) const { return _x == v._x && _y == v._y && _z == v._z; }
    bool MapVec::operator !=(const MapVec& v) const { return !(*this == v); }

    MapVec& MapVec::operator +=(const MapVec& v) { _x += v._x; _y += v._y; _z += v._z; return *this; }
    MapVec& MapVec::operator -=(const MapVec& v) { _x -= v._x; _y -= v._y; _z -= v._z; return *this; }
    MapVec& MapVec::operator *=(double m) { _x *= m; _y *= m; _z *= m; return *this; }
    MapVec& MapVec::operator /=(double d) { _x /= d; _y /= d; _z /= d; return *this; }

    MapVec MapVec::operator +(const MapVec& v) const { return MapVec(_x + v._x, _y + v._y, _z + v._z); }
    MapVec MapVec::operator -(const MapVec& v) const { return MapVec(_x - v._x, _y - v._y, _z - v._z); }
    MapVec MapVec::operator *(double m) const { return MapVec(_x * m, _y * m, _z * m); }
    MapVec MapVec::operator /(double d) const { return MapVec(_x / d, _y / d, _z / d); }

    double MapVec::length() const { return std::sqrt(lengthSqr()); }
    double MapVec::lengthSqr() const { return dotProduct(*this); }

    MapVec& MapVec::normalize() { double len = length(); *this /= len; return *this; }
    MapVec MapVec::getNormalized() const { double len = length(); return MapVec(_x / len, _y / len, _z / len); }

    double MapVec::crossProduct2D(const MapVec& v) const { return _x * v._y - _y * v._x; }
    MapVec MapVec::crossProduct3D(const MapVec& v) const {
        return MapVec(_y * v._z - _z * v._y, _z * v._x - _x * v._z, _x * v._y - _y * v._x);
    }
    double MapVec::dotProduct(const MapVec& v) const { return _x * v._x + _y * v._y + _z * v._z; }

    int MapVec::hash() const {
        std::hash<double> hasher;
        return static_cast<int>((hasher(_z) << 16) ^ (hasher(_y) << 8) ^ hasher(_x));
    }

    std::string MapVec::toString() const {
        std::stringstream ss;
        ss << std::setiosflags(std::ios::fixed);
        ss << "MapVec [x=" << _x << ", y=" << _y << ", z=" << _z << "]";
        return ss.str();
    }

} // namespace routing
