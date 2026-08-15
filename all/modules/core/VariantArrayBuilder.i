#ifndef _VARIANTARRAYBUILDER_I
#define _VARIANTARRAYBUILDER_I

%module VariantArrayBuilder

!proxy_imports(massif::VariantArrayBuider, core.Variant)

%{
#include "core/VariantArrayBuilder.h"
#include <memory>
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"

!value_type(massif::VariantArrayBuilder, core.VariantArrayBuilder)

!standard_equals(massif::VariantArrayBuilder);

%include "core/VariantArrayBuilder.h"

#endif
