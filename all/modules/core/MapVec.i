#ifndef _MAPVEC_I
#define _MAPVEC_I

%module MapVec

%{
#include "core/MapVec.h"
%}

%include <std_string.i>
%include <massifswig.i>

!value_type(massif::MapVec, core.MapVec)

%attribute(massif::MapVec, double, Z, getZ)
%attribute(massif::MapVec, double, X, getX)
%attribute(massif::MapVec, double, Y, getY)
%attribute(massif::MapVec, double, Length, length)
%attributeval(massif::MapVec, massif::MapVec, Normalized, getNormalized)
%rename(add) massif::MapVec::operator+;
%rename(sub) massif::MapVec::operator-;
%rename(mul) massif::MapVec::operator*;
%rename(div) massif::MapVec::operator/;
%rename(get) massif::MapVec::operator[] const;
%ignore massif::MapVec::operator[];
%ignore massif::MapVec::setX;
%ignore massif::MapVec::setY;
%ignore massif::MapVec::setZ;
%ignore massif::MapVec::setCoords;
%ignore massif::MapVec::operator+=;
%ignore massif::MapVec::operator-=;
%ignore massif::MapVec::operator*=;
%ignore massif::MapVec::operator/=;
%ignore massif::MapVec::normalize;
%ignore massif::MapVec::lengthSqr;
!custom_equals(massif::MapVec);
!custom_tostring(massif::MapVec);

%include "core/MapVec.h"

#endif
