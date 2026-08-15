#ifndef _CLICKINFO_I
#define _CLICKINFO_I

%module ClickInfo

%{
#include "ui/ClickInfo.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "ui/ClickInfo.i"

!enum(massif::ClickType::ClickType)
!value_type(massif::ClickInfo, ui.ClickInfo)

%attribute(massif::ClickInfo, massif::ClickType::ClickType, ClickType, getClickType)
%attribute(massif::ClickInfo, float, Duration, getDuration)
!custom_equals(massif::ClickInfo);
!custom_tostring(massif::ClickInfo);

%include "ui/ClickInfo.h"

#endif
