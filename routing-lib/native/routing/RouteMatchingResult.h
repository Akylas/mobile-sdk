#pragma once

#include "RouteMatchingPoint.h"
#include "RouteMatchingEdge.h"
#include "../core/MapPos.h"
#include "../core/Projection.h"

#include <memory>
#include <string>
#include <vector>

namespace routing {

    /**
     * Result of a route matching operation.
     */
    class RouteMatchingResult {
    public:
        RouteMatchingResult(const std::shared_ptr<Projection>& projection,
                            std::vector<RouteMatchingPoint> matchingPoints,
                            std::vector<RouteMatchingEdge> matchingEdges,
                            std::string rawResult);
        virtual ~RouteMatchingResult();

        const std::shared_ptr<Projection>& getProjection() const;
        std::vector<MapPos> getPoints() const;
        const std::vector<RouteMatchingEdge>& getMatchingEdges() const;
        const std::vector<RouteMatchingPoint>& getMatchingPoints() const;
        const std::string& getRawResult() const;

        std::string toString() const;

    private:
        std::shared_ptr<Projection> _projection;
        std::vector<RouteMatchingPoint> _matchingPoints;
        std::vector<RouteMatchingEdge> _matchingEdges;
        std::string _rawResult;
    };

} // namespace routing
