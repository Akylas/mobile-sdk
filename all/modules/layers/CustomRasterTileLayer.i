#ifndef _CUSTOMRASTERTILELAYER_I
#define _CUSTOMRASTERTILELAYER_I

%module CustomRasterTileLayer

!proxy_imports(massif::CustomRasterTileLayer, datasources.TileDataSource, layers.RasterTileLayer)

%{
#include "layers/CustomRasterTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "datasources/TileDataSource.i"
%import "layers/RasterTileLayer.i"

!polymorphic_shared_ptr(massif::CustomRasterTileLayer, layers.CustomRasterTileLayer)

%attributestring(massif::CustomRasterTileLayer, std::string, ShaderSource, getShaderSource, setShaderSource)
%std_exceptions(massif::CustomRasterTileLayer::CustomRasterTileLayer)

%include "layers/CustomRasterTileLayer.h"

#endif
