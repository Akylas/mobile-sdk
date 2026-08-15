#ifndef _MAPBOXELEVATIONDATADECODER_I
#define _MAPBOXELEVATIONDATADECODER_I

%module MapBoxElevationDataDecoder

%module(directors="1") MapBoxElevationDataDecoder
!proxy_imports(massif::MapBoxElevationDataDecoder, graphics.Color, core.MapPos, core.MapPosVector, datasources.TileDataSource, rastertiles.ElevationDecoder)

%{
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"
%import "rastertiles/ElevationDecoder.i"

!polymorphic_shared_ptr(massif::MapBoxElevationDataDecoder, rastertiles.MapBoxElevationDataDecoder)
!standard_equals(massif::MapBoxElevationDataDecoder);

%feature("director") massif::MapBoxElevationDataDecoder;

%ignore massif::MapBoxElevationDataDecoder::getColorComponentCoefficients;
%ignore massif::MapBoxElevationDataDecoder::getVectorTileScales;

%include "rastertiles/MapBoxElevationDataDecoder.h"

#endif
