#ifndef _SCREENPOS_I
#define _SCREENPOS_I

%module ScreenPos

%{
#include "core/ScreenPos.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

!value_type(massif::ScreenPos, core.ScreenPos)
!value_type(std::vector<massif::ScreenPos>, core.ScreenPosVector)

%attribute(massif::ScreenPos, float, X, getX)
%attribute(massif::ScreenPos, float, Y, getY)
%rename(get) massif::ScreenPos::operator[] const;
%ignore massif::ScreenPos::operator[];
%ignore massif::ScreenPos::setX;
%ignore massif::ScreenPos::setY;
%ignore massif::ScreenPos::setCoords;
!custom_equals(massif::ScreenPos);
!custom_tostring(massif::ScreenPos);

%include "core/ScreenPos.h"

!value_template(std::vector<massif::ScreenPos>, core.ScreenPosVector)

#endif
