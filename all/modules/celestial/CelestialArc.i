#ifndef _CELESTIALARC_I
#define _CELESTIALARC_I

%module(directors="1") CelestialArc

!proxy_imports(carto::CelestialArc, celestial.CelestialObject, core.DoubleVector)

%{
#include "celestial/CelestialArc.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "celestial/CelestialObject.i"
%import "core/DoubleVector.i"

!polymorphic_shared_ptr(carto::CelestialArc, celestial.CelestialArc)

%attribute(carto::CelestialArc, float, Radius, getRadius)
%attribute(carto::CelestialArc, float, Width, getWidth, setWidth)
%attribute(carto::CelestialArc, bool, BelowHorizonVisible, isBelowHorizonVisible, setBelowHorizonVisible)
%ignore carto::CelestialArc::buildDirections;
%std_exceptions(carto::CelestialArc::CelestialArc)

%include "celestial/CelestialArc.h"

#endif
