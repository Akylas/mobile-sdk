#ifndef _HILLSHADERASTERTILELAYER_I
#define _HILLSHADERASTERTILELAYER_I

%module HillshadeRasterTileLayer

!proxy_imports(carto::HillshadeRasterTileLayer, core.MapPos, core.MapVec, core.MapPosVector, core.DoubleVector, datasources.TileDataSource, rastertiles.ElevationDecoder, graphics.Color, layers.RasterTileLayer)

%{
#include "layers/HillshadeRasterTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"
%import "rastertiles/ElevationDecoder.i"
%import "graphics/Color.i"
%import "layers/RasterTileLayer.i"
%import "core/DoubleVector.i"

!polymorphic_shared_ptr(carto::HillshadeRasterTileLayer, layers.HillshadeRasterTileLayer)

%attribute(carto::HillshadeRasterTileLayer, float, Contrast, getContrast, setContrast)
%attribute(carto::HillshadeRasterTileLayer, float, HeightScale, getHeightScale, setHeightScale)
%attribute(carto::HillshadeRasterTileLayer, carto::MapVec, IlluminationDirection, getIlluminationDirection, setIlluminationDirection)
%attribute(carto::HillshadeRasterTileLayer, bool, IlluminationMapRotationEnabled, getIlluminationMapRotationEnabled, setIlluminationMapRotationEnabled)
%attribute(carto::HillshadeRasterTileLayer, bool, ExagerateHeightScaleEnabled, getExagerateHeightScaleEnabled, setExagerateHeightScaleEnabled)
%attribute(carto::HillshadeRasterTileLayer, carto::HillshadeMethod, HillshadeMethod, getHillshadeMethod, setHillshadeMethod)
%attributeval(carto::HillshadeRasterTileLayer, carto::Color, ShadowColor, getShadowColor, setShadowColor)
%attributeval(carto::HillshadeRasterTileLayer, carto::Color, HighlightColor, getHighlightColor, setHighlightColor)
%attributeval(carto::HillshadeRasterTileLayer, carto::Color, AccentColor, getAccentColor, setAccentColor)
%attributeval(carto::HillshadeRasterTileLayer, std::string, NormalMapLightingShader, getNormalMapLightingShader, setNormalMapLightingShader)
%std_exceptions(carto::HillshadeRasterTileLayer::HillshadeRasterTileLayer)

%include "layers/HillshadeRasterTileLayer.h"

#endif
