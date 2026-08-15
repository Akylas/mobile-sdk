#ifndef _UTFGRIDCLICKINFO_I
#define _UTFGRIDCLICKINFO_I

%module UTFGridClickInfo

!proxy_imports(massif::UTFGridClickInfo, core.MapPos, core.Variant, layers.Layer, ui.ClickInfo)

%{
#include "ui/UTFGridClickInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_map.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"
%import "core/MapPos.i"
%import "core/Variant.i"
%import "layers/Layer.i"

!shared_ptr(massif::UTFGridClickInfo, ui.UTFGridClickInfo)

%attribute(massif::UTFGridClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attributeval(massif::UTFGridClickInfo, massif::ClickInfo, ClickInfo, getClickInfo)
%attributeval(massif::UTFGridClickInfo, massif::MapPos, ClickPos, getClickPos)
%attributeval(massif::UTFGridClickInfo, massif::Variant, ElementInfo, getElementInfo)
!attributestring_polymorphic(massif::UTFGridClickInfo, layers.Layer, Layer, getLayer)
%ignore massif::UTFGridClickInfo::UTFGridClickInfo;
!standard_equals(massif::UTFGridClickInfo);

%include "ui/UTFGridClickInfo.h"

#endif
