#ifndef _CUSTOMRASTERTILELAYER_I
#define _CUSTOMRASTERTILELAYER_I

%module CustomRasterTileLayer

!proxy_imports(carto::CustomRasterTileLayer, datasources.TileDataSource, layers.RasterTileLayer)

%{
#include "layers/CustomRasterTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"
%import "layers/RasterTileLayer.i"

!polymorphic_shared_ptr(carto::CustomRasterTileLayer, layers.CustomRasterTileLayer)

%attributestring(carto::CustomRasterTileLayer, std::string, ShaderSource, getShaderSource, setShaderSource)
%std_exceptions(carto::CustomRasterTileLayer::CustomRasterTileLayer)

%include "layers/CustomRasterTileLayer.h"

#endif
