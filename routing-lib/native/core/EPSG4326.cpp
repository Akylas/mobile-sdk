#include "EPSG4326.h"
#include "Constants.h"

#include <cmath>

namespace routing {

    EPSG4326::EPSG4326() :
        Projection(MapBounds(MapPos(-180.0, -90.0), MapPos(180.0, 90.0)))
    {
    }

    EPSG4326::~EPSG4326() {}

    MapPos EPSG4326::fromInternal(const MapPos& p) const {
        double x = p.getX() / UNITS_TO_INTERNAL * ROUTING_RAD_TO_DEG;
        double y = 90.0 - ROUTING_RAD_TO_DEG * (2.0 * std::atan(std::exp(-p.getY() / UNITS_TO_INTERNAL)));
        double z = p.getZ() / UNITS_TO_INTERNAL * EARTH_RADIUS;
        return MapPos(x, y, z);
    }

    MapPos EPSG4326::toInternal(const MapPos& p) const {
        double x = p.getX() * UNITS_TO_INTERNAL * ROUTING_DEG_TO_RAD;
        double a = std::sin(p.getY() * ROUTING_DEG_TO_RAD);
        double y = 0.5 * UNITS_TO_INTERNAL * std::log((1.0 + a) / (1.0 - a));
        double z = p.getZ() * UNITS_TO_INTERNAL / EARTH_RADIUS;
        return MapPos(x, y, z);
    }

    MapPos EPSG4326::fromWgs84(const MapPos& wgs84Pos) const { return wgs84Pos; }
    MapPos EPSG4326::toWgs84(const MapPos& p) const { return p; }

    std::string EPSG4326::getName() const { return "EPSG:4326"; }

    const double EPSG4326::EARTH_RADIUS = 6378137.0;
    const double EPSG4326::UNITS_TO_INTERNAL = ROUTING_WORLD_SIZE / (2.0 * ROUTING_PI);

} // namespace routing
