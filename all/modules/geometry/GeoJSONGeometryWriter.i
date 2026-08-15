#ifndef _GEOJSONGEOMETRYWRITER_I
#define _GEOJSONGEOMETRYWRITER_I

%module GeoJSONGeometryWriter

!proxy_imports(massif::GeoJSONGeometryWriter, geometry.Feature, geometry.FeatureCollection, geometry.Geometry, projections.Projection)

%{
#include "geometry/GeoJSONGeometryWriter.h"
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

!attributestring_polymorphic(massif::GeoJSONGeometryWriter, projections.Projection, SourceProjection, getSourceProjection, setSourceProjection)
%attribute(massif::GeoJSONGeometryWriter, bool, Z, getZ, setZ)
%std_exceptions(massif::GeoJSONGeometryWriter::writeGeometry)
%std_exceptions(massif::GeoJSONGeometryWriter::writeFeature)
%std_exceptions(massif::GeoJSONGeometryWriter::writeFeatureCollection)

%include "geometry/GeoJSONGeometryWriter.h"

#endif
