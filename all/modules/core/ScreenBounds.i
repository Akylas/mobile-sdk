#ifndef _SCREENBOUNDS_I
#define _SCREENBOUNDS_I

%module ScreenBounds

!proxy_imports(massif::ScreenBounds, core.ScreenPos)

%{
#include "core/ScreenBounds.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "core/ScreenPos.i"

!value_type(massif::ScreenBounds, core.ScreenBounds)

%attributeval(massif::ScreenBounds, massif::ScreenPos, Min, getMin)
%attributeval(massif::ScreenBounds, massif::ScreenPos, Max, getMax)
%attributeval(massif::ScreenBounds, massif::ScreenPos, Center, getCenter)
!objc_rename(containsPos) massif::ScreenBounds::contains(const ScreenPos&) const;
!objc_rename(containsBounds) massif::ScreenBounds::contains(const ScreenBounds&) const;
%ignore massif::ScreenBounds::setBounds;
%ignore massif::ScreenBounds::setMin;
%ignore massif::ScreenBounds::setMax;
%ignore massif::ScreenBounds::expandToContain;
!custom_equals(massif::ScreenBounds);
!custom_tostring(massif::ScreenBounds);

%include "core/ScreenBounds.h"

#endif
