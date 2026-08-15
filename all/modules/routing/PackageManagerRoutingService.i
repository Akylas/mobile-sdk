#ifndef _PACKAGEMANAGERROUTINGSERVICE_I
#define _PACKAGEMANAGERROUTINGSERVICE_I

%module(directors="1") PackageManagerRoutingService

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_PACKAGEMANAGER_SUPPORT)

!proxy_imports(massif::PackageManagerRoutingService, packagemanager.PackageManager, routing.RoutingService, routing.RoutingRequest, routing.RoutingResult, routing.RouteMatchingRequest, routing.RouteMatchingResult)

%{
#include "routing/PackageManagerRoutingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "routing/RoutingService.i"
%import "packagemanager/PackageManager.i"

!polymorphic_shared_ptr(massif::PackageManagerRoutingService, routing.PackageManagerRoutingService)

%std_exceptions(massif::PackageManagerRoutingService::PackageManagerRoutingService)
%std_io_exceptions(massif::PackageManagerRoutingService::matchRoute)
%std_io_exceptions(massif::PackageManagerRoutingService::calculateRoute)

%feature("director") massif::PackageManagerRoutingService;

%include "routing/PackageManagerRoutingService.h"

#endif

#endif
