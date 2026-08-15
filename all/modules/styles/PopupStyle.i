#ifndef _POPUPSTYLE_I
#define _POPUPSTYLE_I

%module PopupStyle

!proxy_imports(massif::PopupStyle, graphics.Color, styles.BillboardStyle)

%{
#include "styles/PopupStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"
%import "styles/BillboardStyle.i"

!polymorphic_shared_ptr(massif::PopupStyle, styles.PopupStyle)

%ignore massif::PopupStyle::PopupStyle;

%include "styles/PopupStyle.h"

#endif
