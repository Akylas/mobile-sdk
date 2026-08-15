#ifndef _VECTORELEMENTEVENTLISTENER_I
#define _VECTORELEMENTEVENTLISTENER_I

%module(directors="1") VectorElementEventListener

!proxy_imports(massif::VectorElementEventListener, ui.VectorElementClickInfo)

%{
#include "layers/VectorElementEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/VectorElementClickInfo.i"

!polymorphic_shared_ptr(massif::VectorElementEventListener, layers.VectorElementEventListener)

%feature("director") massif::VectorElementEventListener;

%include "layers/VectorElementEventListener.h"

#endif
