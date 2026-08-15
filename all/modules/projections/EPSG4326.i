#ifndef _EPSG4326_I
#define _EPSG4326_I

%module EPSG4326

!proxy_imports(massif::EPSG4326, core.MapBounds, core.MapPos, projections.Projection)

%{
#include "projections/EPSG4326.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "projections/Projection.i"

!polymorphic_shared_ptr(massif::EPSG4326, projections.EPSG4326)

%include "projections/EPSG4326.h"

#endif
