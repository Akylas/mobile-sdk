#pragma once

#include "Projection.h"

namespace routing {

    /**
     * EPSG:4326 WGS84 lat/lon projection.
     */
    class EPSG4326 : public Projection {
    public:
        EPSG4326();
        virtual ~EPSG4326();

        virtual MapPos fromInternal(const MapPos& mapPosInternal) const override;
        virtual MapPos toInternal(const MapPos& mapPos) const override;
        virtual MapPos fromWgs84(const MapPos& wgs84Pos) const override;
        virtual MapPos toWgs84(const MapPos& mapPos) const override;
        virtual std::string getName() const override;

    private:
        static const double EARTH_RADIUS;
        static const double UNITS_TO_INTERNAL;
    };

} // namespace routing
