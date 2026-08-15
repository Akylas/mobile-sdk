#ifndef _POPUP_I
#define _POPUP_I

%module Popup

!proxy_imports(massif::Popup, core.MapPos, core.ScreenPos, graphics.Bitmap, geometry.Geometry, geometry.PointGeometry, styles.PopupStyle, ui.ClickInfo, vectorelements.Billboard)
!java_imports(massif::Popup, com.massifmaps.ui.ClickType)

%{
#include "vectorelements/Popup.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/ScreenPos.i"
%import "graphics/Bitmap.i"
%import "styles/PopupStyle.i"
%import "ui/ClickInfo.i"
%import "vectorelements/Billboard.i"

!polymorphic_shared_ptr(massif::Popup, vectorelements.Popup)

%attribute(massif::Popup, float, AnchorPointX, getAnchorPointX, setAnchorPointX)
%attribute(massif::Popup, float, AnchorPointY, getAnchorPointY, setAnchorPointY)
!attributestring_polymorphic(massif::Popup, styles.PopupStyle, Style, getStyle, setStyle)
%std_exceptions(massif::Popup::Popup)
%std_exceptions(massif::Popup::setStyle)
!objc_rename(setAnchorPointX) massif::Popup::setAnchorPoint;

%include "vectorelements/Popup.h"

#endif
