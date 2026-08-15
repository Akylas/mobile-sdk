#ifndef _SGREMOFFLINEROUTINGSERVICE_I
#define _SGREMOFFLINEROUTINGSERVICE_I

%module(directors="1") SGREOfflineRoutingService

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_OFFLINE_SUPPORT)

!proxy_imports(massif::SGREOfflineRoutingService, core.Variant, geometry.FeatureCollection, projections.Projection, routing.RoutingService, routing.RoutingRequest, routing.RoutingResult, routing.RouteMatchingRequest, routing.RouteMatchingResult)

%{
#include "routing/SGREOfflineRoutingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "routing/RoutingService.i"
%import "core/Variant.i"
%import "geometry/FeatureCollection.i"
%import "projections/Projection.i"

!polymorphic_shared_ptr(massif::SGREOfflineRoutingService, routing.SGREOfflineRoutingService)

%std_io_exceptions(massif::SGREOfflineRoutingService::SGREOfflineRoutingService)
%std_io_exceptions(massif::SGREOfflineRoutingService::matchRoute)
%std_io_exceptions(massif::SGREOfflineRoutingService::calculateRoute)

%feature("director") massif::SGREOfflineRoutingService;

%include "routing/SGREOfflineRoutingService.h"

#endif

#endif
