#ifndef _MULTIVALHALLAOFFLINEROUTINGSERVICE_I
#define _MULTIVALHALLAOFFLINEROUTINGSERVICE_I

%module(directors="1") MultiValhallaOfflineRoutingService

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_VALHALLA_ROUTING_SUPPORT)

!proxy_imports(massif::MultiValhallaOfflineRoutingService, core.Variant, routing.RoutingService, routing.RoutingRequest, routing.RoutingResult, routing.RouteMatchingRequest, routing.RouteMatchingResult)

%{
#include "routing/MultiValhallaOfflineRoutingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"
%import "routing/RoutingService.i"

!polymorphic_shared_ptr(massif::MultiValhallaOfflineRoutingService, routing.MultiValhallaOfflineRoutingService)

%std_io_exceptions(massif::MultiValhallaOfflineRoutingService::matchRoute)
%std_io_exceptions(massif::MultiValhallaOfflineRoutingService::calculateRoute)

%feature("director") massif::MultiValhallaOfflineRoutingService;

%include "routing/MultiValhallaOfflineRoutingService.h"

#endif

#endif
