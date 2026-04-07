#pragma once

#include <string>
#include <functional>

namespace routing {
    class MapVec;

    /**
     * A double precision map position defined using three coordinates.
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

        MapPos& operator +=(const MapVec& v);
        MapPos& operator -=(const MapVec& v);

        MapPos operator +(const MapVec& v) const;
        MapPos operator -(const MapVec& v) const;
        MapVec operator -(const MapPos& p) const;

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
