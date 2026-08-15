#ifndef _POPUPDRAWINFO_I
#define _POPUPDRAWINFO_I

%module PopupDrawInfo

!proxy_imports(massif::PopupDrawInfo, core.ScreenPos, core.ScreenBounds, vectorelements.Popup)

%{
#include "ui/PopupDrawInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/ScreenPos.i"
%import "core/ScreenBounds.i"
%import "vectorelements/Popup.i"

!shared_ptr(massif::PopupDrawInfo, ui.PopupDrawInfo)

%attributeval(massif::PopupDrawInfo, massif::ScreenPos, AnchorScreenPos, getAnchorScreenPos)
%attributeval(massif::PopupDrawInfo, massif::ScreenBounds, ScreenBounds, getScreenBounds)
!attributestring_polymorphic(massif::PopupDrawInfo, vectorelements.Popup, Popup, getPopup)
%attribute(massif::PopupDrawInfo, float, DPToPX, getDPToPX)
!standard_equals(massif::PopupDrawInfo);

%include "ui/PopupDrawInfo.h"

#endif
