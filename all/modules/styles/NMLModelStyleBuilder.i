#ifndef _NMLMODELSTYLEBUILDER_I
#define _NMLMODELSTYLEBUILDER_I

%module NMLModelStyleBuilder

!proxy_imports(massif::NMLModelStyleBuilder, core.BinaryData, styles.NMLModelStyle, styles.BillboardStyleBuilder)

%{
#include "styles/NMLModelStyleBuilder.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "styles/NMLModelStyle.i"
%import "styles/BillboardStyleBuilder.i"

!polymorphic_shared_ptr(massif::NMLModelStyleBuilder, styles.NMLModelStyleBuilder)

%attribute(massif::NMLModelStyleBuilder, massif::BillboardOrientation::BillboardOrientation, OrientationMode, getOrientationMode, setOrientationMode)
%attribute(massif::NMLModelStyleBuilder, massif::BillboardScaling::BillboardScaling, ScalingMode, getScalingMode, setScalingMode)
%attributestring(massif::NMLModelStyleBuilder, std::shared_ptr<massif::BinaryData>, ModelAsset, getModelAsset, setModelAsset)
%std_exceptions(massif::NMLModelStyleBuilder::setModelAsset)

%include "styles/NMLModelStyleBuilder.h"

#endif
