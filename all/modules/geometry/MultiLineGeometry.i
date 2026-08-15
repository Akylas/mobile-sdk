#ifndef _MULTILINEGEOMETRY_I
#define _MULTILINEGEOMETRY_I

%module MultiLineGeometry

!proxy_imports(massif::MultiLineGeometry, geometry.Geometry, geometry.MultiGeometry, geometry.LineGeometry, geometry.LineGeometryVector)

%{
#include "geometry/MultiLineGeometry.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geometry/Geometry.i"
%import "geometry/LineGeometry.i"
%import "geometry/MultiGeometry.i"

!polymorphic_shared_ptr(massif::MultiLineGeometry, geometry.MultiLineGeometry)

%std_exceptions(massif::MultiLineGeometry::getGeometry)

%include "geometry/MultiLineGeometry.h"

#endif
