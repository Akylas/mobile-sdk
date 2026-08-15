#ifndef _VECTORELEMENTDRAGINFO_I
#define _VECTORELEMENTDRAGINFO_I

%module VectorElementDragInfo

#ifdef _MASSIF_EDITABLE_SUPPORT

!proxy_imports(massif::VectorElementDragInfo, core.MapPos, core.ScreenPos, vectorelements.VectorElement)

%{
#include "ui/VectorElementDragInfo.h"
#include <memory>
%}

%import <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/ScreenPos.i"
%import "vectorelements/VectorElement.i"

!enum(massif::VectorElementDragMode::VectorElementDragMode)
!shared_ptr(massif::VectorElementDragInfo, ui.VectorElementDragInfo)

%attributeval(massif::VectorElementDragInfo, massif::ScreenPos, ScreenPos, getScreenPos)
%attributeval(massif::VectorElementDragInfo, massif::MapPos, MapPos, getMapPos)
%attribute(massif::VectorElementDragInfo, massif::VectorElementDragMode::VectorElementDragMode, VectorElementDragMode, getDragMode)
!attributestring_polymorphic(massif::VectorElementDragInfo, vectorelements.VectorElement, VectorElement, getVectorElement)
%ignore massif::VectorElementDragInfo::VectorElementDragInfo;
!standard_equals(massif::VectorElementDragInfo);

%include "ui/VectorElementDragInfo.h"

#endif

#endif
