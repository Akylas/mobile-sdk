#ifndef _PACKAGEMANAGERVALHALLAROUTINGSERVICE_I
#define _PACKAGEMANAGERVALHALLAROUTINGSERVICE_I

%module(directors="1") PackageManagerValhallaRoutingService

#if defined(_MASSIF_ROUTING_SUPPORT) && defined(_MASSIF_VALHALLA_ROUTING_SUPPORT) && defined(_MASSIF_PACKAGEMANAGER_SUPPORT)

!proxy_imports(massif::PackageManagerValhallaRoutingService, packagemanager.PackageManager, core.Variant, routing.RoutingService, routing.RoutingRequest, routing.RoutingResult, routing.RouteMatchingRequest, routing.RouteMatchingResult)

%{
#include "routing/PackageManagerValhallaRoutingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"
%import "routing/RoutingService.i"
%import "packagemanager/PackageManager.i"

!polymorphic_shared_ptr(massif::PackageManagerValhallaRoutingService, routing.PackageManagerValhallaRoutingService)

%std_exceptions(massif::PackageManagerValhallaRoutingService::PackageManagerValhallaRoutingService)
%std_io_exceptions(massif::PackageManagerValhallaRoutingService::matchRoute)
%std_io_exceptions(massif::PackageManagerValhallaRoutingService::calculateRoute)

%feature("director") massif::PackageManagerValhallaRoutingService;

%include "routing/PackageManagerValhallaRoutingService.h"

#endif

#endif
