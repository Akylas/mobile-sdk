#ifndef _CUSTOMPOPUPHANDLER_I
#define _CUSTOMPOPUPHANDLER_I

%module(directors="1") CustomPopupHandler

!proxy_imports(massif::CustomPopupHandler, graphics.Bitmap, ui.PopupDrawInfo, ui.PopupClickInfo)

%{
#include "vectorelements/CustomPopupHandler.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"
%import "ui/PopupDrawInfo.i"
%import "ui/PopupClickInfo.i"

!polymorphic_shared_ptr(massif::CustomPopupHandler, vectorelements.CustomPopupHandler)

%feature("director") massif::CustomPopupHandler;

%include "vectorelements/CustomPopupHandler.h"

#endif
