#ifndef _FEATUREBUILDER_I
#define _FEATUREBUILDER_I

%module FeatureBuilder

!proxy_imports(massif::FeatureBuilder, core.Variant, geometry.Geometry, geometry.Feature)

%{
#include "geometry/FeatureBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/Variant.i"
%import "geometry/Geometry.i"
%import "geometry/Feature.i"

!polymorphic_shared_ptr(massif::FeatureBuilder, geometry.FeatureBuilder)

!attributestring_polymorphic(massif::FeatureBuilder, geometry.Geometry, Geometry, getGeometry, setGeometry)
!standard_equals(massif::FeatureBuilder);

%include "geometry/FeatureBuilder.h"

#endif
