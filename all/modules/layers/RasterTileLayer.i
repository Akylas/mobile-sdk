#ifndef _RASTERTILELAYER_I
#define _RASTERTILELAYER_I

%module RasterTileLayer

!proxy_imports(carto::RasterTileLayer, datasources.TileDataSource, layers.TileLayer, layers.RasterTileEventListener)

%{
#include "layers/RasterTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"
%import "layers/RasterTileEventListener.i"
%import "layers/TileLayer.i"

!enum(carto::RasterTileFilterMode::RasterTileFilterMode)
!polymorphic_shared_ptr(carto::RasterTileLayer, layers.RasterTileLayer)

%attribute(carto::RasterTileLayer, std::size_t, TextureCacheCapacity, getTextureCacheCapacity, setTextureCacheCapacity)
%attribute(carto::RasterTileLayer, carto::RasterTileFilterMode::RasterTileFilterMode, TileFilterMode, getTileFilterMode, setTileFilterMode)
%attribute(carto::RasterTileLayer, float, TileBlendingSpeed, getTileBlendingSpeed, setTileBlendingSpeed)
!attributestring_polymorphic(carto::RasterTileLayer, layers.RasterTileEventListener, RasterTileEventListener, getRasterTileEventListener, setRasterTileEventListener)
%std_exceptions(carto::RasterTileLayer::RasterTileLayer)
%ignore carto::RasterTileLayer::FetchTask;
%ignore carto::RasterTileLayer::getMinZoom;
%ignore carto::RasterTileLayer::getMaxZoom;

%include "layers/RasterTileLayer.h"

#endif
