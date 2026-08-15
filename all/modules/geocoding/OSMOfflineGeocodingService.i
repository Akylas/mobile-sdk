#ifndef _OSMOFFLINEGEOCODINGSERVICE_I
#define _OSMOFFLINEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") OSMOfflineGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT) && defined(_MASSIF_OFFLINE_SUPPORT)

!proxy_imports(massif::OSMOfflineGeocodingService, geocoding.GeocodingService, geocoding.GeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/OSMOfflineGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geocoding/GeocodingService.i"
%import "geocoding/GeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::OSMOfflineGeocodingService, geocoding.OSMOfflineGeocodingService)

%std_io_exceptions(massif::OSMOfflineGeocodingService::OSMOfflineGeocodingService)
%std_io_exceptions(massif::OSMOfflineGeocodingService::calculateAddresses)

%feature("director") massif::OSMOfflineGeocodingService;

%include "geocoding/OSMOfflineGeocodingService.h"

#endif

#endif
