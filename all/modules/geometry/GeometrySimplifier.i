#ifndef _GEOMETRYSIMPLIFIER_I
#define _GEOMETRYSIMPLIFIER_I

%module GeometrySimplifier

!proxy_imports(massif::GeometrySimplifier, geometry.Geometry, projections.Projection)

%{
#include "geometry/GeometrySimplifier.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geometry/Geometry.i"
%import "projections/Projection.i"

!polymorphic_shared_ptr(massif::GeometrySimplifier, geometry.GeometrySimplifier)

%ignore massif::GeometrySimplifier::simplify;
!standard_equals(massif::GeometrySimplifier);

%include "geometry/GeometrySimplifier.h"

#endif
