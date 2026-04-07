#pragma once

#include <string>

namespace routing {

    /**
     * A double precision map vector defined by 3 coordinates.
     */
    class MapVec {
    public:
        MapVec();
        MapVec(double x, double y);
        MapVec(double x, double y, double z);

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

        MapVec& operator +=(const MapVec& v);
        MapVec& operator -=(const MapVec& v);
        MapVec& operator *=(double multiplier);
        MapVec& operator /=(double divider);

        MapVec operator +(const MapVec& v) const;
        MapVec operator -(const MapVec& v) const;
        MapVec operator *(double multiplier) const;
        MapVec operator /(double divider) const;

        bool operator ==(const MapVec& v) const;
        bool operator !=(const MapVec& v) const;

        double length() const;
        double lengthSqr() const;

        MapVec& normalize();
        MapVec getNormalized() const;

        double crossProduct2D(const MapVec& v) const;
        MapVec crossProduct3D(const MapVec& v) const;
        double dotProduct(const MapVec& v) const;

        int hash() const;
        std::string toString() const;

    private:
        double _x;
        double _y;
        double _z;
    };

} // namespace routing
