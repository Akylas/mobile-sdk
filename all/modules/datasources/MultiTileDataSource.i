#ifndef _LOCALPACKAGEMANAGERTILEDATASOURCE_I
#define _LOCALPACKAGEMANAGERTILEDATASOURCE_I

%module(directors="1") MultiTileDataSource

!proxy_imports(carto::MultiTileDataSource, core.MapTile, core.MapBounds, core.StringMap, datasources.TileDataSource, datasources.MBTilesTileDataSource, datasources.components.TileData)

%{
#include "datasources/MultiTileDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <cartoswig.i>

%import "core/MapTile.i"
%import "core/StringMap.i"
%import "datasources/TileDataSource.i"
%import "datasources/MBTilesTileDataSource.i"
%import "datasources/components/TileData.i"

!polymorphic_shared_ptr(carto::MultiTileDataSource, datasources.MultiTileDataSource)

%std_exceptions(carto::MultiTileDataSource::MultiTileDataSource)
%std_exceptions(carto::LocalVectorDataSource::add)
// %std_exceptions(carto::LocalVectorDataSource::addAll)
%std_exceptions(carto::LocalVectorDataSource::remove)
// %std_exceptions(carto::LocalVectorDataSource::removeAll)

%ignore carto::LocalVectorDataSource::addAll;
%ignore carto::LocalVectorDataSource::removeAll;

%feature("director") carto::MultiTileDataSource;

%include "datasources/MultiTileDataSource.h"

#endif