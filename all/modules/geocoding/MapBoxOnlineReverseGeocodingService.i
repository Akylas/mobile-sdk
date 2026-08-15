#ifndef _MAPBOXONLINEREVERSEGEOCODINGSERVICE_I
#define _MAPBOXONLINEREVERSEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") MapBoxOnlineReverseGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::MapBoxOnlineReverseGeocodingService, geocoding.ReverseGeocodingService, geocoding.ReverseGeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/MapBoxOnlineReverseGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/ReverseGeocodingService.i"
%import "geocoding/ReverseGeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::MapBoxOnlineReverseGeocodingService, geocoding.MapBoxOnlineReverseGeocodingService)

%attributestring(massif::MapBoxOnlineReverseGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::MapBoxOnlineReverseGeocodingService::calculateAddresses)

%feature("director") massif::MapBoxOnlineReverseGeocodingService;

%include "geocoding/MapBoxOnlineReverseGeocodingService.h"

#endif

#endif
