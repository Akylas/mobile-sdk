#ifndef _POSTPROCESSEFFECT_I
#define _POSTPROCESSEFFECT_I

%module PostProcessEffect

!proxy_imports(carto::PostProcessEffect)

%{
#include "renderers/PostProcessEffect.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <cartoswig.i>

!shared_ptr(carto::PostProcessEffect, renderers.PostProcessEffect)

%attributestring(carto::PostProcessEffect, std::string, Name, getName)
%attributestring(carto::PostProcessEffect, std::string, FragmentShader, getFragmentShader)
%attribute(carto::PostProcessEffect, bool, TerrainDepthRequired, isTerrainDepthRequired, setTerrainDepthRequired)
%std_exceptions(carto::PostProcessEffect::PostProcessEffect)

%ignore carto::PostProcessEffect::getFloatParameters;

!standard_equals(carto::PostProcessEffect);

%include "renderers/PostProcessEffect.h"

#endif
