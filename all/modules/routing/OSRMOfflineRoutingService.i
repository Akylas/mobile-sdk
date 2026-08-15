#ifndef _OSRMOFFLINEROUTINGSERVICE_I
#define _OSRMOFFLINEROUTINGSERVICE_I

%module(directors="1") OSRMOfflineRoutingService

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_OFFLINE_SUPPORT)

!proxy_imports(massif::OSRMOfflineRoutingService, routing.RoutingService, routing.RoutingRequest, routing.RoutingResult, routing.RouteMatchingRequest, routing.RouteMatchingResult)

%{
#include "routing/OSRMOfflineRoutingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "routing/RoutingService.i"

!polymorphic_shared_ptr(massif::OSRMOfflineRoutingService, routing.OSRMOfflineRoutingService)

%std_io_exceptions(massif::OSRMOfflineRoutingService::OSRMOfflineRoutingService)
%std_io_exceptions(massif::OSRMOfflineRoutingService::matchRoute)
%std_io_exceptions(massif::OSRMOfflineRoutingService::calculateRoute)

%feature("director") massif::OSRMOfflineRoutingService;

%include "routing/OSRMOfflineRoutingService.h"

#endif

#endif
