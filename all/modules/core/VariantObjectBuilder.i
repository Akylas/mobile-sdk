#ifndef _VARIANTOBJECTBUILDER_I
#define _VARIANTOBJECTBUILDER_I

%module VariantObjectBuilder

!proxy_imports(massif::VariantObjectBuider, core.Variant)

%{
#include "core/VariantObjectBuilder.h"
#include <memory>
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"

!value_type(massif::VariantObjectBuilder, core.VariantObjectBuilder)

!standard_equals(massif::VariantObjectBuilder);

%include "core/VariantObjectBuilder.h"

#endif
