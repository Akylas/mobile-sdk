#ifndef _ELEVATIONDECODER_I
#define _ELEVATIONDECODER_I

%module ElevationDecoder

!proxy_imports(massif::ElevationDecoder, graphics.Color)

%{
#include "rastertiles/ElevationDecoder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"

!polymorphic_shared_ptr(massif::ElevationDecoder, rastertiles.ElevationDecoder)

!standard_equals(massif::ElevationDecoder);
%ignore massif::ElevationDecoder::getColorComponentCoefficients;
%ignore massif::ElevationDecoder::getVectorTileScales;

%include "rastertiles/ElevationDecoder.h"

#endif
