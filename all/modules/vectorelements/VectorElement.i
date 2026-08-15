#ifndef _VECTORELEMENT_I
#define _VECTORELEMENT_I

#pragma SWIG nowarn=401

%module VectorElement

!proxy_imports(massif::VectorElement, core.MapBounds, core.Variant, core.StringVariantMap, geometry.Geometry)

%{
#include "vectorelements/VectorElement.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/MapBounds.i"
%import "core/Variant.i"
%import "geometry/Geometry.i"

!polymorphic_shared_ptr(massif::VectorElement, vectorelements.VectorElement)
!value_type(std::vector<std::shared_ptr<massif::VectorElement> >, vectorelements.VectorElementVector)

%attribute(massif::VectorElement, long long, Id, getId, setId)
%attributeval(massif::VectorElement, %arg(std::map<std::string, massif::Variant>), MetaData, getMetaData, setMetaData)
%attribute(massif::VectorElement, bool, Visible, isVisible, setVisible)
%csmethodmodifiers massif::VectorElement::Bounds "public virtual";
%attributeval(massif::VectorElement, massif::MapBounds, Bounds, getBounds)
%csmethodmodifiers massif::VectorElement::Geometry "public virtual";
!attributestring_polymorphic(massif::VectorElement, geometry.Geometry, Geometry, getGeometry)
%std_exceptions(massif::VectorElement::VectorElement)
!standard_equals(massif::VectorElement);

%include "vectorelements/VectorElement.h"

!value_template(std::vector<std::shared_ptr<massif::VectorElement> >, vectorelements.VectorElementVector);

#endif
