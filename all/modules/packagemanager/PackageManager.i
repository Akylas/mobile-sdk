#ifndef _PACKAGEMANAGER_I
#define _PACKAGEMANAGER_I

#pragma SWIG nowarn=325
#pragma SWIG nowarn=401

%module PackageManager

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageManager, core.MapPos, core.MapBounds, packagemanager.PackageInfo, packagemanager.PackageMetaInfo, packagemanager.PackageStatus, packagemanager.PackageManagerListener, packagemanager.PackageInfoVector, projections.Projection)

%{
#include "packagemanager/PackageManager.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <stdint.i>
%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapPos.i"
%import "core/MapBounds.i"
%import "packagemanager/PackageInfo.i"
%import "packagemanager/PackageMetaInfo.i"
%import "packagemanager/PackageStatus.i"
%import "packagemanager/PackageManagerListener.i"
%import "projections/Projection.i"

using std::uint64_t;

!polymorphic_shared_ptr(massif::PackageManager, packagemanager.PackageManager)

%attributeval(massif::PackageManager, std::vector<std::shared_ptr<massif::PackageInfo> >, ServerPackages, getServerPackages)
%attributeval(massif::PackageManager, std::vector<std::shared_ptr<massif::PackageInfo> >, LocalPackages, getLocalPackages)
%attribute(massif::PackageManager, int, ServerPackageListAge, getServerPackageListAge)
%attributestring(massif::PackageManager, std::shared_ptr<massif::PackageMetaInfo>, ServerPackageListMetaInfo, getServerPackageListMetaInfo)
!attributestring_polymorphic(massif::PackageManager, packagemanager.PackageManagerListener, PackageManagerListener, getPackageManagerListener, setPackageManagerListener)
%std_io_exceptions(massif::PackageManager::PackageManager)
%ignore massif::PackageManager::PackageManager(const std::string&, const std::string&, const std::string&, const std::string&, const std::shared_ptr<Logger>&);
%ignore massif::PackageManager::OnChangeListener;
%ignore massif::PackageManager::registerOnChangeListener;
%ignore massif::PackageManager::unregisterOnChangeListener;
%ignore massif::PackageManager::getSchema;
%ignore massif::PackageManager::accessLocalPackages;
!standard_equals(massif::PackageManager);

%include "packagemanager/PackageManager.h"

#endif

#endif
