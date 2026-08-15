#ifndef _MAPINTERACTIONINFO_I
#define _MAPINTERACTIONINFO_I

%module MapInteractionInfo

!proxy_imports(massif::MapInteractionInfo)

%{
#include "ui/MapInteractionInfo.h"
#include <memory>
%}

%import <std_shared_ptr.i>
%include <massifswig.i>

!shared_ptr(massif::MapInteractionInfo, ui.MapInteractionInfo)

%attribute(massif::MapInteractionInfo, bool, PanAction, isPanAction)
%attribute(massif::MapInteractionInfo, bool, ZoomAction, isZoomAction)
%attribute(massif::MapInteractionInfo, bool, RotateAction, isRotateAction)
%attribute(massif::MapInteractionInfo, bool, TiltAction, isTiltAction)
%attribute(massif::MapInteractionInfo, bool, AnimationStarted, isAnimationStarted)
%ignore massif::MapInteractionInfo::MapInteractionInfo;
!standard_equals(massif::MapInteractionInfo);

%include "ui/MapInteractionInfo.h"

#endif
