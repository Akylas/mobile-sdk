#ifndef _ROUTEMATCHINGPOINT_I
#define _ROUTEMATCHINGPOINT_I

%module RouteMatchingPoint

#ifdef _MASSIF_ROUTING_SUPPORT

!proxy_imports(massif::RouteMatchingPoint, core.MapPos)

%{
#include "routing/RouteMatchingPoint.h"
%}

%include <std_vector.i>
%include <massifswig.i>

%import "core/MapPos.i"

!enum(massif::RouteMatchingPointType::RouteMatchingPointType)
!value_type(massif::RouteMatchingPoint, routing.RouteMatchingPoint)
!value_type(std::vector<massif::RouteMatchingPoint>, routing.RouteMatchingPointVector)

%attributeval(massif::RouteMatchingPoint, massif::MapPos, Pos, getPos)
%attribute(massif::RouteMatchingPoint, RouteMatchingPointType::RouteMatchingPointType, Type, getType)
%attribute(massif::RouteMatchingPoint, int, EdgeIndex, getEdgeIndex)
!standard_equals(massif::RouteMatchingPoint);
!custom_tostring(massif::RouteMatchingPoint);

%include "routing/RouteMatchingPoint.h"

!value_template(std::vector<massif::RouteMatchingPoint>, routing.RouteMatchingPointVector);

#endif

#endif
