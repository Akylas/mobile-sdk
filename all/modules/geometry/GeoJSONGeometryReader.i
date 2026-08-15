#ifndef _GEOJSONGEOMETRYREADER_I
#define _GEOJSONGEOMETRYREADER_I

%module GeoJSONGeometryReader

!proxy_imports(massif::GeoJSONGeometryReader, geometry.Feature, geometry.FeatureCollection, geometry.Geometry, projections.Projection)

%{
#include "geometry/GeoJSONGeometryReader.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geometry/Feature.i"
%import "geometry/FeatureCollection.i"
%import "geometry/Geometry.i"
%import "projections/Projection.i"

!attributestring_polymorphic(massif::GeoJSONGeometryReader, projections.Projection, TargetProjection, getTargetProjection, setTargetProjection)
%std_exceptions(massif::GeoJSONGeometryReader::readGeometry)
%std_exceptions(massif::GeoJSONGeometryReader::readFeature)
%std_exceptions(massif::GeoJSONGeometryReader::readFeatureCollection)

%include "geometry/GeoJSONGeometryReader.h"

#endif
