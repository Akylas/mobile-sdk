#ifndef _ASSETPACKAGE_I
#define _ASSETPACKAGE_I

%module(directors="1") AssetPackage

!proxy_imports(massif::AssetPackage, core.BinaryData, core.StringVector)

%{
#include "utils/AssetPackage.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "core/StringVector.i"

!polymorphic_shared_ptr(massif::AssetPackage, utils.AssetPackage)

%attributeval(massif::AssetPackage, %arg(std::vector<std::string>), AssetNames, getAssetNames)
!standard_equals(massif::AssetPackage);

%feature("director") massif::AssetPackage;

%include "utils/AssetPackage.h"

#endif
