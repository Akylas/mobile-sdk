#ifndef _PMTILESTILEDATASOURCE_I
#define _PMTILESTILEDATASOURCE_I

%module(directors="1") PMTilesTileDataSource

#ifdef _CARTO_OFFLINE_SUPPORT

!proxy_imports(carto::PMTilesTileDataSource, core.MapTile, core.MapBounds, datasources.TileDataSource, datasources.components.TileData)

%{
#include "datasources/PMTilesTileDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "core/MapTile.i"
%import "datasources/TileDataSource.i"
%import "datasources/components/TileData.i"

!polymorphic_shared_ptr(carto::PMTilesTileDataSource, datasources.PMTilesTileDataSource)

%std_io_exceptions(carto::PMTilesTileDataSource::PMTilesTileDataSource)

%feature("director") carto::PMTilesTileDataSource;

%include "datasources/PMTilesTileDataSource.h"

#endif

#endif
