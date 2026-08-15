#ifndef _PACKAGEMANAGERLISTENER_I
#define _PACKAGEMANAGERLISTENER_I

%module(directors="1") PackageManagerListener

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

!proxy_imports(massif::PackageManagerListener, packagemanager.PackageStatus)

%{
#include "packagemanager/PackageManagerListener.h"	
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>

%import "packagemanager/PackageStatus.i"

!enum(massif::PackageErrorType::PackageErrorType)
!polymorphic_shared_ptr(massif::PackageManagerListener, packagemanager.PackageManagerListener)

%feature("director") massif::PackageManagerListener;

%include "packagemanager/PackageManagerListener.h"

#endif

#endif
