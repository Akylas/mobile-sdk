#ifndef _STYLEBUILDER_I
#define _STYLEBUILDER_I

%module StyleBuilder

!proxy_imports(massif::StyleBuilder, graphics.Color)

%{
#include "styles/StyleBuilder.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"

!polymorphic_shared_ptr(massif::StyleBuilder, styles.StyleBuilder)

%attributeval(massif::StyleBuilder, massif::Color, Color, getColor, setColor)
!standard_equals(massif::StyleBuilder);

%include "styles/StyleBuilder.h"

#endif
