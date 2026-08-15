#ifndef _COLOR_I
#define _COLOR_I

%module Color

%{
#include "graphics/Color.h"
%}

%include <std_string.i>
%include <massifswig.i>

!value_type(massif::Color, graphics.Color)

%attribute(massif::Color, int, ARGB, getARGB)
%attribute(massif::Color, unsigned char, R, getR)
%attribute(massif::Color, unsigned char, G, getG)
%attribute(massif::Color, unsigned char, B, getB)
%attribute(massif::Color, unsigned char, A, getA)
!custom_equals(massif::Color);
!custom_tostring(massif::Color);

%include "graphics/Color.h"

#endif
