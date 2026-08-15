#ifndef _BALLOONPOPUPBUTTONCLICKINFO_I
#define _BALLOONPOPUPBUTTONCLICKINFO_I

%module BalloonPopupButtonClickInfo

!proxy_imports(massif::BalloonPopupButtonClickInfo, ui.ClickInfo, vectorelements.BalloonPopupButton, vectorelements.VectorElement)

%{
#include "ui/BalloonPopupButtonClickInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "vectorelements/BalloonPopupButton.i"
%import "vectorelements/VectorElement.i"

!shared_ptr(massif::BalloonPopupButtonClickInfo, ui.BalloonPopupButtonClickInfo)

%attribute(massif::BalloonPopupButtonClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::BalloonPopupButtonClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
!attributestring_polymorphic(massif::BalloonPopupButtonClickInfo, vectorelements.BalloonPopupButton, Button, getButton)
!attributestring_polymorphic(massif::BalloonPopupButtonClickInfo, vectorelements.VectorElement, VectorElement, getVectorElement)
%ignore massif::BalloonPopupButtonClickInfo::BalloonPopupButtonClickInfo;
!standard_equals(massif::BalloonPopupButtonClickInfo);

%include "ui/BalloonPopupButtonClickInfo.h"

#endif
