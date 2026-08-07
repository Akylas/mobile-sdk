#ifndef _CELESTIALEVENTLISTENER_I
#define _CELESTIALEVENTLISTENER_I

%module(directors="1") CelestialEventListener

!proxy_imports(carto::CelestialEventListener, celestial.CelestialObject, ui.ClickInfo)

%{
#include "layers/CelestialEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "celestial/CelestialObject.i"
%import "ui/ClickInfo.i"

!polymorphic_shared_ptr(carto::CelestialEventListener, layers.CelestialEventListener)

%feature("director") carto::CelestialEventListener;

%include "layers/CelestialEventListener.h"

#endif
