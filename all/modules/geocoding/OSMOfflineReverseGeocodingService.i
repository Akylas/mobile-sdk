#ifndef _OSMOFFLINEREVERSEGEOCODINGSERVICE_I
#define _OSMOFFLINEREVERSEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") OSMOfflineReverseGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT) && defined(_MASSIF_OFFLINE_SUPPORT)

!proxy_imports(massif::OSMOfflineReverseGeocodingService, geocoding.ReverseGeocodingService, geocoding.ReverseGeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/OSMOfflineReverseGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geocoding/ReverseGeocodingService.i"
%import "geocoding/ReverseGeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::OSMOfflineReverseGeocodingService, geocoding.OSMOfflineReverseGeocodingService)

%std_io_exceptions(massif::OSMOfflineReverseGeocodingService::OSMOfflineReverseGeocodingService)
%std_io_exceptions(massif::OSMOfflineReverseGeocodingService::calculateAddresses)

%feature("director") massif::OSMOfflineReverseGeocodingService;

%include "geocoding/OSMOfflineReverseGeocodingService.h"

#endif

#endif
