#ifndef _POLYGON3D_I
#define _POLYGON3D_I

%module Polygon3D

!proxy_imports(massif::Polygon3D, core.MapPosVector, core.MapPosVectorVector, geometry.PolygonGeometry, styles.Polygon3DStyle, vectorelements.VectorElement)

%{
#include "vectorelements/Polygon3D.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "geometry/PolygonGeometry.i"
%import "styles/Polygon3DStyle.i"
%import "vectorelements/VectorElement.i"

!polymorphic_shared_ptr(massif::Polygon3D, vectorelements.Polygon3D)

%attribute(massif::Polygon3D, float, Height, getHeight, setHeight)
%attributestring(massif::Polygon3D, std::shared_ptr<massif::Polygon3DStyle>, Style, getStyle, setStyle)
%csmethodmodifiers massif::Polygon3D::Geometry "public new";
!attributestring_polymorphic(massif::Polygon3D, geometry.PolygonGeometry, Geometry, getGeometry, setGeometry)
%std_exceptions(massif::Polygon3D::Polygon3D)
%std_exceptions(massif::Polygon3D::setGeometry)
%std_exceptions(massif::Polygon3D::setStyle)
%ignore massif::Polygon3D::getDrawData;
%ignore massif::Polygon3D::setDrawData;

%include "vectorelements/Polygon3D.h"

#endif
