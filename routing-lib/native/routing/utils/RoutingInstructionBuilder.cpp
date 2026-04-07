#include "RoutingInstructionBuilder.h"

namespace routing {

    RoutingInstructionBuilder::RoutingInstructionBuilder() :
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

    RoutingAction::RoutingAction RoutingInstructionBuilder::getAction() const { return _action; }
    void RoutingInstructionBuilder::setAction(RoutingAction::RoutingAction a) { _action = a; }

    int RoutingInstructionBuilder::getPointIndex() const { return _pointIndex; }
    void RoutingInstructionBuilder::setPointIndex(int idx) { _pointIndex = idx; }

    const std::string& RoutingInstructionBuilder::getStreetName() const { return _streetName; }
    void RoutingInstructionBuilder::setStreetName(const std::string& s) { _streetName = s; }

    const std::string& RoutingInstructionBuilder::getInstruction() const { return _instruction; }
    void RoutingInstructionBuilder::setInstruction(const std::string& s) { _instruction = s; }

    float RoutingInstructionBuilder::getTurnAngle() const { return _turnAngle; }
    void RoutingInstructionBuilder::setTurnAngle(float a) { _turnAngle = a; }

    float RoutingInstructionBuilder::getAzimuth() const { return _azimuth; }
    void RoutingInstructionBuilder::setAzimuth(float a) { _azimuth = a; }

    double RoutingInstructionBuilder::getDistance() const { return _distance; }
    void RoutingInstructionBuilder::setDistance(double d) { _distance = d; }

    double RoutingInstructionBuilder::getTime() const { return _time; }
    void RoutingInstructionBuilder::setTime(double t) { _time = t; }

    const Variant& RoutingInstructionBuilder::getGeometryTag() const { return _geometryTag; }
    void RoutingInstructionBuilder::setGeometryTag(const Variant& v) { _geometryTag = v; }

    RoutingInstruction RoutingInstructionBuilder::buildRoutingInstruction() const {
        return RoutingInstruction(_action, _pointIndex, _streetName, _instruction,
                                  _turnAngle, _azimuth, _distance, _time, _geometryTag);
    }

} // namespace routing
