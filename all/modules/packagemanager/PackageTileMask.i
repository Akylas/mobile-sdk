#ifndef _PACKAGETILEMASK_I
#define _PACKAGETILEMASK_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module PackageTileMask

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageTileMask, core.MapTile, geometry.MultiPolygonGeometry, projections.Projection)

%{
#include "packagemanager/PackageTileMask.h"
#include "geometry/MultiPolygonGeometry.h"
#include "projections/Projection.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/MapTile.i"
%import "geometry/MultiPolygonGeometry.i"
%import "projections/Projection.i"

!enum(massif::PackageTileStatus::PackageTileStatus)
!shared_ptr(massif::PackageTileMask, packagemanager.PackageTileMask)

%attributestring(massif::PackageTileMask, std::string, StringValue, getStringValue)
%attribute(massif::PackageTileMask, int, MaxZoomLevel, getMaxZoomLevel)
%ignore massif::PackageTileMask::Tile;
%ignore massif::PackageTileMask::getURLSafeStringValue;
%ignore massif::PackageTileMask::PackageTileMask;
!standard_equals(massif::PackageTileMask);

%include "packagemanager/PackageTileMask.h"

#endif

#endif
