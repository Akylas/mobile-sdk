/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_ROUTINGRESULTBUILDER_H_
#define _MASSIF_ROUTINGRESULTBUILDER_H_

#ifdef _MASSIF_ROUTING_SUPPORT

#include "core/MapPos.h"
#include "routing/RoutingResult.h"
#include "routing/utils/RoutingInstructionBuilder.h"

#include <memory>
#include <string>
#include <vector>
#include <list>

namespace massif {
    class Projection;

    class RoutingResultBuilder {
    public:
        explicit RoutingResultBuilder(const std::shared_ptr<Projection>& proj, const std::string& rawResult);

        int addPoints(const std::vector<MapPos>& points);

        RoutingInstructionBuilder& addInstruction(RoutingAction::RoutingAction action, int pointIndex);

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
    
}

#endif

#endif
