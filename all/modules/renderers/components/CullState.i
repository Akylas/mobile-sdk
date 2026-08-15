#ifndef _CULLSTATE_I
#define _CULLSTATE_I

%module CullState

!proxy_imports(massif::CullState, core.MapEnvelope, core.MapPos, graphics.ViewState, projections.Projection)

%{
#include "components/Exceptions.h"
#include "renderers/components/CullState.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapEnvelope.i"
%import "core/MapPos.i"
%import "graphics/ViewState.i"
%import "projections/Projection.i"

!shared_ptr(massif::CullState, renderers.components.CullState)

%attributeval(massif::CullState, massif::ViewState, ViewState, getViewState)
%std_exceptions(massif::CullState::getProjectionEnvelope)
%ignore massif::CullState::getEnvelope;
!standard_equals(massif::CullState);

%include "renderers/components/CullState.h"

#endif
