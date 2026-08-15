#ifndef _MAPENVELOPE_I
#define _MAPENVELOPE_I

%module MapEnvelope

!proxy_imports(massif::MapEnvelope, core.MapBounds, core.MapPosVector)

%{
#include "core/MapEnvelope.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapBounds.i"

!value_type(massif::MapEnvelope, core.MapEnvelope)

%attributeval(massif::MapEnvelope, massif::MapBounds, Bounds, getBounds)
%attributeval(massif::MapEnvelope, std::vector<massif::MapPos>, ConvexHull, getConvexHull)
!custom_equals(massif::MapEnvelope);
!custom_tostring(massif::MapEnvelope);

%include "core/MapEnvelope.h"

#endif
