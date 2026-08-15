#ifndef _POLYGON3DSTYLE_I
#define _POLYGON3DSTYLE_I

%module Polygon3DStyle

!proxy_imports(massif::Polygon3DStyle, graphics.Color, styles.Style)

%{
#include "styles/Polygon3DStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"
%import "styles/Style.i"

!polymorphic_shared_ptr(massif::Polygon3DStyle, styles.Polygon3DStyle)

%attributeval(massif::Polygon3DStyle, massif::Color, SideColor, getSideColor)
%ignore massif::Polygon3DStyle::Polygon3DStyle;

%include "styles/Polygon3DStyle.h"

#endif
