#ifndef _DOUGLASPEUCKERGEOMETRYSIMPLIFIER_I
#define _DOUGLASPEUCKERGEOMETRYSIMPLIFIER_I

%module DouglasPeuckerGeometrySimplifier

!proxy_imports(massif::DouglasPeuckerGeometrySimplifier, geometry.Geometry, geometry.GeometrySimplifier, projections.Projection)

%{
#include "geometry/DouglasPeuckerGeometrySimplifier.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "geometry/GeometrySimplifier.i"

!polymorphic_shared_ptr(massif::DouglasPeuckerGeometrySimplifier, geometry.DouglasPeuckerGeometrySimplifier)

%ignore massif::DouglasPeuckerGeometrySimplifier::simplify;

%include "geometry/DouglasPeuckerGeometrySimplifier.h"

#endif
