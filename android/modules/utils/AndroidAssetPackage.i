#ifndef _ANDROIDASSETPACKAGE_I
#define _ANDROIDASSETPACKAGE_I

%module(directors="1") AndroidAssetPackage

!proxy_imports(massif::AndroidAssetPackage, core.BinaryData, core.StringVector, utils.AssetPackage)

%{
#include "utils/AndroidAssetPackage.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "core/StringVector.i"
%import "utils/AssetPackage.i"

!polymorphic_shared_ptr(massif::AndroidAssetPackage, utils.AndroidAssetPackage)

%attributestring(massif::AndroidAssetPackage, std::string, BasePath, getBasePath)
%attributeval(massif::AndroidAssetPackage, %arg(std::vector<std::string>), LocalAssetNames, getLocalAssetNames)
%std_io_exceptions(massif::AndroidAssetPackage::AndroidAssetPackage)

%include "utils/AndroidAssetPackage.h"

#endif
