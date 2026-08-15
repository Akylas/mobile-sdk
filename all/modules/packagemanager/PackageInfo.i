#ifndef _PACKAGEINFO_I
#define _PACKAGEINFO_I

%module PackageInfo

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageInfo, packagemanager.PackageMetaInfo, packagemanager.PackageTileMask, core.StringVector)

%{
#include "packagemanager/PackageInfo.h"
#include <memory>
%}

%include <stdint.i>
%include <std_shared_ptr.i>
%include <std_vector.i>
%include <massifswig.i>

%import "core/StringVector.i"
%import "packagemanager/PackageMetaInfo.i"
%import "packagemanager/PackageTileMask.i"

using std::uint64_t;

!enum(massif::PackageType::PackageType)
!shared_ptr(massif::PackageInfo, packagemanager.PackageInfo)
!value_type(std::vector<std::shared_ptr<massif::PackageInfo> >, packagemanager.PackageInfoVector)

%attributestring(massif::PackageInfo, std::string, PackageId, getPackageId)
%attribute(massif::PackageInfo, massif::PackageType::PackageType, PackageType, getPackageType)
%attributestring(massif::PackageInfo, std::string, Name, getName)
%attribute(massif::PackageInfo, int, Version, getVersion)
%attribute(massif::PackageInfo, std::uint64_t, Size, getSize)
%attributestring(massif::PackageInfo, std::shared_ptr<massif::PackageMetaInfo>, MetaInfo, getMetaInfo)
%attributestring(massif::PackageInfo, std::shared_ptr<massif::PackageTileMask>, TileMask, getTileMask)
%ignore massif::PackageInfo::getServerURL;
!standard_equals(massif::PackageInfo);

%include "packagemanager/PackageInfo.h"

!value_template(std::vector<std::shared_ptr<massif::PackageInfo> >, packagemanager.PackageInfoVector)

#endif

#endif
