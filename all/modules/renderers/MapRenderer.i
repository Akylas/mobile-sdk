#ifndef _MAPRENDERER_I
#define _MAPRENDERER_I

%module MapRenderer

!proxy_imports(carto::MapRenderer, core.MapPos, core.MapBounds, core.ScreenPos, graphics.ViewState, renderers.MapRendererListener, renderers.RendererCaptureListener, renderers.RedrawRequestListener)

%{
#include "renderers/MapRenderer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "core/MapBounds.i"
%import "core/ScreenPos.i"
%import "graphics/ViewState.i"
%import "renderers/MapRendererListener.i"
%import "renderers/RendererCaptureListener.i"
%import "renderers/RedrawRequestListener.i"

!shared_ptr(carto::MapRenderer, renderers.MapRenderer)

%attributestring(carto::MapRenderer, std::shared_ptr<carto::MapRendererListener>, MapRendererListener, getMapRendererListener, setMapRendererListener)
%std_exceptions(carto::MapRenderer::captureRendering)
%ignore carto::MapRenderer::MapRenderer;
%ignore carto::MapRenderer::init;
%ignore carto::MapRenderer::deinit;
%ignore carto::MapRenderer::getBillboardDrawDatas;
%ignore carto::MapRenderer::getProjectionSurface;
%ignore carto::MapRenderer::getAnimationHandler;
%ignore carto::MapRenderer::getKineticEventHandler;
%ignore carto::MapRenderer::getRedrawRequestListener;
%ignore carto::MapRenderer::setRedrawRequestListener;
%ignore carto::MapRenderer::calculateCameraEvent;
%ignore carto::MapRenderer::moveToFitBounds;
%ignore carto::MapRenderer::screenToWorld;
%ignore carto::MapRenderer::worldToScreen;
%ignore carto::MapRenderer::onSurfaceCreated;
%ignore carto::MapRenderer::onSurfaceChanged;
%ignore carto::MapRenderer::onDrawFrame;
%ignore carto::MapRenderer::onSurfaceDestroyed;
%ignore carto::MapRenderer::finishRendering;
%ignore carto::MapRenderer::clearAndBindScreenFBO;
%ignore carto::MapRenderer::blendAndUnbindScreenFBO;
%ignore carto::MapRenderer::setZBuffering;
%ignore carto::MapRenderer::calculateRayIntersectedElements;
%ignore carto::MapRenderer::billboardsChanged;
%ignore carto::MapRenderer::layerChanged;
%ignore carto::MapRenderer::viewChanged;
%ignore carto::MapRenderer::registerOnChangeListener;
%ignore carto::MapRenderer::unregisterOnChangeListener;
%ignore carto::MapRenderer::addRenderThreadCallback;

!standard_equals(carto::MapRenderer);

%include "renderers/MapRenderer.h"

#endif
