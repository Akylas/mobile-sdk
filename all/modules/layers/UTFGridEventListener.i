#ifndef _UTFGRIDEVENTLISTENER_I
#define _UTFGRIDEVENTLISTENER_I

%module(directors="1") UTFGridEventListener

!proxy_imports(massif::UTFGridEventListener, ui.UTFGridClickInfo)

%{
#include "layers/UTFGridEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/UTFGridClickInfo.i"

!polymorphic_shared_ptr(massif::UTFGridEventListener, layers.UTFGridEventListener)

%feature("director") massif::UTFGridEventListener;

%include "layers/UTFGridEventListener.h"

#endif
