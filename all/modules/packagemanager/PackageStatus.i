#ifndef _PACKAGESTATUS_I
#define _PACKAGESTATUS_I

%module PackageStatus

#ifdef _MASSIF_PACKAGEMANAGER_SUPPORT

%{
#include "packagemanager/PackageStatus.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

!enum(massif::PackageAction::PackageAction)
!shared_ptr(massif::PackageStatus, packagemanager.PackageStatus)

%attribute(massif::PackageStatus, bool, Paused, isPaused)
%attribute(massif::PackageStatus, massif::PackageAction::PackageAction, CurrentAction, getCurrentAction)
%attribute(massif::PackageStatus, float, Progress, getProgress)
!standard_equals(massif::PackageStatus);

%include "packagemanager/PackageStatus.h"

#endif

#endif
