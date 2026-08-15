#ifndef _PELIASONLINEGEOCODINGSERVICE_I
#define _PELIASONLINEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") PeliasOnlineGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::PeliasOnlineGeocodingService, geocoding.GeocodingService, geocoding.GeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/PeliasOnlineGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/GeocodingService.i"
%import "geocoding/GeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::PeliasOnlineGeocodingService, geocoding.PeliasOnlineGeocodingService)

%attributestring(massif::PeliasOnlineGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::PeliasOnlineGeocodingService::calculateAddresses)

%feature("director") massif::PeliasOnlineGeocodingService;

%include "geocoding/PeliasOnlineGeocodingService.h"

#endif

#endif
