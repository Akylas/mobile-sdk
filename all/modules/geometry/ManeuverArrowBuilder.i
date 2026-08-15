#ifndef _MANEUVERARROWBUILDER_I
#define _MANEUVERARROWBUILDER_I

%module ManeuverArrowBuilder

!proxy_imports(massif::ManeuverArrowBuilder, core.MapPos, core.MapPosVector, geometry.FeatureCollection, projections.Projection)

%{
#include "geometry/ManeuverArrowBuilder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "geometry/FeatureCollection.i"
%import "projections/Projection.i"

!shared_ptr(massif::ManeuverArrowBuilder, geometry.ManeuverArrowBuilder)

%attribute(massif::ManeuverArrowBuilder, float, LengthBefore, getLengthBefore, setLengthBefore)
%attribute(massif::ManeuverArrowBuilder, float, LengthAfter, getLengthAfter, setLengthAfter)
%std_exceptions(massif::ManeuverArrowBuilder::buildArrow)
%std_exceptions(massif::ManeuverArrowBuilder::buildArrowAtIndex)

%include "geometry/ManeuverArrowBuilder.h"

#endif
