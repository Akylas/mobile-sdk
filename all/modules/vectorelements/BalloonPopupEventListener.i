#ifndef _BALLOONPOPUPEVENTLISTENER_I
#define _BALLOONPOPUPEVENTLISTENER_I

%module(directors="1") BalloonPopupEventListener

!proxy_imports(massif::BalloonPopupEventListener, ui.BalloonPopupButtonClickInfo)

%{
#include "vectorelements/BalloonPopupEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/BalloonPopupButtonClickInfo.i"

!polymorphic_shared_ptr(massif::BalloonPopupEventListener, vectorelements.BalloonPopupEventListener)

%feature("director") massif::BalloonPopupEventListener;

%include "vectorelements/BalloonPopupEventListener.h"

#endif
