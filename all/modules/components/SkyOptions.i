#ifndef _SKYOPTIONS_I
#define _SKYOPTIONS_I

%module SkyOptions

!proxy_imports(carto::SkyOptions, graphics.Color)

%{
#include "components/SkyOptions.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "graphics/Color.i"

!shared_ptr(carto::SkyOptions, components.SkyOptions)

%attribute(carto::SkyOptions, bool, Enabled, isEnabled, setEnabled)
%attributeval(carto::SkyOptions, carto::Color, SkyColor, getSkyColor, setSkyColor)
%attributeval(carto::SkyOptions, carto::Color, HorizonColor, getHorizonColor, setHorizonColor)
%attributeval(carto::SkyOptions, carto::Color, GroundColor, getGroundColor, setGroundColor)
%attribute(carto::SkyOptions, float, HorizonBlend, getHorizonBlend, setHorizonBlend)
%attribute(carto::SkyOptions, float, FogBlend, getFogBlend, setFogBlend)
%attribute(carto::SkyOptions, float, FogHorizon, getFogHorizon, setFogHorizon)
%attribute(carto::SkyOptions, bool, SunDiscEnabled, isSunDiscEnabled, setSunDiscEnabled)
%attributestring(carto::SkyOptions, std::string, ShaderSource, getShaderSource, setShaderSource)

%ignore carto::SkyOptions::OnChangeListener;
%ignore carto::SkyOptions::registerOnChangeListener;
%ignore carto::SkyOptions::unregisterOnChangeListener;

%include "components/SkyOptions.h"

#endif
