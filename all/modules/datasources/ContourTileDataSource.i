#ifndef _CONTOURTILEDATASOURCE_I
#define _CONTOURTILEDATASOURCE_I

%module(directors="1") ContourTileDataSource

!proxy_imports(carto::ContourTileDataSource, core.MapTile, core.MapBounds, datasources.TileDataSource, datasources.components.TileData, rastertiles.ElevationDecoder)

%{
#include "datasources/ContourTileDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "datasources/TileDataSource.i"
%import "datasources/components/TileData.i"
%import "rastertiles/ElevationDecoder.i"

!polymorphic_shared_ptr(carto::ContourTileDataSource, datasources.ContourTileDataSource)

%attributestring(carto::ContourTileDataSource, std::string, LayerName, getLayerName, setLayerName)
%attribute(carto::ContourTileDataSource, float, BaseInterval, getBaseInterval, setBaseInterval)
%attribute(carto::ContourTileDataSource, int, Resolution, getResolution, setResolution)
%attribute(carto::ContourTileDataSource, int, MinVisibleZoom, getMinVisibleZoom, setMinVisibleZoom)
%attribute(carto::ContourTileDataSource, bool, SeamlessEdgesEnabled, isSeamlessEdgesEnabled, setSeamlessEdgesEnabled)
%attribute(carto::ContourTileDataSource, float, SimplifyTolerance, getSimplifyTolerance, setSimplifyTolerance)

%std_exceptions(carto::ContourTileDataSource::ContourTileDataSource)

%feature("director") carto::ContourTileDataSource;

%include "datasources/ContourTileDataSource.h"

#endif
