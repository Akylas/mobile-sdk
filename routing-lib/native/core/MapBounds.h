#pragma once

#include "MapPos.h"
#include "MapVec.h"

#include <string>

namespace routing {

    /**
     * An axis aligned cuboid on the map defined by minimum and maximum map positions.
     */
    class MapBounds {
    public:
        MapBounds();
        MapBounds(const MapPos& min, const MapPos& max);

        bool operator ==(const MapBounds& mapBounds) const;
        bool operator !=(const MapBounds& mapBounds) const;

        void setBounds(const MapPos& min, const MapPos& max);

        MapPos getCenter() const;
        MapVec getDelta() const;

        const MapPos& getMin() const;
        void setMin(const MapPos& min);
        const MapPos& getMax() const;
        void setMax(const MapPos& max);

        bool contains(const MapPos& pos) const;
        bool contains(const MapBounds& bounds) const;
        bool intersects(const MapBounds& bounds) const;

        void expandToContain(const MapPos& pos);
        void expandToContain(const MapBounds& bounds);
        void shrinkToIntersection(const MapBounds& bounds);

        int hash() const;
        std::string toString() const;

    private:
        MapPos _min;
        MapPos _max;
    };

} // namespace routing
