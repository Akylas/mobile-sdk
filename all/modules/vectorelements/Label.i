#ifndef _LABEL_I
#define _LABEL_I

%module Label

!proxy_imports(massif::Label, core.MapPos, graphics.Bitmap, geometry.Geometry, geometry.PointGeometry, styles.LabelStyle, vectorelements.Billboard)

%{
#include "vectorelements/Label.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"
%import "styles/LabelStyle.i"
%import "vectorelements/Billboard.i"

!polymorphic_shared_ptr(massif::Label, vectorelements.Label)

!attributestring_polymorphic(massif::Label, styles.LabelStyle, Style, getStyle, setStyle)
%std_exceptions(massif::Label::Label)
%std_exceptions(massif::Label::setStyle)

%include "vectorelements/Label.h"

#endif
