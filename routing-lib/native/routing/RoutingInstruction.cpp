#include "RoutingInstruction.h"

#include <sstream>

namespace routing {

    RoutingInstruction::RoutingInstruction() :
        _action(RoutingAction::ROUTING_ACTION_NO_TURN),
        _pointIndex(-1),
        _streetName(),
        _instruction(),
        _turnAngle(0),
        _azimuth(0),
        _distance(0),
        _time(0),
        _geometryTag()
    {
    }

    RoutingInstruction::RoutingInstruction(RoutingAction::RoutingAction action,
                                           int pointIndex,
                                           const std::string& streetName,
                                           const std::string& instruction,
                                           float turnAngle,
                                           float azimuth,
                                           double distance,
                                           double time,
                                           const Variant& geometryTag) :
        _action(action),
        _pointIndex(pointIndex),
        _streetName(streetName),
        _instruction(instruction),
        _turnAngle(turnAngle),
        _azimuth(azimuth),
        _distance(distance),
        _time(time),
        _geometryTag(geometryTag)
    {
    }

    RoutingInstruction::~RoutingInstruction() {}

    RoutingAction::RoutingAction RoutingInstruction::getAction() const { return _action; }
    int RoutingInstruction::getPointIndex() const { return _pointIndex; }
    const std::string& RoutingInstruction::getStreetName() const { return _streetName; }
    const std::string& RoutingInstruction::getInstruction() const { return _instruction; }
    float RoutingInstruction::getTurnAngle() const { return _turnAngle; }
    float RoutingInstruction::getAzimuth() const { return _azimuth; }
    double RoutingInstruction::getDistance() const { return _distance; }
    double RoutingInstruction::getTime() const { return _time; }
    const Variant& RoutingInstruction::getGeometryTag() const { return _geometryTag; }

    std::string RoutingInstruction::toString() const {
        static const char* actionNames[] = {
            "Head on", "Finish", "No turn", "Go straight", "Turn right", "U turn",
            "Turn left", "Reach via location", "Enter roundabout", "Leave roundabout",
            "Stay on roundabout", "Start at end of street",
            "Enter against allowed direction", "Leave against allowed direction",
            "Go up", "Go down", "Wait", "Enter ferry", "Leave ferry"
        };
        std::stringstream ss;
        ss << "RoutingInstruction [action=" << actionNames[static_cast<int>(_action)];
        if (!_streetName.empty()) ss << ", streetName=" << _streetName;
        if (!_instruction.empty()) ss << ", instruction=" << _instruction;
        ss << ", azimuth=" << _azimuth;
        if (_turnAngle != 0) ss << ", turnAngle=" << _turnAngle;
        if (_distance != 0) ss << ", distance=" << _distance;
        if (_time != 0) ss << ", time=" << _time;
        ss << "]";
        return ss.str();
    }

} // namespace routing
