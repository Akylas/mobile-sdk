#ifndef _LABELSTYLE_I
#define _LABELSTYLE_I

%module LabelStyle

!proxy_imports(massif::LabelStyle, graphics.Color, styles.BillboardStyle)

%{
#include "styles/LabelStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"
%import "styles/BillboardStyle.i"

!polymorphic_shared_ptr(massif::LabelStyle, styles.LabelStyle)

%attribute(massif::LabelStyle, massif::BillboardOrientation::BillboardOrientation, OrientationMode, getOrientationMode)
%attribute(massif::LabelStyle, massif::BillboardScaling::BillboardScaling, ScalingMode, getScalingMode)
%attribute(massif::LabelStyle, float, RenderScale, getRenderScale)
%attribute(massif::LabelStyle, bool, Flippable, isFlippable)
%attribute(massif::LabelStyle, float, AnchorPointX, getAnchorPointX)
%attribute(massif::LabelStyle, float, AnchorPointY, getAnchorPointY)
%ignore massif::LabelStyle::LabelStyle;

%include "styles/LabelStyle.h"

#endif
