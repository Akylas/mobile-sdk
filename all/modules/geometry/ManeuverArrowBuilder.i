#ifndef _MANEUVERARROWBUILDER_I
#define _MANEUVERARROWBUILDER_I

%module ManeuverArrowBuilder

!proxy_imports(carto::ManeuverArrowBuilder, core.MapPos, core.MapPosVector, geometry.FeatureCollection, projections.Projection)

%{
#include "geometry/ManeuverArrowBuilder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "geometry/FeatureCollection.i"
%import "projections/Projection.i"

!shared_ptr(carto::ManeuverArrowBuilder, geometry.ManeuverArrowBuilder)

%attribute(carto::ManeuverArrowBuilder, float, LengthBefore, getLengthBefore, setLengthBefore)
%attribute(carto::ManeuverArrowBuilder, float, LengthAfter, getLengthAfter, setLengthAfter)
%std_exceptions(carto::ManeuverArrowBuilder::buildArrow)
%std_exceptions(carto::ManeuverArrowBuilder::buildArrowAtIndex)

%include "geometry/ManeuverArrowBuilder.h"

#endif
