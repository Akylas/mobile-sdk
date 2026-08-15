#ifndef _POLYGON3DSTYLEBUILDER_I
#define _POLYGON3DSTYLEBUILDER_I

%module Polygon3DStyleBuilder

!proxy_imports(massif::Polygon3DStyleBuilder, graphics.Color, styles.Polygon3DStyle, styles.StyleBuilder)

%{
#include "styles/Polygon3DStyleBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"
%import "styles/Polygon3DStyle.i"
%import "styles/StyleBuilder.i"

!polymorphic_shared_ptr(massif::Polygon3DStyleBuilder, styles.Polygon3DStyleBuilder)

%attributeval(massif::Polygon3DStyleBuilder, massif::Color, SideColor, getSideColor, setSideColor)

%include "styles/Polygon3DStyleBuilder.h"

#endif
