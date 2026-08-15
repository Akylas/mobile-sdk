#ifndef _VECTORTILEEVENTLISTENER_I
#define _VECTORTILEEVENTLISTENER_I

%module(directors="1") VectorTileEventListener

!proxy_imports(massif::VectorTileEventListener, ui.VectorTileClickInfo)

%{
#include "layers/VectorTileEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/VectorTileClickInfo.i"

!polymorphic_shared_ptr(massif::VectorTileEventListener, layers.VectorTileEventListener)

%feature("director") massif::VectorTileEventListener;

%include "layers/VectorTileEventListener.h"

#endif
