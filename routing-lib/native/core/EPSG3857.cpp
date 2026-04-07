#include "EPSG3857.h"
#include "Constants.h"

#include <cmath>

namespace routing {

    EPSG3857::EPSG3857() :
        Projection(MapBounds(
            MapPos(-ROUTING_PI * EARTH_RADIUS, -ROUTING_PI * EARTH_RADIUS),
            MapPos( ROUTING_PI * EARTH_RADIUS,  ROUTING_PI * EARTH_RADIUS)))
    {
    }

    EPSG3857::~EPSG3857() {}

    MapPos EPSG3857::fromInternal(const MapPos& p) const {
        return MapPos(p.getX() / METERS_TO_INTERNAL_EQUATOR,
                      p.getY() / METERS_TO_INTERNAL_EQUATOR,
                      p.getZ() / METERS_TO_INTERNAL_EQUATOR);
    }

    MapPos EPSG3857::toInternal(const MapPos& p) const {
        return MapPos(p.getX() * METERS_TO_INTERNAL_EQUATOR,
                      p.getY() * METERS_TO_INTERNAL_EQUATOR,
                      p.getZ() * METERS_TO_INTERNAL_EQUATOR);
    }

    MapPos EPSG3857::fromWgs84(const MapPos& wgs84Pos) const {
        double x = wgs84Pos.getX() * ROUTING_DEG_TO_RAD * EARTH_RADIUS;
        double a = std::sin(wgs84Pos.getY() * ROUTING_DEG_TO_RAD);
        double y = 0.5 * EARTH_RADIUS * std::log((1.0 + a) / (1.0 - a));
        return MapPos(x, y, wgs84Pos.getZ());
    }

    MapPos EPSG3857::toWgs84(const MapPos& p) const {
        double x = p.getX() / EARTH_RADIUS * ROUTING_RAD_TO_DEG;
        double y = 90.0 - ROUTING_RAD_TO_DEG * (2.0 * std::atan(std::exp(-p.getY() / EARTH_RADIUS)));
        return MapPos(x, y, p.getZ());
    }

    std::string EPSG3857::getName() const { return "EPSG:3857"; }

    const double EPSG3857::EARTH_RADIUS = 6378137.0;
    const double EPSG3857::METERS_TO_INTERNAL_EQUATOR =
        ROUTING_WORLD_SIZE / (2.0 * ROUTING_PI * EARTH_RADIUS);

} // namespace routing
