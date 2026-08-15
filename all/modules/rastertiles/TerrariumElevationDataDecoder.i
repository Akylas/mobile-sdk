#ifndef _TERRARIUMELEVATIONDATADECODER_I
#define _TERRARIUMELEVATIONDATADECODER_I

%module TerrariumElevationDataDecoder

%module(directors="1") TerrariumElevationDataDecoder
!proxy_imports(massif::TerrariumElevationDataDecoder, graphics.Color, core.MapPos, core.MapPosVector, datasources.TileDataSource, rastertiles.ElevationDecoder)

%{
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"
%import "rastertiles/ElevationDecoder.i"

!polymorphic_shared_ptr(massif::TerrariumElevationDataDecoder, rastertiles.TerrariumElevationDataDecoder)
!standard_equals(massif::TerrariumElevationDataDecoder);

%feature("director") massif::TerrariumElevationDataDecoder;

%ignore massif::TerrariumElevationDataDecoder::getColorComponentCoefficients;
%ignore massif::TerrariumElevationDataDecoder::getVectorTileScales;

%include "rastertiles/TerrariumElevationDataDecoder.h"

#endif
