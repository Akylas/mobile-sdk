#ifndef _GEOMETRYCOLLECTIONSTYLE_I
#define _GEOMETRYCOLLECTIONSTYLE_I

%module GeometryCollectionStyle

!proxy_imports(massif::GeometryCollectionStyle, styles.Style, styles.PointStyle, styles.LineStyle, styles.PolygonStyle, graphics.Color)

%{
#include "styles/GeometryCollectionStyle.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/Style.i"
%import "styles/PointStyle.i"
%import "styles/LineStyle.i"
%import "styles/PolygonStyle.i"

!polymorphic_shared_ptr(massif::GeometryCollectionStyle, styles.GeometryCollectionStyle)

%attributestring(massif::GeometryCollectionStyle, std::shared_ptr<massif::PointStyle>, PointStyle, getPointStyle)
%attributestring(massif::GeometryCollectionStyle, std::shared_ptr<massif::LineStyle>, LineStyle, getLineStyle)
%attributestring(massif::GeometryCollectionStyle, std::shared_ptr<massif::PolygonStyle>, PolygonStyle, getPolygonStyle)
%ignore massif::GeometryCollectionStyle::GeometryCollectionStyle;

%include "styles/GeometryCollectionStyle.h"

#endif
