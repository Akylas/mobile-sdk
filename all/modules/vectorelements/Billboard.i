#ifndef _BILLBOARD_I
#define _BILLBOARD_I

%module Billboard

!proxy_imports(massif::Billboard, core.MapBounds, core.MapPos, geometry.Geometry, styles.BillboardStyle, vectorelements.VectorElement)

%{
#include "vectorelements/Billboard.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "geometry/Geometry.i"
%import "styles/BillboardStyle.i"
%import "vectorelements/VectorElement.i"

!polymorphic_shared_ptr(massif::Billboard, vectorelements.Billboard)

%attribute(massif::Billboard, float, Rotation, getRotation, setRotation)
%csmethodmodifiers massif::Billboard::Bounds "public override";
%attributeval(massif::Billboard, massif::MapBounds, Bounds, getBounds)
!attributestring_polymorphic(massif::Billboard, geometry.Geometry, RootGeometry, getRootGeometry)
%csmethodmodifiers massif::Billboard::Geometry "public new";
!attributestring_polymorphic(massif::Billboard, geometry.Geometry, Geometry, getGeometry, setGeometry)
%attributestring(massif::Billboard, std::shared_ptr<massif::Billboard>, BaseBillboard, getBaseBillboard, setBaseBillboard)
%std_exceptions(massif::Billboard::Billboard)
%std_exceptions(massif::Billboard::setBaseBillboard)
%std_exceptions(massif::Billboard::setGeometry)
%ignore massif::Billboard::getDrawData;
%ignore massif::Billboard::setDrawData;

%include "vectorelements/Billboard.h"

#endif
