#pragma once

#include "MapPos.h"
#include "MapBounds.h"

namespace routing {

    /**
     * Abstract base class for all projections.
     */
    class Projection {
    public:
        virtual ~Projection();

        virtual MapBounds getBounds() const;

        virtual MapPos fromInternal(const MapPos& pos) const = 0;
        virtual MapPos toInternal(const MapPos& pos) const = 0;

        virtual MapPos fromWgs84(const MapPos& pos) const = 0;
        virtual MapPos toWgs84(const MapPos& pos) const = 0;

        // Convenience: lat/lng -> projected position
        MapPos fromLatLong(double lat, double lng) const;
        // Convenience: projected position -> lat/lng
        MapPos toLatLong(double x, double y) const;

        virtual std::string getName() const = 0;

    protected:
        explicit Projection(const MapBounds& bounds);

        MapBounds _bounds;
    };

} // namespace routing
