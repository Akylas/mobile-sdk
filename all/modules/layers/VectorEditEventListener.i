#ifndef _VECTOREDITEVENTLISTENER_I
#define _VECTOREDITEVENTLISTENER_I

%module(directors="1") VectorEditEventListener

#ifdef _MASSIF_EDITABLE_SUPPORT

!proxy_imports(massif::VectorEditEventListener, core.ScreenPos, geometry.Geometry, vectorelements.VectorElement, styles.PointStyle, ui.VectorElementDragInfo)

%{
#include "layers/VectorEditEventListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/ScreenPos.i"
%import "geometry/Geometry.i"
%import "vectorelements/VectorElement.i"
%import "styles/PointStyle.i"
%import "ui/VectorElementDragInfo.i"

!enum(massif::VectorElementDragPointStyle::VectorElementDragPointStyle)
!enum(massif::VectorElementDragResult::VectorElementDragResult)
!polymorphic_shared_ptr(massif::VectorEditEventListener, layers.VectorEditEventListener)

%feature("director") massif::VectorEditEventListener;

%include "layers/VectorEditEventListener.h"

#endif

#endif
