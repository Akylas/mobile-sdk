#ifndef _CELESTIALOBJECT_I
#define _CELESTIALOBJECT_I

#pragma SWIG nowarn=401

%module CelestialObject

!proxy_imports(carto::CelestialObject, core.MapPos, core.Variant, graphics.Color)

%{
#include "celestial/CelestialObject.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "core/Variant.i"
%import "graphics/Color.i"

!polymorphic_shared_ptr(carto::CelestialObject, celestial.CelestialObject)
!value_type(std::vector<std::shared_ptr<carto::CelestialObject> >, celestial.CelestialObjectVector)

%attribute(carto::CelestialObject, bool, DirectionAnchored, isDirectionAnchored)
%attribute(carto::CelestialObject, float, Azimuth, getAzimuth)
%attribute(carto::CelestialObject, float, Altitude, getAltitude)
%attribute(carto::CelestialObject, double, Distance, getDistance)
%attributeval(carto::CelestialObject, carto::MapPos, Position, getPosition)
%attribute(carto::CelestialObject, double, PositionAltitude, getPositionAltitude)
%attributeval(carto::CelestialObject, carto::Color, Color, getColor, setColor)
%attribute(carto::CelestialObject, bool, Visible, isVisible, setVisible)
%ignore carto::CelestialObject::calculateDirectionVector;
%ignore carto::CelestialObject::setComponents;
%ignore carto::CelestialObject::getLayer;
%std_exceptions(carto::CelestialObject::CelestialObject)
!standard_equals(carto::CelestialObject);

%include "celestial/CelestialObject.h"

!value_template(std::vector<std::shared_ptr<carto::CelestialObject> >, celestial.CelestialObjectVector);

#endif
