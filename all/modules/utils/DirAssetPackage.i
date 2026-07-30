#ifndef _DIRASSETPACKAGE_I
#define _DIRASSETPACKAGE_I

%module(directors="1") DirAssetPackage

!proxy_imports(carto::DirAssetPackage, core.BinaryData, core.StringVector, utils.AssetPackage)

%{
#include "utils/DirAssetPackage.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "core/BinaryData.i"
%import "core/StringVector.i"
%import "utils/AssetPackage.i"

!polymorphic_shared_ptr(carto::DirAssetPackage, utils.DirAssetPackage)

%attributestring(carto::DirAssetPackage, std::string, DirPath, getDirPath)
%attributeval(carto::DirAssetPackage, %arg(std::vector<std::string>), LocalAssetNames, getLocalAssetNames)
%std_io_exceptions(carto::DirAssetPackage::DirAssetPackage)

%include "utils/DirAssetPackage.h"

#endif
