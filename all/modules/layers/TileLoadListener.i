#ifndef _TILELOADLISTENER_I
#define _TILELOADLISTENER_I

%module(directors="1") TileLoadListener

%{
#include "layers/TileLoadListener.h"	
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>

!polymorphic_shared_ptr(massif::TileLoadListener, layers.TileLoadListener)

%feature("director") massif::TileLoadListener;

%include "layers/TileLoadListener.h"

#endif
