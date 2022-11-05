#ifndef _PACKAGETILEMASK_I
#define _PACKAGETILEMASK_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module PackageTileMask

#ifdef _CARTO_PACKAGEMANAGER_SUPPORT

!proxy_imports(carto::PackageTileMask, core.MapTile, geometry.MultiPolygonGeometry, projections.Projection)

%{
#include "packagemanager/PackageTileMask.h"
#include "geometry/MultiPolygonGeometry.h"
#include "projections/Projection.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "core/MapTile.i"
%import "geometry/MultiPolygonGeometry.i"
%import "projections/Projection.i"

!enum(carto::PackageTileStatus::PackageTileStatus)
!shared_ptr(carto::PackageTileMask, packagemanager.PackageTileMask)

%attributestring(carto::PackageTileMask, std::string, StringValue, getStringValue)
%attribute(carto::PackageTileMask, int, MaxZoomLevel, getMaxZoomLevel)
%ignore carto::PackageTileMask::Tile;
%ignore carto::PackageTileMask::getURLSafeStringValue;
%ignore carto::PackageTileMask::PackageTileMask;
!standard_equals(carto::PackageTileMask);

%include "packagemanager/PackageTileMask.h"

#endif

#endif
