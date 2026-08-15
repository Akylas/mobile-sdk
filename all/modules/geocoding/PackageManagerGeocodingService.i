#ifndef _PACKAGEMANAGERGEOCODINGSERVICE_I
#define _PACKAGEMANAGERGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") PackageManagerGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT) && defined(_MASSIF_PACKAGEMANAGER_SUPPORT)

!proxy_imports(massif::PackageManagerGeocodingService, geocoding.GeocodingService, geocoding.GeocodingRequest, geocoding.GeocodingResult, packagemanager.PackageManager, projections.Projection)

%{
#include "geocoding/PackageManagerGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geocoding/GeocodingService.i"
%import "packagemanager/PackageManager.i"

!polymorphic_shared_ptr(massif::PackageManagerGeocodingService, geocoding.PackageManagerGeocodingService)

%std_exceptions(massif::PackageManagerGeocodingService::PackageManagerGeocodingService)
%std_io_exceptions(massif::PackageManagerGeocodingService::calculateAddresses)

%feature("director") massif::PackageManagerGeocodingService;

%include "geocoding/PackageManagerGeocodingService.h"

#endif

#endif
