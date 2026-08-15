#ifndef _CUSTOMPOPUP_I
#define _CUSTOMPOPUP_I

%module CustomPopup

!proxy_imports(massif::CustomPopup, core.MapPos, core.ScreenPos, graphics.Bitmap, geometry.Geometry, geometry.PointGeometry, styles.PopupStyle, ui.ClickInfo, ui.PopupDrawInfo, ui.PopupClickInfo, vectorelements.Billboard, vectorelements.CustomPopupHandler)
!java_imports(massif::CustomPopup, com.massifmaps.ui.ClickType)

%{
#include "vectorelements/CustomPopup.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/ScreenPos.i"
%import "graphics/Bitmap.i"
%import "styles/PopupStyle.i"
%import "ui/PopupDrawInfo.i"
%import "ui/PopupClickInfo.i"
%import "vectorelements/Billboard.i"
%import "vectorelements/CustomPopupHandler.i"

!polymorphic_shared_ptr(massif::CustomPopup, vectorelements.CustomPopup)

!attributestring_polymorphic(massif::CustomPopup, vectorelements.CustomPopupHandler, PopupHandler, getPopupHandler)
%std_exceptions(massif::CustomPopup::CustomPopup)

%include "vectorelements/CustomPopup.h"

#endif
