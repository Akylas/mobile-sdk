#pragma once

#include "../RoutingAction.h"
#include "../RoutingInstruction.h"
#include "../../core/Variant.h"

#include <string>

namespace routing {

    class RoutingInstructionBuilder {
    public:
        RoutingInstructionBuilder();

        RoutingAction::RoutingAction getAction() const;
        void setAction(RoutingAction::RoutingAction action);

        int getPointIndex() const;
        void setPointIndex(int pointIndex);

        const std::string& getStreetName() const;
        void setStreetName(const std::string& streetName);

        const std::string& getInstruction() const;
        void setInstruction(const std::string& instruction);

        float getTurnAngle() const;
        void setTurnAngle(float angle);

        float getAzimuth() const;
        void setAzimuth(float azimuth);

        double getDistance() const;
        void setDistance(double distance);

        double getTime() const;
        void setTime(double time);

        const Variant& getGeometryTag() const;
        void setGeometryTag(const Variant& variant);

        RoutingInstruction buildRoutingInstruction() const;

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
