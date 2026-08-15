#ifndef _ANIMATIONSTYLEBUILDER_I
#define _ANIMATIONSTYLEBUILDER_I

%module AnimationStyleBuilder

!proxy_imports(massif::AnimationStyleBuilder, styles.AnimationStyle)

%{
#include "styles/AnimationStyleBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "styles/AnimationStyle.i"

!polymorphic_shared_ptr(massif::AnimationStyleBuilder, styles.AnimationStyleBuilder)

%attribute(massif::AnimationStyleBuilder, float, RelativeSpeed, getRelativeSpeed, setRelativeSpeed)
%attribute(massif::AnimationStyleBuilder, float, PhaseInDuration, getPhaseInDuration, setPhaseInDuration)
%attribute(massif::AnimationStyleBuilder, float, PhaseOutDuration, getPhaseOutDuration, setPhaseOutDuration)
%attribute(massif::AnimationStyleBuilder, massif::AnimationType::AnimationType, FadeAnimationType, getFadeAnimationType, setFadeAnimationType)
%attribute(massif::AnimationStyleBuilder, massif::AnimationType::AnimationType, SizeAnimationType, getSizeAnimationType, setSizeAnimationType)

%include "styles/AnimationStyleBuilder.h"

#endif
