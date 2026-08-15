#ifndef _RASTERTILEEVENTLISTENER_I
#define _RASTERTILEEVENTLISTENER_I

%module(directors="1") RasterTileEventListener

!proxy_imports(massif::RasterTileEventListener, ui.RasterTileClickInfo)

%{
#include "layers/RasterTileEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/RasterTileClickInfo.i"

!polymorphic_shared_ptr(massif::RasterTileEventListener, layers.RasterTileEventListener)

%feature("director") massif::RasterTileEventListener;

%include "layers/RasterTileEventListener.h"

#endif
