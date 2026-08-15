#ifndef _BILLBOARDSTYLEBUILDER_I
#define _BILLBOARDSTYLEBUILDER_I

%module BillboardStyleBuilder

!proxy_imports(massif::BillboardStyleBuilder, styles.AnimationStyle, styles.StyleBuilder)

%{
#include "styles/BillboardStyleBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/BillboardStyle.i"
%import "styles/StyleBuilder.i"
%import "styles/AnimationStyle.i"

!polymorphic_shared_ptr(massif::BillboardStyleBuilder, styles.BillboardStyleBuilder)

%attribute(massif::BillboardStyleBuilder, bool, ScaleWithDPI, isScaleWithDPI, setScaleWithDPI)
%attribute(massif::BillboardStyleBuilder, int, PlacementPriority, getPlacementPriority, setPlacementPriority)
%attribute(massif::BillboardStyleBuilder, bool, CausesOverlap, isCausesOverlap, setCausesOverlap)
%attribute(massif::BillboardStyleBuilder, bool, HideIfOverlapped, isHideIfOverlapped, setHideIfOverlapped)
%attribute(massif::BillboardStyleBuilder, float, AttachAnchorPointX, getAttachAnchorPointX, setAttachAnchorPointX)
%attribute(massif::BillboardStyleBuilder, float, AttachAnchorPointY, getAttachAnchorPointY, setAttachAnchorPointY)
%attribute(massif::BillboardStyleBuilder, float, HorizontalOffset, getHorizontalOffset, setHorizontalOffset)
%attribute(massif::BillboardStyleBuilder, float, VerticalOffset, getVerticalOffset, setVerticalOffset)
%attributestring(massif::BillboardStyleBuilder, std::shared_ptr<massif::AnimationStyle>, AnimationStyle, getAnimationStyle, setAnimationStyle)
!objc_rename(setAttachAnchorPointX) massif::BillboardStyleBuilder::setAttachAnchorPoint(float, float);

%include "styles/BillboardStyleBuilder.h"

#endif
