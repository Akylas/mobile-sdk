#ifndef _TORQUETILELAYER_I
#define _TORQUETILELAYER_I

%module TorqueTileLayer

!proxy_imports(massif::TorqueTileLayer, layers.VectorTileLayer, datasources.TileDataSource, vectortiles.TorqueTileDecoder)

%{
#include "layers/TorqueTileLayer.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "layers/VectorTileLayer.i"
%import "vectortiles/TorqueTileDecoder.i"

!polymorphic_shared_ptr(massif::TorqueTileLayer, layers.TorqueTileLayer)

%std_exceptions(massif::TorqueTileLayer::TorqueTileLayer)

%include "layers/TorqueTileLayer.h"

#endif
