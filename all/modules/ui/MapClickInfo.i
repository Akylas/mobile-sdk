#ifndef _MAPCLICKINFO_I
#define _MAPCLICKINFO_I

%module MapClickInfo

!proxy_imports(massif::MapClickInfo, core.MapPos, ui.ClickInfo)

%{
#include "ui/MapClickInfo.h"
#include <memory>
%}

%import <std_shared_ptr.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "core/MapPos.i"

!shared_ptr(massif::MapClickInfo, ui.MapClickInfo)

%attribute(massif::MapClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::MapClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
%attributeval(massif::MapClickInfo, massif::MapPos, ClickPos, getClickPos)
%ignore massif::MapClickInfo::MapClickInfo;
!standard_equals(massif::MapClickInfo);

%include "ui/MapClickInfo.h"

#endif
