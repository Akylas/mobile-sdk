#ifndef _MAPRENDERERLISTENER_I
#define _MAPRENDERERLISTENER_I

%module(directors="1") MapRendererListener

%{
#include "renderers/MapRendererListener.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>

!polymorphic_shared_ptr(massif::MapRendererListener, renderers.MapRendererListener)

%feature("director") massif::MapRendererListener;

%include "renderers/MapRendererListener.h"

#endif
