#pragma once

#include "RoutingAction.h"
#include "../core/Variant.h"

#include <string>

namespace routing {

    /**
     * A single navigation instruction within a routing result.
     */
    class RoutingInstruction {
    public:
        RoutingInstruction();
        RoutingInstruction(RoutingAction::RoutingAction action,
                           int pointIndex,
                           const std::string& streetName,
                           const std::string& instruction,
                           float turnAngle,
                           float azimuth,
                           double distance,
                           double time,
                           const Variant& geometryTag);
        virtual ~RoutingInstruction();

        RoutingAction::RoutingAction getAction() const;
        int getPointIndex() const;
        const std::string& getStreetName() const;
        const std::string& getInstruction() const;
        float getTurnAngle() const;
        float getAzimuth() const;
        double getDistance() const;
        double getTime() const;
        const Variant& getGeometryTag() const;

        std::string toString() const;

    private:
        RoutingAction::RoutingAction _action;
        int _pointIndex;
        std::string _streetName;
        std::string _instruction;
        float _turnAngle;
        float _azimuth;
        double _distance;
        double _time;
        Variant _geometryTag;
    };

} // namespace routing
