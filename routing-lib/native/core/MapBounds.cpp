#include "MapBounds.h"

#include <iomanip>
#include <limits>
#include <sstream>

namespace routing {

    MapBounds::MapBounds() :
        _min(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()),
        _max(-std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity())
    {
    }

    MapBounds::MapBounds(const MapPos& min, const MapPos& max) : _min(min), _max(max) {}

    bool MapBounds::operator ==(const MapBounds& b) const { return _min == b._min && _max == b._max; }
    bool MapBounds::operator !=(const MapBounds& b) const { return !(*this == b); }

    void MapBounds::setBounds(const MapPos& min, const MapPos& max) {
        _min.setX(min.getX() <= max.getX() ? min.getX() : max.getX());
        _max.setX(min.getX() <= max.getX() ? max.getX() : min.getX());
        _min.setY(min.getY() <= max.getY() ? min.getY() : max.getY());
        _max.setY(min.getY() <= max.getY() ? max.getY() : min.getY());
        _min.setZ(min.getZ() <= max.getZ() ? min.getZ() : max.getZ());
        _max.setZ(min.getZ() <= max.getZ() ? max.getZ() : min.getZ());
    }

    MapPos MapBounds::getCenter() const {
        MapVec delta(getDelta());
        delta /= 2;
        return _min + delta;
    }

    MapVec MapBounds::getDelta() const { return _max - _min; }

    const MapPos& MapBounds::getMin() const { return _min; }
    void MapBounds::setMin(const MapPos& min) { _min = min; }
    const MapPos& MapBounds::getMax() const { return _max; }
    void MapBounds::setMax(const MapPos& max) { _max = max; }

    bool MapBounds::contains(const MapPos& pos) const {
        return pos.getX() >= _min.getX() && pos.getX() <= _max.getX() &&
               pos.getY() >= _min.getY() && pos.getY() <= _max.getY() &&
               pos.getZ() >= _min.getZ() && pos.getZ() <= _max.getZ();
    }

    bool MapBounds::contains(const MapBounds& bounds) const {
        return bounds.getMin().getX() >= _min.getX() && bounds.getMax().getX() <= _max.getX() &&
               bounds.getMin().getY() >= _min.getY() && bounds.getMax().getY() <= _max.getY() &&
               bounds.getMin().getZ() >= _min.getZ() && bounds.getMax().getZ() <= _max.getZ();
    }

    bool MapBounds::intersects(const MapBounds& bounds) const {
        return !(bounds.getMax().getX() < _min.getX() || bounds.getMin().getX() > _max.getX() ||
                 bounds.getMax().getY() < _min.getY() || bounds.getMin().getY() > _max.getY() ||
                 bounds.getMax().getZ() < _min.getZ() || bounds.getMin().getZ() > _max.getZ());
    }

    void MapBounds::expandToContain(const MapPos& pos) {
        if (pos.getX() < _min.getX()) _min.setX(pos.getX());
        if (pos.getX() > _max.getX()) _max.setX(pos.getX());
        if (pos.getY() < _min.getY()) _min.setY(pos.getY());
        if (pos.getY() > _max.getY()) _max.setY(pos.getY());
        if (pos.getZ() < _min.getZ()) _min.setZ(pos.getZ());
        if (pos.getZ() > _max.getZ()) _max.setZ(pos.getZ());
    }

    void MapBounds::expandToContain(const MapBounds& bounds) {
        expandToContain(bounds.getMin());
        expandToContain(bounds.getMax());
    }

    void MapBounds::shrinkToIntersection(const MapBounds& bounds) {
        if (bounds.getMin().getX() > _min.getX()) _min.setX(bounds.getMin().getX());
        if (bounds.getMax().getX() < _max.getX()) _max.setX(bounds.getMax().getX());
        if (bounds.getMin().getY() > _min.getY()) _min.setY(bounds.getMin().getY());
        if (bounds.getMax().getY() < _max.getY()) _max.setY(bounds.getMax().getY());
        if (bounds.getMin().getZ() > _min.getZ()) _min.setZ(bounds.getMin().getZ());
        if (bounds.getMax().getZ() < _max.getZ()) _max.setZ(bounds.getMax().getZ());
    }

    int MapBounds::hash() const {
        return static_cast<int>(_min.hash() << 16 ^ _max.hash());
    }

    std::string MapBounds::toString() const {
        std::stringstream ss;
        ss << std::setiosflags(std::ios::fixed);
        ss << "MapBounds [minX=" << _min.getX() << ", minY=" << _min.getY() << ", minZ=" << _min.getZ()
           << ", maxX=" << _max.getX() << ", maxY=" << _max.getY() << ", maxZ=" << _max.getZ() << "]";
        return ss.str();
    }

} // namespace routing
