#ifndef _MAPPOS_I
#define _MAPPOS_I

#pragma SWIG nowarn=317

%module MapPos

!proxy_imports(massif::MapPos, core.MapVec)

%{
#include "core/MapPos.h"
#include "core/MapVec.h"
%}

%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/MapVec.i" 

!value_type(massif::MapPos, core.MapPos)
!value_type(std::vector<massif::MapPos>, core.MapPosVector)
!value_type(std::vector<std::vector<massif::MapPos> >, core.MapPosVectorVector)

%attribute(massif::MapPos, double, Z, getZ)
%attribute(massif::MapPos, double, X, getX)
%attribute(massif::MapPos, double, Y, getY)
%rename(add) massif::MapPos::operator+;
%rename(subVec) massif::MapPos::operator-(const MapVec &) const;
%rename(subPos) massif::MapPos::operator-(const MapPos &) const;
%rename(get) massif::MapPos::operator[] const;
%ignore massif::MapPos::operator[];
%ignore massif::MapPos::setX;
%ignore massif::MapPos::setY;
%ignore massif::MapPos::setZ;
%ignore massif::MapPos::setCoords;
%ignore massif::MapPos::operator!=;
%ignore massif::MapPos::operator+=;
%ignore massif::MapPos::operator-=;
%ignore massif::MapPos::rotate2D;
!custom_equals(massif::MapPos);
!custom_tostring(massif::MapPos);

%include "core/MapPos.h"

!value_template(std::vector<massif::MapPos>, core.MapPosVector)
!value_template(std::vector<std::vector<massif::MapPos> >, core.MapPosVectorVector)

#endif
