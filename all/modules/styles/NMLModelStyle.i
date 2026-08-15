#ifndef _NMLMODELSTYLE_I
#define _NMLMODELSTYLE_I

%module NMLModelStyle

!proxy_imports(massif::NMLModelStyle, core.BinaryData, graphics.Color, styles.BillboardStyle)

%{
#include "styles/NMLModelStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "styles/BillboardStyle.i"

!polymorphic_shared_ptr(massif::NMLModelStyle, styles.NMLModelStyle)

%attribute(massif::NMLModelStyle, massif::BillboardOrientation::BillboardOrientation, OrientationMode, getOrientationMode)
%attribute(massif::NMLModelStyle, massif::BillboardScaling::BillboardScaling, ScalingMode, getScalingMode)
%attributestring(massif::NMLModelStyle, std::shared_ptr<massif::BinaryData>, ModelAsset, getModelAsset)
%ignore massif::NMLModelStyle::getSourceModel;
%ignore massif::NMLModelStyle::NMLModelStyle;

%include "styles/NMLModelStyle.h"

#endif
