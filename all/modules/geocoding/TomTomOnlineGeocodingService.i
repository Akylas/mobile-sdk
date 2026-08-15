#ifndef _TOMTOMONLINEGEOCODINGSERVICE_I
#define _TOMTOMONLINEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") TomTomOnlineGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::TomTomOnlineGeocodingService, geocoding.GeocodingService, geocoding.GeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/TomTomOnlineGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/GeocodingService.i"
%import "geocoding/GeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::TomTomOnlineGeocodingService, geocoding.TomTomOnlineGeocodingService)

%attributestring(massif::TomTomOnlineGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::TomTomOnlineGeocodingService::calculateAddresses)

%feature("director") massif::TomTomOnlineGeocodingService;

%include "geocoding/TomTomOnlineGeocodingService.h"

#endif

#endif
