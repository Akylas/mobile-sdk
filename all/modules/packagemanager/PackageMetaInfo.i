#ifndef _PACKAGEMETAINFO_I
#define _PACKAGEMETAINFO_I

%module PackageMetaInfo

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageMetaInfo, core.Variant)

%{
#include "packagemanager/PackageMetaInfo.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "core/Variant.i"

!shared_ptr(massif::PackageMetaInfo, packagemanager.PackageMetaInfo)

%attributeval(massif::PackageMetaInfo, massif::Variant, Variant, getVariant)
!standard_equals(massif::PackageMetaInfo);

%include "packagemanager/PackageMetaInfo.h"

#endif

#endif
