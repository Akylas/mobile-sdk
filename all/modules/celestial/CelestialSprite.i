#ifndef _CELESTIALSPRITE_I
#define _CELESTIALSPRITE_I

%module(directors="1") CelestialSprite

!proxy_imports(carto::CelestialSprite, celestial.CelestialObject, graphics.Bitmap)

%{
#include "celestial/CelestialSprite.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "celestial/CelestialObject.i"
%import "graphics/Bitmap.i"

!polymorphic_shared_ptr(carto::CelestialSprite, celestial.CelestialSprite)

%attribute(carto::CelestialSprite, float, AngularSize, getAngularSize, setAngularSize)
%attribute(carto::CelestialSprite, float, ScreenSize, getScreenSize, setScreenSize)
!attributestring_polymorphic(carto::CelestialSprite, graphics.Bitmap, Bitmap, getBitmap, setBitmap)
%attribute(carto::CelestialSprite, float, Softness, getSoftness, setSoftness)
%attribute(carto::CelestialSprite, float, ClickRadius, getClickRadius, setClickRadius)
%std_exceptions(carto::CelestialSprite::CelestialSprite)

%include "celestial/CelestialSprite.h"

#endif
