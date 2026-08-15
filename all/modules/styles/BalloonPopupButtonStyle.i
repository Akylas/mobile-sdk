#ifndef _BALLOONPOPUPBUTTONSTYLE_I
#define _BALLOONPOPUPBUTTONSTYLE_I

%module BalloonPopupButtonStyle

!proxy_imports(massif::BalloonPopupButtonStyle, graphics.Color, styles.BalloonPopupStyle, styles.BalloonPopupMargins, styles.Style)

%{
#include "styles/BalloonPopupButtonStyle.h"
#include "styles/BalloonPopupStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/Style.i"
%import "styles/BalloonPopupStyle.i"

!polymorphic_shared_ptr(massif::BalloonPopupButtonStyle, styles.BalloonPopupButtonStyle)

%attributeval(massif::BalloonPopupButtonStyle, massif::Color, BackgroundColor, getBackgroundColor)
%attribute(massif::BalloonPopupButtonStyle, int, ButtonWidth, getButtonWidth)
%attribute(massif::BalloonPopupButtonStyle, int, CornerRadius, getCornerRadius)
%attributeval(massif::BalloonPopupButtonStyle, massif::Color, TextColor, getTextColor)
%attributestring(massif::BalloonPopupButtonStyle, std::string, TextFontName, getTextFontName)
%attribute(massif::BalloonPopupButtonStyle, int, TextFontSize, getTextFontSize)
%attributeval(massif::BalloonPopupButtonStyle, massif::BalloonPopupMargins, TextMargins, getTextMargins)
%attributeval(massif::BalloonPopupButtonStyle, massif::Color, StrokeColor, getStrokeColor)
%attribute(massif::BalloonPopupButtonStyle, int, StrokeWidth, getStrokeWidth)
%ignore massif::BalloonPopupButtonStyle::BalloonPopupButtonStyle;

%include "styles/BalloonPopupButtonStyle.h"

#endif
