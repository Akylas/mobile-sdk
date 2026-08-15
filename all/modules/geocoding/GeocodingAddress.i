#ifndef _GEOCODINGADDRESS_I
#define _GEOCODINGADDRESS_I

#pragma SWIG nowarn=325

%module GeocodingAddress

#ifdef _MASSIF_GEOCODING_SUPPORT

!proxy_imports(massif::GeocodingAddress, core.Address, core.StringVector)

%{
#include "geocoding/GeocodingAddress.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/StringVector.i"
%import "core/Address.i"

!value_type(massif::GeocodingAddress, geocoding.GeocodingAddress)
!custom_tostring(massif::GecodingAddress);

%include "geocoding/GeocodingAddress.h"

#endif

#endif
