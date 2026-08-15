#ifndef _POSTPROCESSEFFECT_I
#define _POSTPROCESSEFFECT_I

%module PostProcessEffect

!proxy_imports(massif::PostProcessEffect, graphics.Color)

%{
#include "renderers/PostProcessEffect.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"

!shared_ptr(massif::PostProcessEffect, renderers.PostProcessEffect)

%attributestring(massif::PostProcessEffect, std::string, Name, getName)
%attributestring(massif::PostProcessEffect, std::string, FragmentShader, getFragmentShader)
%attribute(massif::PostProcessEffect, bool, TerrainDepthRequired, isTerrainDepthRequired, setTerrainDepthRequired)
%std_exceptions(massif::PostProcessEffect::PostProcessEffect)

%ignore massif::PostProcessEffect::getFloatParameters;
%ignore massif::PostProcessEffect::getColorParameters;

!standard_equals(massif::PostProcessEffect);

%include "renderers/PostProcessEffect.h"

#endif
