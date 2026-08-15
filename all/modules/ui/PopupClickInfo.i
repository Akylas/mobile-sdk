#ifndef _POPUPCLICKINFO_I
#define _POPUPCLICKINFO_I

%module PopupClickInfo

!proxy_imports(massif::PopupClickInfo, core.MapPos, core.ScreenPos, vectorelements.Popup, ui.ClickInfo)

%{
#include "ui/PopupClickInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "core/MapPos.i"
%import "core/ScreenPos.i"
%import "vectorelements/Popup.i"

!shared_ptr(massif::PopupClickInfo, ui.PopupClickInfo)

%attribute(massif::PopupClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::PopupClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
%attributeval(massif::PopupClickInfo, massif::MapPos, ClickPos, getClickPos)
%attributeval(massif::PopupClickInfo, massif::ScreenPos, ElementClickPos, getElementClickPos)
!attributestring_polymorphic(massif::PopupClickInfo, vectorelements.Popup, Popup, getPopup)
%ignore massif::PopupClickInfo::PopupClickInfo;
!standard_equals(massif::PopupClickInfo);

%include "ui/PopupClickInfo.h"

#endif
