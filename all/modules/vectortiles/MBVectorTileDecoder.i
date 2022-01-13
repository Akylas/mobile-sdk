#ifndef _MBVECTORTILEDECODER_I
#define _MBVECTORTILEDECODER_I

%module MBVectorTileDecoder

!proxy_imports(carto::MBVectorTileDecoder, core.BinaryData, core.StringVector, graphics.Color, styles.CompiledStyleSet, styles.CartoCSSStyleSet, vectortiles.VectorTileDecoder)

%{
#include "vectortiles/MBVectorTileDecoder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "core/BinaryData.i"
%import "styles/CompiledStyleSet.i"
%import "styles/CartoCSSStyleSet.i"
%import "vectortiles/VectorTileDecoder.i"

!polymorphic_shared_ptr(carto::MBVectorTileDecoder, vectortiles.MBVectorTileDecoder)

%attributeval(carto::MBVectorTileDecoder, std::vector<std::string>, StyleParameters, getStyleParameters)
%attributestring(carto::MBVectorTileDecoder, std::shared_ptr<carto::CompiledStyleSet>, CompiledStyle, getCompiledStyleSet, setCompiledStyleSet)
%attributestring(carto::MBVectorTileDecoder, std::shared_ptr<carto::CartoCSSStyleSet>, CartoCSSStyle, getCartoCSSStyleSet, setCartoCSSStyleSet)
%attribute(carto::MBVectorTileDecoder, bool, FeatureIdOverride, isFeatureIdOverride, setFeatureIdOverride)
%std_exceptions(carto::MBVectorTileDecoder::MBVectorTileDecoder)
%std_exceptions(carto::MBVectorTileDecoder::setCompiledStyleSet)
%std_exceptions(carto::MBVectorTileDecoder::setCartoCSSStyleSet)
%std_exceptions(carto::MBVectorTileDecoder::getStyleParameter)
%std_exceptions(carto::MBVectorTileDecoder::setStyleParameter)
%ignore carto::MBVectorTileDecoder::isCartoCSSLayerNamesIgnored;
%ignore carto::MBVectorTileDecoder::setCartoCSSLayerNamesIgnored;
%ignore carto::MBVectorTileDecoder::getLayerNameOverride;
%ignore carto::MBVectorTileDecoder::setLayerNameOverride;
%ignore carto::MBVectorTileDecoder::decodeFeature;
%ignore carto::MBVectorTileDecoder::decodeFeatures;
%ignore carto::MBVectorTileDecoder::decodeTile;
%ignore carto::MBVectorTileDecoder::getMapSettings;
%ignore carto::MBVectorTileDecoder::getNutiParameters;
%ignore carto::MBVectorTileDecoder::loadMapnikMap;
%ignore carto::MBVectorTileDecoder::loadCartoCSSMap;

%include "vectortiles/MBVectorTileDecoder.h"

#endif
