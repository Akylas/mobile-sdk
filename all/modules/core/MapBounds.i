#ifndef _MAPBOUNDS_I
#define _MAPBOUNDS_I

%module MapBounds

!proxy_imports(massif::MapBounds, core.MapPos, core.MapVec)

%{
#include "core/MapBounds.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapVec.i"

!value_type(massif::MapBounds, core.MapBounds)

%attributeval(massif::MapBounds, massif::MapPos, Max, getMax)
%attributeval(massif::MapBounds, massif::MapPos, Center, getCenter)
%attributeval(massif::MapBounds, massif::MapPos, Min, getMin)
%attributeval(massif::MapBounds, massif::MapVec, Delta, getDelta)
!objc_rename(containsPos) massif::MapBounds::contains(const MapPos&) const;
!objc_rename(containsBounds) massif::MapBounds::contains(const MapBounds&) const;
%ignore massif::MapBounds::setBounds;
%ignore massif::MapBounds::setMin;
%ignore massif::MapBounds::setMax;
%ignore massif::MapBounds::expandToContain;
!custom_equals(massif::MapBounds);
!custom_tostring(massif::MapBounds);

%include "core/MapBounds.h"

#endif
