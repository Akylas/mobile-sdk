#ifndef _GEOCODINGRESULT_I
#define _GEOCODINGRESULT_I

#pragma SWIG nowarn=325

%module GeocodingResult

#ifdef _MASSIF_GEOCODING_SUPPORT

!proxy_imports(massif::GeocodingResult, geocoding.GeocodingAddress, geometry.FeatureCollection, projections.Projection)

%{
#include "geocoding/GeocodingResult.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <massifswig.i>

%import "geocoding/GeocodingAddress.i"
%import "geometry/FeatureCollection.i"
%import "projections/Projection.i"

!shared_ptr(massif::GeocodingResult, geocoding.GeocodingResult)

%attributestring(massif::GeocodingResult, std::shared_ptr<massif::Projection>, Projection, getProjection)
%attributestring(massif::GeocodingResult, std::shared_ptr<massif::FeatureCollection>, FeatureCollection, getFeatureCollection)
%attributeval(massif::GeocodingResult, massif::GeocodingAddress, Address, getAddress)
%attribute(massif::GeocodingResult, float, Rank, getRank)
%std_exceptions(massif::GeocodingResult::GeocodingResult)
!standard_equals(massif::GeocodingResult);
!custom_tostring(massif::GeocodingResult);

%include "geocoding/GeocodingResult.h"

!value_template(std::vector<std::shared_ptr<massif::GeocodingResult> >, geocoding.GeocodingResultVector);

#endif

#endif
