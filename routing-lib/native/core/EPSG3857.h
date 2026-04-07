#pragma once

#include "Projection.h"

namespace routing {

    /**
     * EPSG:3857 Spherical Mercator projection (used by most web map services).
     */
    class EPSG3857 : public Projection {
    public:
        EPSG3857();
        virtual ~EPSG3857();

        virtual MapPos fromInternal(const MapPos& mapPosInternal) const override;
        virtual MapPos toInternal(const MapPos& mapPos) const override;
        virtual MapPos fromWgs84(const MapPos& wgs84Pos) const override;
        virtual MapPos toWgs84(const MapPos& mapPos) const override;
        virtual std::string getName() const override;

    private:
        static const double EARTH_RADIUS;
        static const double METERS_TO_INTERNAL_EQUATOR;
    };

} // namespace routing
