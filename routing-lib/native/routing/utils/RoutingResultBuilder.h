#pragma once

#include "../RoutingResult.h"
#include "RoutingInstructionBuilder.h"
#include "../../core/Projection.h"

#include <memory>
#include <string>
#include <vector>
#include <list>

namespace routing {

    class RoutingResultBuilder {
    public:
        explicit RoutingResultBuilder(const std::shared_ptr<Projection>& proj,
                                      const std::string& rawResult);

        int addPoints(const std::vector<MapPos>& points);

        RoutingInstructionBuilder& addInstruction(RoutingAction::RoutingAction action,
                                                  int pointIndex);

        std::shared_ptr<RoutingResult> buildRoutingResult() const;

    private:
        float calculateTurnAngle(int pointIndex) const;
        float calculateAzimuth(int pointIndex) const;
        std::string calculateDirection(float azimuth) const;
        std::string calculateDistance(double distance) const;
        std::string calculateInstruction(const RoutingInstructionBuilder& instr) const;

        const std::shared_ptr<Projection> _projection;
        std::vector<MapPos> _points;
        std::list<RoutingInstructionBuilder> _instructions;
        std::string _rawResult;
    };

} // namespace routing
