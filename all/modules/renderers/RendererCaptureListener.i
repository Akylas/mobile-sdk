#ifndef _RENDERERCAPTURELISTENER_I
#define _RENDERERCAPTURELISTENER_I

%module(directors="1") RendererCaptureListener

!proxy_imports(massif::RendererCaptureListener, graphics.Bitmap)

%{
#include "renderers/RendererCaptureListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"

!polymorphic_shared_ptr(massif::RendererCaptureListener, renderers.RendererCaptureListener)

%feature("director") massif::RendererCaptureListener;

%include "renderers/RendererCaptureListener.h"

#endif
