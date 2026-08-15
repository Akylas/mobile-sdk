#ifndef _MULTIPOLYGONGEOMETRY_I
#define _MULTIPOLYGONGEOMETRY_I

%module MultiPolygonGeometry

!proxy_imports(massif::MultiPolygonGeometry, geometry.Geometry, geometry.MultiGeometry, geometry.PolygonGeometry, geometry.PolygonGeometryVector)

%{
#include "geometry/MultiPolygonGeometry.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geometry/Geometry.i"
%import "geometry/PolygonGeometry.i"
%import "geometry/MultiGeometry.i"

!polymorphic_shared_ptr(massif::MultiPolygonGeometry, geometry.MultiPolygonGeometry)

%std_exceptions(massif::MultiPolygonGeometry::getGeometry)

%include "geometry/MultiPolygonGeometry.h"

#endif
