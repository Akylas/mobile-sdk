#ifndef _ROUTEMATCHINGRESULT_I
#define _ROUTEMATCHINGRESULT_I

#pragma SWIG nowarn=325

%module RouteMatchingResult

#ifdef _MASSIF_ROUTING_SUPPORT

!proxy_imports(massif::RouteMatchingResult, core.MapPos, core.MapPosVector, projections.Projection, routing.RouteMatchingPoint, routing.RouteMatchingEdge)

%{
#include "routing/RouteMatchingResult.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "projections/Projection.i"
%import "routing/RouteMatchingPoint.i"
%import "routing/RouteMatchingEdge.i"

!shared_ptr(massif::RouteMatchingResult, routing.RouteMatchingResult)

%attributestring(massif::RouteMatchingResult, std::shared_ptr<massif::Projection>, Projection, getProjection)
%attributeval(massif::RouteMatchingResult, std::vector<massif::MapPos>, Points, getPoints)
%attributeval(massif::RouteMatchingResult, std::vector<massif::RouteMatchingEdge>, MatchingEdges, getMatchingEdges)
%attributeval(massif::RouteMatchingResult, std::vector<massif::RouteMatchingPoint>, MatchingPoints, getMatchingPoints)
%attributestring(massif::RouteMatchingResult, std::string, RawResult, getRawResult)
%std_exceptions(massif::RouteMatchingResult::RouteMatchingResult)
!standard_equals(massif::RouteMatchingResult);
!custom_tostring(massif::RouteMatchingResult);

%include "routing/RouteMatchingResult.h"

#endif

#endif
