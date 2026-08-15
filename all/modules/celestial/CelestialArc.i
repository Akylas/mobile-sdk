#ifndef _CELESTIALARC_I
#define _CELESTIALARC_I

%module(directors="1") CelestialArc

!proxy_imports(massif::CelestialArc, celestial.CelestialObject, core.DoubleVector)

%{
#include "celestial/CelestialArc.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <massifswig.i>

%import "celestial/CelestialObject.i"
%import "core/DoubleVector.i"

!polymorphic_shared_ptr(massif::CelestialArc, celestial.CelestialArc)

%attribute(massif::CelestialArc, float, Radius, getRadius)
%attribute(massif::CelestialArc, float, Width, getWidth, setWidth)
%attribute(massif::CelestialArc, bool, BelowHorizonVisible, isBelowHorizonVisible, setBelowHorizonVisible)
%attribute(massif::CelestialArc, float, ClickRadius, getClickRadius, setClickRadius)
%attribute(massif::CelestialArc, bool, Segmented, isSegmented)
%ignore massif::CelestialArc::buildDirections;
%std_exceptions(massif::CelestialArc::CelestialArc)

%include "celestial/CelestialArc.h"

#endif
