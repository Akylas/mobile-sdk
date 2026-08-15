#ifndef _VECTORTILEFEATURE_I
#define _VECTORTILEFEATURE_I

%module VectorTileFeature

!proxy_imports(massif::VectorTileFeature, core.MapTile, core.Variant, geometry.Feature)

%{
#include "geometry/VectorTileFeature.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/MapTile.i"
%import "geometry/Feature.i"

!polymorphic_shared_ptr(massif::VectorTileFeature, geometry.VectorTileFeature)
!value_type(std::vector<std::shared_ptr<massif::VectorTileFeature> >, geometry.VectorTileFeatureVector)

%attribute(massif::VectorTileFeature, long long, Id, getId)
%attributeval(massif::VectorTileFeature, massif::MapTile, MapTile, getMapTile)
%attributestring(massif::VectorTileFeature, std::string, LayerName, getLayerName)
%attribute(massif::VectorTileFeature, double, Distance, getDistance)
!standard_equals(massif::VectorTileFeature);

%include "geometry/VectorTileFeature.h"

!value_template(std::vector<std::shared_ptr<massif::VectorTileFeature> >, geometry.VectorTileFeatureVector)

#endif
