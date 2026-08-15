#ifndef _BALLOONPOPUPBUTTON_I
#define _BALLOONPOPUPBUTTON_I

#pragma SWIG nowarn=401

%module BalloonPopupButton

!proxy_imports(massif::BalloonPopupButton, core.Variant, styles.BalloonPopupButtonStyle)

%{
#include "vectorelements/BalloonPopupButton.h"
#include "core/Variant.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/Variant.i"
%import "styles/BalloonPopupButtonStyle.i"

!polymorphic_shared_ptr(massif::BalloonPopupButton, vectorelements.BalloonPopupButton)

%attributestring(massif::BalloonPopupButton, std::string, Text, getText)
!attributestring_polymorphic(massif::BalloonPopupButton, styles.BalloonPopupButtonStyle, Style, getStyle)
%attributeval(massif::BalloonPopupButton, massif::Variant, Tag, getTag, setTag)
%std_exceptions(massif::BalloonPopupButton::BalloonPopupButton)

%include "vectorelements/BalloonPopupButton.h"

#endif
