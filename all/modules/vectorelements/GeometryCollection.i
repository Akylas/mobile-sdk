#ifndef _GEOMETRYCOLLECTION_I
#define _GEOMETRYCOLLECTION_I

%module GeometryCollection

!proxy_imports(massif::GeometryCollection, geometry.MultiGeometry, styles.GeometryCollectionStyle, vectorelements.VectorElement)

%{
#include "vectorelements/GeometryCollection.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "geometry/MultiGeometry.i"
%import "styles/GeometryCollectionStyle.i"
%import "vectorelements/VectorElement.i"

!polymorphic_shared_ptr(massif::GeometryCollection, vectorelements.GeometryCollection)

%csmethodmodifiers massif::GeometryCollection::Geometry "public new";
!attributestring_polymorphic(massif::GeometryCollection, geometry.MultiGeometry, Geometry, getGeometry, setGeometry)
%attributestring(massif::GeometryCollection, std::shared_ptr<massif::GeometryCollectionStyle>, Style, getStyle, setStyle)
%std_exceptions(massif::GeometryCollection::GeometryCollection)
%std_exceptions(massif::GeometryCollection::setGeometry)
%std_exceptions(massif::GeometryCollection::setStyle)
%ignore massif::GeometryCollection::getDrawData;
%ignore massif::GeometryCollection::setDrawData;

%include "vectorelements/GeometryCollection.h"

#endif
