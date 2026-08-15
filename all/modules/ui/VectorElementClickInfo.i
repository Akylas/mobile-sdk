#ifndef _VECTORELEMENTCLICKINFO_I
#define _VECTORELEMENTCLICKINFO_I

%module VectorElementClickInfo

!proxy_imports(massif::VectorElementClickInfo, core.MapPos, vectorelements.VectorElement, layers.Layer, ui.ClickInfo)

%{
#include "ui/VectorElementClickInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "core/MapPos.i"
%import "layers/Layer.i"
%import "vectorelements/VectorElement.i"

!shared_ptr(massif::VectorElementClickInfo, ui.VectorElementClickInfo)

%attribute(massif::VectorElementClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::VectorElementClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
%attributeval(massif::VectorElementClickInfo, massif::MapPos, ClickPos, getClickPos)
%attributeval(massif::VectorElementClickInfo, massif::MapPos, ElementClickPos, getElementClickPos)
!attributestring_polymorphic(massif::VectorElementClickInfo, vectorelements.VectorElement, VectorElement, getVectorElement)
!attributestring_polymorphic(massif::VectorElementClickInfo, layers.Layer, Layer, getLayer)
%ignore massif::VectorElementClickInfo::VectorElementClickInfo;
!standard_equals(massif::VectorElementClickInfo);

%include "ui/VectorElementClickInfo.h"

#endif
