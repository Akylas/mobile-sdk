#ifndef _CELESTIALLAYER_I
#define _CELESTIALLAYER_I

%module CelestialLayer

!proxy_imports(carto::CelestialLayer, celestial.CelestialObject, celestial.CelestialObjectVector, layers.Layer, layers.CelestialEventListener)

%{
#include "layers/CelestialLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "layers/Layer.i"
%import "layers/CelestialEventListener.i"
%import "celestial/CelestialObject.i"

!polymorphic_shared_ptr(carto::CelestialLayer, layers.CelestialLayer)

!attributestring_polymorphic(carto::CelestialLayer, layers.CelestialEventListener, CelestialEventListener, getCelestialEventListener, setCelestialEventListener)
%std_exceptions(carto::CelestialLayer::add)
%std_exceptions(carto::CelestialLayer::addAll)
%ignore carto::CelestialLayer::onDrawFrame;
%ignore carto::CelestialLayer::calculateRayIntersectedElements;
%ignore carto::CelestialLayer::processClick;

%include "layers/CelestialLayer.h"

#endif
