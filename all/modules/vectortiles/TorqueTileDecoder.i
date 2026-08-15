#ifndef _TORQUETILEDECODER_I
#define _TORQUETILEDECODER_I

%module TorqueTileDecoder

!proxy_imports(massif::TorqueTileDecoder, core.BinaryData, graphics.Color, styles.CartoCSSStyleSet, vectortiles.VectorTileDecoder)

%{
#include "vectortiles/TorqueTileDecoder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "styles/CartoCSSStyleSet.i"
%import "vectortiles/VectorTileDecoder.i"

!polymorphic_shared_ptr(massif::TorqueTileDecoder, vectortiles.TorqueTileDecoder)

%attribute(massif::TorqueTileDecoder, int, FrameCount, getFrameCount)
%attribute(massif::TorqueTileDecoder, int, Resolution, getResolution)
%attribute(massif::TorqueTileDecoder, float, AnimationDuration, getAnimationDuration)
%attributestring(massif::TorqueTileDecoder, std::shared_ptr<massif::CartoCSSStyleSet>, StyleSet, getStyleSet, setStyleSet)
%std_exceptions(massif::TorqueTileDecoder::TorqueTileDecoder)
%std_exceptions(massif::TorqueTileDecoder::setStyleSet)
%ignore massif::TorqueTileDecoder::decodeFeature;
%ignore massif::TorqueTileDecoder::decodeFeatures;
%ignore massif::TorqueTileDecoder::decodeTile;
%ignore massif::TorqueTileDecoder::getMapSettings;
%ignore massif::TorqueTileDecoder::getSymbolizerContextSettings;

%include "vectortiles/TorqueTileDecoder.h"

#endif
