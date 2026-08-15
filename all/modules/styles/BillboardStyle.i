#ifndef _BILLBOARDSTYLE_I
#define _BILLBOARDSTYLE_I

%module BillboardStyle

!proxy_imports(massif::BillboardStyle, graphics.Color, styles.AnimationStyle, styles.Style)

%{
#include "styles/BillboardStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/Style.i"
%import "styles/AnimationStyle.i"

!enum(massif::BillboardOrientation::BillboardOrientation)
!enum(massif::BillboardScaling::BillboardScaling)
!polymorphic_shared_ptr(massif::BillboardStyle, styles.BillboardStyle)

%attribute(massif::BillboardStyle, bool, ScaleWithDPI, isScaleWithDPI)
%attribute(massif::BillboardStyle, int, PlacementPriority, getPlacementPriority)
%attribute(massif::BillboardStyle, bool, CausesOverlap, isCausesOverlap)
%attribute(massif::BillboardStyle, bool, HideIfOverlapped, isHideIfOverlapped)
%attribute(massif::BillboardStyle, float, AttachAnchorPointX, getAttachAnchorPointX)
%attribute(massif::BillboardStyle, float, AttachAnchorPointY, getAttachAnchorPointY)
%attribute(massif::BillboardStyle, float, HorizontalOffset, getHorizontalOffset)
%attribute(massif::BillboardStyle, float, VerticalOffset, getVerticalOffset)
%attributestring(massif::BillboardStyle, std::shared_ptr<massif::AnimationStyle>, AnimationStyle, getAnimationStyle)
%ignore massif::BillboardStyle::BillboardStyle;

%include "styles/BillboardStyle.h"

#endif
