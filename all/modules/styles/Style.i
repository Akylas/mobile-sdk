#ifndef _STYLE_I
#define _STYLE_I

%module Style

!proxy_imports(massif::Style, graphics.Color)

%{
#include "styles/Style.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"

!polymorphic_shared_ptr(massif::Style, styles.Style)

%attributeval(massif::Style, massif::Color, Color, getColor)
!standard_equals(massif::Style);

%include "styles/Style.h"

#endif
