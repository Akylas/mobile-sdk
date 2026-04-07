#pragma once

#include "../core/MapPos.h"

#include <string>

namespace routing {

    namespace RouteMatchingPointType {
        enum RouteMatchingPointType {
            ROUTE_MATCHING_POINT_UNMATCHED,
            ROUTE_MATCHING_POINT_INTERPOLATED,
            ROUTE_MATCHING_POINT_MATCHED
        };
    }

    /**
     * A single matched point in a RouteMatchingResult.
     */
    class RouteMatchingPoint {
    public:
        RouteMatchingPoint();
        RouteMatchingPoint(const MapPos& pos,
                           RouteMatchingPointType::RouteMatchingPointType type,
                           int edgeIndex);
        virtual ~RouteMatchingPoint();

        const MapPos& getPos() const;
        RouteMatchingPointType::RouteMatchingPointType getType() const;
        int getEdgeIndex() const;

        std::string toString() const;

    private:
        MapPos _pos;
        RouteMatchingPointType::RouteMatchingPointType _type;
        int _edgeIndex;
    };

} // namespace routing
