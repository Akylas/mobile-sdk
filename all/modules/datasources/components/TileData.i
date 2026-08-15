#ifndef _TILEDATA_I
#define _TILEDATA_I

%module TileData

!proxy_imports(massif::TileData, core.BinaryData)

%{
#include "datasources/components/TileData.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"

!shared_ptr(massif::TileData, datasources.components.TileData)

%attribute(massif::TileData, long long, MaxAge, getMaxAge, setMaxAge)
%attribute(massif::TileData, bool, ReplaceWithParent, isReplaceWithParent, setReplaceWithParent)
%attributestring(massif::TileData, std::shared_ptr<massif::BinaryData>, Data, getData)

%ignore massif::TileData::getMetadata;
%ignore massif::TileData::setMetadata;

!standard_equals(massif::TileData);

%include "datasources/components/TileData.h"

#endif
