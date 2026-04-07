#pragma once

#include <string>
#include <functional>

namespace routing {

    /**
     * A WGS-84 map position stored as (lon, lat [, alt]) or as (x, y [, z]).
     * For routing: x = longitude, y = latitude.
     */
    class MapPos {
    public:
        MapPos();
        MapPos(double x, double y);
        MapPos(double x, double y, double z);

        double getX() const;
        void setX(double x);
        double getY() const;
        void setY(double y);
        double getZ() const;
        void setZ(double z);

        double operator [](std::size_t n) const;
        double& operator [](std::size_t n);

        void setCoords(double x, double y);
        void setCoords(double x, double y, double z);

        bool operator ==(const MapPos& p) const;
        bool operator !=(const MapPos& p) const;

        int hash() const;
        std::string toString() const;

    private:
        double _x;
        double _y;
        double _z;
    };

} // namespace routing

namespace std {
    template <>
    struct hash<routing::MapPos> {
        size_t operator ()(const routing::MapPos& pos) const {
            return static_cast<size_t>(pos.hash());
        }
    };
}
