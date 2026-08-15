#ifndef _BASEMAPVIEW_I
#define _BASEMAPVIEW_I

%module BaseMapView

!proxy_imports(massif::BaseMapView, core.MapPos, core.MapVec, core.MapBounds, core.ScreenPos, core.ScreenBounds, components.Options, components.Layers, renderers.MapRenderer, renderers.RedrawRequestListener, ui.MapEventListener)

%{
#include "ui/BaseMapView.h"
#include "core/MapPos.h"
#include "core/ScreenPos.h"
%}

%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapBounds.i"
%import "core/ScreenPos.i"
%import "core/ScreenBounds.i"
%import "core/MapVec.i"
%import "components/Options.i"
%import "components/Layers.i"
%import "renderers/MapRenderer.i"
%import "renderers/RedrawRequestListener.i"
%import "ui/MapEventListener.i"

%include "ui/BaseMapView.h"

#endif
