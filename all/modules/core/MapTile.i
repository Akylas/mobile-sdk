#ifndef _MAPTILE_I
#define _MAPTILE_I

#pragma SWIG nowarn=317

%module MapTile

%{
#include "core/MapTile.h"
%}

%include <std_string.i>
%include <massifswig.i>

!value_type(massif::MapTile, core.MapTile)

%attribute(massif::MapTile, int, X, getX)
%attribute(massif::MapTile, int, Y, getY)
%attribute(massif::MapTile, int, Zoom, getZoom)
%attribute(massif::MapTile, int, FrameNr, getFrameNr)
%attribute(massif::MapTile, long long, TileId, getTileId)
%ignore massif::MapTile::getParent;
%ignore massif::MapTile::getChild;
%ignore massif::MapTile::getFlipped;
!custom_equals(massif::MapTile);
!custom_tostring(massif::MapTile);

%include "core/MapTile.h"

#endif
