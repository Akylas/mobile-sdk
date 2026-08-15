#ifndef _MULTIGEOMETRY_I
#define _MULTIGEOMETRY_I

%module MultiGeometry

!proxy_imports(massif::MultiGeometry, core.MapPos, geometry.Geometry, geometry.GeometryVector)

%{
#include "geometry/MultiGeometry.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "geometry/Geometry.i"

!polymorphic_shared_ptr(massif::MultiGeometry, geometry.MultiGeometry)

%attribute(massif::MultiGeometry, int, GeometryCount, getGeometryCount)
%std_exceptions(massif::MultiGeometry::getGeometry)

%include "geometry/MultiGeometry.h"

#endif
