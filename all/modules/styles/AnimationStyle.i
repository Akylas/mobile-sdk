#ifndef _ANIMATIONSTYLE_I
#define _ANIMATIONSTYLE_I

%module AnimationStyle

!proxy_imports(massif::AnimationStyle)

%{
#include "styles/AnimationStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

!enum(massif::AnimationType::AnimationType)
!polymorphic_shared_ptr(massif::AnimationStyle, styles.AnimationStyle)

%attribute(massif::AnimationStyle, float, RelativeSpeed, getRelativeSpeed)
%attribute(massif::AnimationStyle, float, PhaseInDuration, getPhaseInDuration)
%attribute(massif::AnimationStyle, float, PhaseOutDuration, getPhaseOutDuration)
%attribute(massif::AnimationStyle, massif::AnimationType::AnimationType, FadeAnimationType, getFadeAnimationType)
%attribute(massif::AnimationStyle, massif::AnimationType::AnimationType, SizeAnimationType, getSizeAnimationType)
%ignore massif::AnimationStyle::AnimationStyle;
%ignore massif::AnimationStyle::CalculateTransition;
!standard_equals(massif::AnimationStyle);

%include "styles/AnimationStyle.h"

#endif
