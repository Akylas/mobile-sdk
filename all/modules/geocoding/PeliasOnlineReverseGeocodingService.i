#ifndef _PELIASONLINEREVERSEGEOCODINGSERVICE_I
#define _PELIASONLINEREVERSEGEOCODINGSERVICE_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module(directors="1") PeliasOnlineReverseGeocodingService

#if defined(_MASSIF_GEOCODING_SUPPORT)

!proxy_imports(massif::PeliasOnlineReverseGeocodingService, geocoding.ReverseGeocodingService, geocoding.ReverseGeocodingRequest, geocoding.GeocodingResult, projections.Projection)

%{
#include "geocoding/PeliasOnlineReverseGeocodingService.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geocoding/ReverseGeocodingService.i"
%import "geocoding/ReverseGeocodingRequest.i"
%import "geocoding/GeocodingResult.i"

!polymorphic_shared_ptr(massif::PeliasOnlineReverseGeocodingService, geocoding.PeliasOnlineReverseGeocodingService)

%attributestring(massif::PeliasOnlineReverseGeocodingService, std::string, CustomServiceURL, getCustomServiceURL, setCustomServiceURL)
%std_io_exceptions(massif::PeliasOnlineReverseGeocodingService::calculateAddresses)

%feature("director") massif::PeliasOnlineReverseGeocodingService;

%include "geocoding/PeliasOnlineReverseGeocodingService.h"

#endif

#endif
