#ifndef _PACKAGEMANAGERTILEDATASOURCE_I
#define _PACKAGEMANAGERTILEDATASOURCE_I

%module(directors="1") PackageManagerTileDataSource

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageManagerTileDataSource, core.MapTile, core.MapBounds, core.StringMap, datasources.TileDataSource, datasources.components.TileData, packagemanager.PackageManager)

%{
#include "datasources/PackageManagerTileDataSource.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "datasources/TileDataSource.i"
%import "packagemanager/PackageManager.i"

!polymorphic_shared_ptr(massif::PackageManagerTileDataSource, datasources.PackageManagerTileDataSource)

!attributestring_polymorphic(massif::PackageManagerTileDataSource, packagemanager.PackageManager, PackageManager, getPackageManager)
%std_exceptions(massif::PackageManagerTileDataSource::PackageManagerTileDataSource)

%feature("director") massif::PackageManagerTileDataSource;

%include "datasources/PackageManagerTileDataSource.h"

#endif

#endif
