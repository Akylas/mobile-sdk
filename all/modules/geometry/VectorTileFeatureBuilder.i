#ifndef _VECTORTILEFEATUREBUILDER_I
#define _VECTORTILEFEATUREBUILDER_I

%module VectorTileFeatureBuilder

!proxy_imports(massif::VectorTileFeatureBuilder, core.MapTile, core.Variant, geometry.Feature, geometry.FeatureBuilder, geometry.VectorTileFeature)

%{
#include "geometry/VectorTileFeatureBuilder.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/MapTile.i"
%import "geometry/FeatureBuilder.i"
%import "geometry/VectorTileFeature.i"

!polymorphic_shared_ptr(massif::VectorTileFeatureBuilder, geometry.VectorTileFeatureBuilder)

%attribute(massif::VectorTileFeatureBuilder, long long, Id, getId, setId)
%attributeval(massif::VectorTileFeatureBuilder, massif::MapTile, MapTile, getMapTile, setMapTile)
%attributestring(massif::VectorTileFeatureBuilder, std::string, LayerName, getLayerName, setLayerName)
!standard_equals(massif::VectorTileFeatureBuilder);

%include "geometry/VectorTileFeatureBuilder.h"

#endif
