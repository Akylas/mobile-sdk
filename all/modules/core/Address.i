#ifndef _ADDRESS_I
#define _ADDRESS_I

#pragma SWIG nowarn=325

%module Address

#ifdef _MASSIF_GEOCODING_SUPPORT

!proxy_imports(massif::Address, core.StringVector)

%{
#include "core/Address.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/StringVector.i"

!value_type(massif::Address, core.Address)

%attributestring(massif::Address, std::string, Country, getCountry)
%attributestring(massif::Address, std::string, Region, getRegion)
%attributestring(massif::Address, std::string, County, getCounty)
%attributestring(massif::Address, std::string, Locality, getLocality)
%attributestring(massif::Address, std::string, Neighbourhood, getNeighbourhood)
%attributestring(massif::Address, std::string, Street, getStreet)
%attributestring(massif::Address, std::string, Postcode, getPostcode)
%attributestring(massif::Address, std::string, HouseNumber, getHouseNumber)
%attributestring(massif::Address, std::string, Name, getName)
%attributeval(massif::Address, std::vector<std::string>, Categories, getCategories)
!custom_equals(massif::Address);
!custom_tostring(massif::Address);

%include "core/Address.h"

#endif

#endif
