#ifndef _GEOMETRYCOLLECTIONSTYLEBUILDER_I
#define _GEOMETRYCOLLECTIONSTYLEBUILDER_I

%module GeometryCollectionStyleBuilder

!proxy_imports(massif::GeometryCollectionStyleBuilder, styles.GeometryCollectionStyle, styles.PointStyle, styles.LineStyle, styles.PolygonStyle, styles.StyleBuilder)

%{
#include "styles/GeometryCollectionStyleBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "styles/GeometryCollectionStyle.i"
%import "styles/PointStyle.i"
%import "styles/LineStyle.i"
%import "styles/PolygonStyle.i"
%import "styles/StyleBuilder.i"

!polymorphic_shared_ptr(massif::GeometryCollectionStyleBuilder, styles.GeometryCollectionStyleBuilder)

%attributestring(massif::GeometryCollectionStyleBuilder, std::shared_ptr<massif::PointStyle>, PointStyle, getPointStyle, setPointStyle)
%attributestring(massif::GeometryCollectionStyleBuilder, std::shared_ptr<massif::LineStyle>, LineStyle, getLineStyle, setLineStyle)
%attributestring(massif::GeometryCollectionStyleBuilder, std::shared_ptr<massif::PolygonStyle>, PolygonStyle, getPolygonStyle, setPolygonStyle)

%include "styles/GeometryCollectionStyleBuilder.h"

#endif
