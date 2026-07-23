#ifndef _COMPOSITEVECTORTILELAYER_I
#define _COMPOSITEVECTORTILELAYER_I

%module CompositeVectorTileLayer

!proxy_imports(carto::CompositeVectorTileLayer, datasources.TileDataSource, layers.VectorTileLayer, vectortiles.VectorTileDecoder, rastertiles.ElevationDecoder)

%{
#include "layers/CompositeVectorTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_vector.i>
%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "layers/VectorTileLayer.i"
%import "datasources/TileDataSource.i"
%import "vectortiles/VectorTileDecoder.i"
%import "rastertiles/ElevationDecoder.i"

!enum(carto::CompositeSourceType::CompositeSourceType)
!polymorphic_shared_ptr(carto::CompositeVectorTileLayer, layers.CompositeVectorTileLayer)

%attribute(carto::CompositeVectorTileLayer, bool, SinglePassRenderingEnabled, isSinglePassRenderingEnabled, setSinglePassRenderingEnabled)
%std_exceptions(carto::CompositeVectorTileLayer::CompositeVectorTileLayer)
%std_exceptions(carto::CompositeVectorTileLayer::addExternalDataSource)
%std_exceptions(carto::CompositeVectorTileLayer::addVectorDataSource)

%include "layers/CompositeVectorTileLayer.h"

#endif
