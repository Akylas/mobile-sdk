#ifndef _VIEWSTATE_I
#define _VIEWSTATE_I

#pragma SWIG nowarn=325

%module ViewState

!proxy_imports(massif::ViewState, core.MapPos, core.ScreenPos, core.MapVec, components.Options, projections.Projection)

%{
#include "graphics/ViewState.h"	
#include <cglib/mat.h>
%}

%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapVec.i"
%import "core/ScreenPos.i"
%import "components/Options.i"

!value_type(massif::ViewState, graphics.ViewState)

%ignore massif::ViewState::getCameraPos;
%ignore massif::ViewState::getFocusPos;
%ignore massif::ViewState::getUpVec;
%ignore massif::ViewState::getFrustum;

%attribute(massif::ViewState, float, Rotation, getRotation)
%attribute(massif::ViewState, float, Zoom, getZoom)
%attribute(massif::ViewState, float, Tilt, getTilt)
%attribute(massif::ViewState, float, Zoom0Distance, getZoom0Distance)
%attribute(massif::ViewState, int, FOVY, getFOVY)
%attribute(massif::ViewState, float, Near, getNear)
%attribute(massif::ViewState, float, Far, getFar)
%attribute(massif::ViewState, bool, CameraChanged, isCameraChanged)
%attribute(massif::ViewState, int, Width, getWidth)
%attribute(massif::ViewState, int, Height, getHeight)
%attribute(massif::ViewState, int, ScreenWidth, getScreenWidth)
%attribute(massif::ViewState, int, ScreenHeight, getScreenHeight)
%attribute(massif::ViewState, float, DPI, getDPI)
%attribute(massif::ViewState, float, DPToPX, getDPToPX)
%attribute(massif::ViewState, float, UnitToDPCoef, getUnitToDPCoef)
%attribute(massif::ViewState, float, UnitToPXCoef, getUnitToPXCoef)
%attribute(massif::ViewState, float, AspectRatio, getAspectRatio)
%ignore massif::ViewState::ViewState;
%ignore massif::ViewState::RotationState;
%ignore massif::ViewState::getMinZoom;
%ignore massif::ViewState::getHalfWidth;
%ignore massif::ViewState::getHalfHeight;
%ignore massif::ViewState::getTanHalfFOVY;
%ignore massif::ViewState::getHalfFOVY;
%ignore massif::ViewState::getNormalizedResolution;
%ignore massif::ViewState::getTanHalfFOVX;
%ignore massif::ViewState::getCosHalfFOVY;
%ignore massif::ViewState::getCosHalfFOVXY;
%ignore massif::ViewState::setCameraPos;
%ignore massif::ViewState::setFocusPos;
%ignore massif::ViewState::setUpVec;
%ignore massif::ViewState::setRotation;
%ignore massif::ViewState::setTilt;
%ignore massif::ViewState::setZoom;
%ignore massif::ViewState::cameraChanged;
%ignore massif::ViewState::get2PowZoom;
%ignore massif::ViewState::getRotationState;
%ignore massif::ViewState::getProjectionSurface;
%ignore massif::ViewState::getProjectionMat;
%ignore massif::ViewState::getModelviewMat;
%ignore massif::ViewState::getModelviewProjectionMat;
%ignore massif::ViewState::getRTELocalMat;
%ignore massif::ViewState::getRTEModelviewMat;
%ignore massif::ViewState::getRTEModelviewProjectionMat;
%ignore massif::ViewState::getRTESkyProjectionMat;
%ignore massif::ViewState::setScreenSize;
%ignore massif::ViewState::setTerrainCameraReference; // renderer plumbing, published every frame
%ignore massif::ViewState::clampZoom;
%ignore massif::ViewState::clampFocusPos;
%ignore massif::ViewState::getFocusPosNormal;
%ignore massif::ViewState::isSkyVisible;
%ignore massif::ViewState::calculateViewState;
%ignore massif::ViewState::worldToScreen;
%ignore massif::ViewState::screenToWorld;
%ignore massif::ViewState::estimateWorldPixelMeasure;
%ignore massif::ViewState::getHorizontalLayerOffsetDir;
%ignore massif::ViewState::setHorizontalLayerOffsetDir;
!standard_equals(massif::ViewState);

%include "graphics/ViewState.h"

#endif
