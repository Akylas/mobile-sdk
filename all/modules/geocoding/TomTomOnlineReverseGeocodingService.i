#ifndef _TOMTOMONLINEREVERSEGEOCODINGSERVICE_I
#define _TOMTOMONLINEREVERSEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") TomTomOnlineReverseGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::TomTomOnlineReverseGeocodingService, geocoding.ReverseGeocodingService, geocoding.ReverseGeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/TomTomOnlineReverseGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/ReverseGeocodingService.i"
%import "geocoding/ReverseGeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::TomTomOnlineReverseGeocodingService, geocoding.TomTomOnlineReverseGeocodingService)

%attributestring(massif::TomTomOnlineReverseGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::TomTomOnlineReverseGeocodingService::calculateAddresses)

%feature("director") massif::TomTomOnlineReverseGeocodingService;

%include "geocoding/TomTomOnlineReverseGeocodingService.h"

#endif

#endif
