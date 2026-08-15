#ifndef _BITMAPOVERLAYRASTERTILEDATASOURCE_I
#define _BITMAPOVERLAYRASTERTILEDATASOURCE_I

%module(directors="1") BitmapOverlayRasterTileDataSource

!proxy_imports(massif::BitmapOverlayRasterTileDataSource, core.MapTile, core.MapPos, core.MapPosVector, core.MapBounds, core.ScreenPos, core.ScreenPosVector, core.ScreenPosVector, core.StringMap, datasources.TileDataSource, datasources.components.TileData, projections.Projection, graphics.Bitmap)

%{
#include "datasources/BitmapOverlayRasterTileDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapBounds.i"
%import "core/ScreenPos.i"
%import "core/MapTile.i"
%import "datasources/TileDataSource.i"
%import "datasources/components/TileData.i"
%import "projections/Projection.i"
%import "graphics/Bitmap.i"

!polymorphic_shared_ptr(massif::BitmapOverlayRasterTileDataSource, datasources.BitmapOverlayRasterTileDataSource)

%std_exceptions(massif::BitmapOverlayRasterTileDataSource::BitmapOverlayRasterTileDataSource)

%feature("director") massif::BitmapOverlayRasterTileDataSource;

%include "datasources/BitmapOverlayRasterTileDataSource.h"

#endif
