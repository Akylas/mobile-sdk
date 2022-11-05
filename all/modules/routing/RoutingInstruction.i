#ifndef _ROUTINGINSTRUCTION_I
#define _ROUTINGINSTRUCTION_I

%module RoutingInstruction

#ifdef _CARTO_ROUTING_SUPPORT

!proxy_imports(carto::RoutingInstruction, core.MapPos, core.Variant)

%{
#include "routing/RoutingInstruction.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "core/MapPos.i" 
%import "core/Variant.i"

!enum(carto::RoutingAction::RoutingAction)
!value_type(carto::RoutingInstruction, routing.RoutingInstruction)
!value_type(std::vector<carto::RoutingInstruction>, routing.RoutingInstructionVector)

%attribute(carto::RoutingInstruction, RoutingAction::RoutingAction, Action, getAction)
%attribute(carto::RoutingInstruction, int, PointIndex, getPointIndex)
%attribute(carto::RoutingInstruction, std::string, StreetName, getStreetName)
%attribute(carto::RoutingInstruction, std::string, Instruction, getInstruction)
%attribute(carto::RoutingInstruction, float, TurnAngle, getTurnAngle)
%attribute(carto::RoutingInstruction, float, Azimuth, getAzimuth)
%attribute(carto::RoutingInstruction, double, Distance, getDistance)
%attribute(carto::RoutingInstruction, double, Time, getTime)
%attributeval(carto::RoutingInstruction, carto::Variant, GeometryTag, getGeometryTag)
!standard_equals(carto::RoutingInstruction);
!custom_tostring(carto::RoutingInstruction);

%include "routing/RoutingInstruction.h"

!value_template(std::vector<carto::RoutingInstruction>, routing.RoutingInstructionVector);

#endif

#endif
