#ifndef _LIGHTOPTIONS_I
#define _LIGHTOPTIONS_I

%module LightOptions

!proxy_imports(carto::LightOptions, graphics.Color)

%{
#include "components/LightOptions.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "graphics/Color.i"

!shared_ptr(carto::LightOptions, components.LightOptions)

%attribute(carto::LightOptions, float, SunAzimuth, getSunAzimuth, setSunAzimuth)
%attribute(carto::LightOptions, float, SunAltitude, getSunAltitude, setSunAltitude)
%attributeval(carto::LightOptions, carto::Color, SunColor, getSunColor, setSunColor)
%attribute(carto::LightOptions, float, SunIntensity, getSunIntensity, setSunIntensity)
%attribute(carto::LightOptions, float, AmbientIntensity, getAmbientIntensity, setAmbientIntensity)

%ignore carto::LightOptions::OnChangeListener;
%ignore carto::LightOptions::registerOnChangeListener;
%ignore carto::LightOptions::unregisterOnChangeListener;
%ignore carto::LightOptions::getSunDirection;

%include "components/LightOptions.h"

#endif
