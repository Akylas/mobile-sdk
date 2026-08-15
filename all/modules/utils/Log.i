#ifndef _LOG_I
#define _LOG_I

%module Log

!proxy_imports(massif::Log, utils.LogEventListener)

%{
#include "utils/Log.h"
%}

%include <std_string.i>
%include <massifswig.i>

%import "utils/LogEventListener.i"

%staticattribute(massif::Log, bool, ShowError, IsShowError, SetShowError)
%staticattribute(massif::Log, bool, ShowWarn, IsShowWarn, SetShowWarn)
%staticattribute(massif::Log, bool, ShowInfo, IsShowInfo, SetShowInfo)
%staticattribute(massif::Log, bool, ShowDebug, IsShowDebug, SetShowDebug)
%staticattributestring(massif::Log, std::string, Tag, GetTag, SetTag)
!staticattributestring_polymorphic(massif::Log, utils.LogEventListener, LogEventListener, GetLogEventListener, SetLogEventListener)
%ignore massif::Log::Fatalf;
%ignore massif::Log::Errorf;
%ignore massif::Log::Warnf;
%ignore massif::Log::Infof;
%ignore massif::Log::Debugf;

%include "utils/Log.h"

#endif
