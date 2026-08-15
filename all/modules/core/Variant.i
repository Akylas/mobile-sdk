#ifndef _VARIANT_I
#define _VARIANT_I

%module Variant

!proxy_imports(massif::Variant, core.StringVector, core.VariantVector, core.StringVariantMap)

%{
#include "core/Variant.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_vector.i>
%include <std_map.i>
%include <massifswig.i>

%import "core/StringVector.i"

!enum(massif::VariantType::VariantType)
!value_type(massif::Variant, core.Variant)
!value_type(std::vector<massif::Variant>, core.VariantVector)
!value_type(std::map<std::string, massif::Variant>, core.StringVariantMap)

%attribute(massif::Variant, massif::VariantType::VariantType, Type, getType)
%attribute(massif::Variant, bool, Bool, getBool)
%attribute(massif::Variant, long long, Long, getLong)
%attribute(massif::Variant, double, Double, getDouble)
%attributestring(massif::Variant, std::string, String, getString)
%attribute(massif::Variant, int, ArraySize, getArraySize)
%attributeval(massif::Variant, std::vector<std::string>, ObjectKeys, getObjectKeys)
%std_exceptions(massif::Variant::FromString)
%ignore massif::Variant::Variant(const char*);
%ignore massif::Variant::toPicoJSON;
%ignore massif::Variant::FromPicoJSON;
!custom_equals(massif::Variant);
!custom_tostring(massif::Variant);

%include "core/Variant.h"

!value_template(std::vector<massif::Variant>, core.VariantVector)
!value_template(std::map<std::string, massif::Variant>, core.StringVariantMap)

#endif
