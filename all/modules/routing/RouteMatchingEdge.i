#ifndef _ROUTEMATCHINGEDGE_I
#define _ROUTEMATCHINGEDGE_I

%module RouteMatchingEdge

#ifdef _MASSIF_ROUTING_SUPPORT

!proxy_imports(massif::RouteMatchingEdge, core.Variant, core.StringVariantMap)

%{
#include "routing/RouteMatchingEdge.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"

!value_type(massif::RouteMatchingEdge, routing.RouteMatchingEdge)
!value_type(std::vector<massif::RouteMatchingEdge>, routing.RouteMatchingEdgeVector)

!standard_equals(massif::RouteMatchingEdge);
!custom_tostring(massif::RouteMatchingEdge);

%include "routing/RouteMatchingEdge.h"

!value_template(std::vector<massif::RouteMatchingEdge>, routing.RouteMatchingEdgeVector);

#endif

#endif
