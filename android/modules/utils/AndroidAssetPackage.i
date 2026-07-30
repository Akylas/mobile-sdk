#ifndef _ANDROIDASSETPACKAGE_I
#define _ANDROIDASSETPACKAGE_I

%module(directors="1") AndroidAssetPackage

!proxy_imports(carto::AndroidAssetPackage, core.BinaryData, core.StringVector, utils.AssetPackage)

%{
#include "utils/AndroidAssetPackage.h"
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

!polymorphic_shared_ptr(carto::AndroidAssetPackage, utils.AndroidAssetPackage)

%attributestring(carto::AndroidAssetPackage, std::string, BasePath, getBasePath)
%attributeval(carto::AndroidAssetPackage, %arg(std::vector<std::string>), LocalAssetNames, getLocalAssetNames)
%std_io_exceptions(carto::AndroidAssetPackage::AndroidAssetPackage)

%include "utils/AndroidAssetPackage.h"

#endif
