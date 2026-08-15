#ifndef _RASTERTILECLICKINFO_I
#define _RASTERTILECLICKINFO_I

%module RasterTileClickInfo

!proxy_imports(massif::RasterTileClickInfo, core.MapPos, core.MapTile, graphics.Color, layers.Layer, ui.ClickInfo)

%{
#include "ui/RasterTileClickInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "core/MapPos.i"
%import "core/MapTile.i"
%import "graphics/Color.i"
%import "layers/Layer.i"

!shared_ptr(massif::RasterTileClickInfo, ui.RasterTileClickInfo)

%attribute(massif::RasterTileClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::RasterTileClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
%attributeval(massif::RasterTileClickInfo, massif::MapPos, ClickPos, getClickPos)
%attributeval(massif::RasterTileClickInfo, massif::MapTile, MapTile, getMapTile)
%attributeval(massif::RasterTileClickInfo, massif::Color, NearestColor, getNearestColor)
%attributeval(massif::RasterTileClickInfo, massif::Color, InterpolatedColor, getInterpolatedColor)
!attributestring_polymorphic(massif::RasterTileClickInfo, layers.Layer, Layer, getLayer)
%ignore massif::RasterTileClickInfo::RasterTileClickInfo;
!standard_equals(massif::RasterTileClickInfo);

%include "ui/RasterTileClickInfo.h"

#endif
