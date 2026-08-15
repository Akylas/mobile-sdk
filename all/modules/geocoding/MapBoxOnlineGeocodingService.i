#ifndef _MAPBOXONLINEGEOCODINGSERVICE_I
#define _MAPBOXONLINEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") MapBoxOnlineGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::MapBoxOnlineGeocodingService, geocoding.GeocodingService, geocoding.GeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/MapBoxOnlineGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/GeocodingService.i"
%import "geocoding/GeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::MapBoxOnlineGeocodingService, geocoding.MapBoxOnlineGeocodingService)

%attributestring(massif::MapBoxOnlineGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::MapBoxOnlineGeocodingService::calculateAddresses)

%feature("director") massif::MapBoxOnlineGeocodingService;

%include "geocoding/MapBoxOnlineGeocodingService.h"

#endif

#endif
