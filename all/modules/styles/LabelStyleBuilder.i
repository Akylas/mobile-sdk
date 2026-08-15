#ifndef _LABELSTYLEBUILDER_I
#define _LABELSTYLEBUILDER_I

%module LabelStyleBuilder

!proxy_imports(massif::LabelStyleBuilder, styles.BillboardStyleBuilder, styles.LabelStyle)

%{
#include "styles/LabelStyleBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/BillboardStyleBuilder.i"
%import "styles/LabelStyle.i"

!polymorphic_shared_ptr(massif::LabelStyleBuilder, styles.LabelStyleBuilder)

%attribute(massif::LabelStyleBuilder, massif::BillboardOrientation::BillboardOrientation, OrientationMode, getOrientationMode, setOrientationMode)
%attribute(massif::LabelStyleBuilder, massif::BillboardScaling::BillboardScaling, ScalingMode, getScalingMode, setScalingMode)
%attribute(massif::LabelStyleBuilder, float, RenderScale, getRenderScale, setRenderScale)
%attribute(massif::LabelStyleBuilder, bool, Flippable, isFlippable, setFlippable)
%attribute(massif::LabelStyleBuilder, float, AnchorPointX, getAnchorPointX, setAnchorPointX)
%attribute(massif::LabelStyleBuilder, float, AnchorPointY, getAnchorPointY, setAnchorPointY)
!objc_rename(setAnchorPointX) massif::LabelStyleBuilder::setAnchorPoint(float, float);

%include "styles/LabelStyleBuilder.h"

#endif
