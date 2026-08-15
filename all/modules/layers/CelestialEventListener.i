#ifndef _CELESTIALEVENTLISTENER_I
#define _CELESTIALEVENTLISTENER_I

%module(directors="1") CelestialEventListener

!proxy_imports(massif::CelestialEventListener, celestial.CelestialObject, ui.ClickInfo)

%{
#include "layers/CelestialEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "celestial/CelestialObject.i"
%import "ui/ClickInfo.i"

!polymorphic_shared_ptr(massif::CelestialEventListener, layers.CelestialEventListener)

%feature("director") massif::CelestialEventListener;

%include "layers/CelestialEventListener.h"

#endif
