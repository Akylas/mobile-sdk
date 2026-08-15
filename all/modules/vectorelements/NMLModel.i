#ifndef _NMLMODEL_I
#define _NMLMODEL_I

%module NMLModel

!proxy_imports(massif::NMLModel, core.BinaryData, core.MapBounds, core.MapPos, core.MapVec, geometry.Geometry, styles.NMLModelStyle, vectorelements.Billboard)

%{
#include "vectorelements/NMLModel.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "core/MapPos.i"
%import "geometry/Geometry.i"
%import "styles/NMLModelStyle.i"
%import "vectorelements/Billboard.i"

!polymorphic_shared_ptr(massif::NMLModel, vectorelements.NMLModel)

%attribute(massif::NMLModel, float, RotationAngle, getRotationAngle, setRotationAngle)
%attributeval(massif::NMLModel, massif::MapVec, RotationAxis, getRotationAxis, setRotationAxis)
%attributestring(massif::NMLModel, std::shared_ptr<massif::NMLModelStyle>, Style, getStyle, setStyle)
%attribute(massif::NMLModel, float, Scale, getScale, setScale)
%std_exceptions(massif::NMLModel::NMLModel)
%std_exceptions(massif::NMLModel::setGeometry)
%std_exceptions(massif::NMLModel::setStyle)

%include "vectorelements/NMLModel.h"

#endif
