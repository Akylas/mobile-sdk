#pragma once

#include "RoutingInstruction.h"
#include "../core/MapPos.h"
#include "../core/Projection.h"

#include <memory>
#include <string>
#include <vector>

namespace routing {

    /**
     * Result of a route calculation. Contains the route geometry (points) and
     * a list of navigation instructions.
     */
    class RoutingResult {
    public:
        RoutingResult(const std::shared_ptr<Projection>& projection,
                      std::vector<MapPos> points,
                      std::vector<RoutingInstruction> instructions,
                      std::string rawResult);
        virtual ~RoutingResult();

        const std::shared_ptr<Projection>& getProjection() const;
        const std::vector<MapPos>& getPoints() const;
        const std::vector<RoutingInstruction>& getInstructions() const;

        double getTotalDistance() const;
        double getTotalTime() const;
        const std::string& getRawResult() const;

        std::string toString() const;

    private:
        std::shared_ptr<Projection> _projection;
        std::vector<MapPos> _points;
        std::vector<RoutingInstruction> _instructions;
        std::string _rawResult;
    };

} // namespace routing
