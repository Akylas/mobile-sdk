#ifndef _PACKAGEMANAGERREVERSEGEOCODINGSERVICE_I
#define _PACKAGEMANAGERREVERSEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") PackageManagerReverseGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT) && defined(_MASSIF_PACKAGEMANAGER_SUPPORT)

!proxy_imports(massif::PackageManagerReverseGeocodingService, geocoding.ReverseGeocodingService, geocoding.ReverseGeocodingRequest, geocoding.GeocodingResult, packagemanager.PackageManager, projections.Projection)

%{
#include "geocoding/PackageManagerReverseGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geocoding/ReverseGeocodingService.i"
%import "packagemanager/PackageManager.i"

!polymorphic_shared_ptr(massif::PackageManagerReverseGeocodingService, geocoding.PackageManagerReverseGeocodingService)

%std_exceptions(massif::PackageManagerReverseGeocodingService::PackageManagerReverseGeocodingService)
%std_io_exceptions(massif::PackageManagerReverseGeocodingService::calculateAddresses)

%feature("director") massif::PackageManagerReverseGeocodingService;

%include "geocoding/PackageManagerReverseGeocodingService.h"

#endif

#endif
