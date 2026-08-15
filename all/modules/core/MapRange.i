#ifndef _MAPRANGE_I
#define _MAPRANGE_I

%module MapRange

%{
#include "core/MapRange.h"
%}

%include <std_string.i>
%include <massifswig.i>

!value_type(massif::MapRange, core.MapRange)

%attribute(massif::MapRange, float, Max, getMax)
%attribute(massif::MapRange, float, Min, getMin)
%attribute(massif::MapRange, float, Midrange, getMidrange)
%attribute(massif::MapRange, float, Length, length)
%ignore massif::MapRange::setRange;
%ignore massif::MapRange::setMin;
%ignore massif::MapRange::setMax;
!custom_equals(massif::MapRange);
!custom_tostring(massif::MapRange);

%include "core/MapRange.h"

#endif
