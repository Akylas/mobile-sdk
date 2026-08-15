#ifndef _ROUTINGINSTRUCTION_I
#define _ROUTINGINSTRUCTION_I

%module RoutingInstruction

#ifdef _MASSIF_ROUTING_SUPPORT

!proxy_imports(massif::RoutingInstruction, core.MapPos, core.Variant)

%{
#include "routing/RoutingInstruction.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/MapPos.i" 
%import "core/Variant.i"

!enum(massif::RoutingAction::RoutingAction)
!value_type(massif::RoutingInstruction, routing.RoutingInstruction)
!value_type(std::vector<massif::RoutingInstruction>, routing.RoutingInstructionVector)

%attribute(massif::RoutingInstruction, RoutingAction::RoutingAction, Action, getAction)
%attribute(massif::RoutingInstruction, int, PointIndex, getPointIndex)
%attribute(massif::RoutingInstruction, std::string, StreetName, getStreetName)
%attribute(massif::RoutingInstruction, std::string, Instruction, getInstruction)
%attribute(massif::RoutingInstruction, float, TurnAngle, getTurnAngle)
%attribute(massif::RoutingInstruction, float, Azimuth, getAzimuth)
%attribute(massif::RoutingInstruction, double, Distance, getDistance)
%attribute(massif::RoutingInstruction, double, Time, getTime)
%attributeval(massif::RoutingInstruction, massif::Variant, GeometryTag, getGeometryTag)
!standard_equals(massif::RoutingInstruction);
!custom_tostring(massif::RoutingInstruction);

%include "routing/RoutingInstruction.h"

!value_template(std::vector<massif::RoutingInstruction>, routing.RoutingInstructionVector);

#endif

#endif
