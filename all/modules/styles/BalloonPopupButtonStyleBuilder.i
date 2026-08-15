#ifndef _BALLOONPOPUPBUTTONSTYLEBUILDER_I
#define _BALLOONPOPUPBUTTONSTYLEBUILDER_I

%module BalloonPopupButtonStyleBuilder

!proxy_imports(massif::BalloonPopupButtonStyleBuilder, graphics.Color, styles.StyleBuilder, styles.BalloonPopupStyle, styles.BalloonPopupButtonStyle, styles.BalloonPopupMargins)

%{
#include "styles/BalloonPopupButtonStyleBuilder.h"
#include "styles/BalloonPopupStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "graphics/Color.i"
%import "styles/StyleBuilder.i"
%import "styles/BalloonPopupButtonStyle.i"
%import "styles/BalloonPopupStyle.i"

!polymorphic_shared_ptr(massif::BalloonPopupButtonStyleBuilder, styles.BalloonPopupButtonStyleBuilder)

%attribute(massif::BalloonPopupButtonStyleBuilder, int, ButtonWidth, getButtonWidth, setButtonWidth)
%attribute(massif::BalloonPopupButtonStyleBuilder, int, CornerRadius, getCornerRadius, setCornerRadius)
%attributeval(massif::BalloonPopupButtonStyleBuilder, massif::Color, TextColor, getTextColor, setTextColor)
%attributestring(massif::BalloonPopupButtonStyleBuilder, std::string, TextFontName, getTextFontName, setTextFontName)
%attribute(massif::BalloonPopupButtonStyleBuilder, int, TextFontSize, getTextFontSize, setTextFontSize)
%attributeval(massif::BalloonPopupButtonStyleBuilder, massif::BalloonPopupMargins, TextMargins, getTextMargins, setTextMargins)
%attributeval(massif::BalloonPopupButtonStyleBuilder, massif::Color, StrokeColor, getStrokeColor, setStrokeColor)
%attribute(massif::BalloonPopupButtonStyleBuilder, int, StrokeWidth, getStrokeWidth, setStrokeWidth)
%csmethodmodifiers massif::BalloonPopupButtonStyleBuilder::buildStyle "public new";

%include "styles/BalloonPopupButtonStyleBuilder.h"

#endif
