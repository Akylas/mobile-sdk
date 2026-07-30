#ifndef _ASSETUTILS_I
#define _ASSETUTILS_I

%module AssetUtils

!proxy_imports(carto::AssetUtils, core.BinaryData, core.StringVector)

%{
#include "utils/AssetUtils.h"	
%}

%include <std_string.i>
%include <std_vector.i>
%include <cartoswig.i>

%import "core/BinaryData.i"
%import "core/StringVector.i"

%typemap(in) jobject androidAssetManager %{
    $1 = (jobject)$input;
%}

%typemap(javain) jobject "$javainput"
%typemap(csin) jobject "$csinput.Handle"

%typemap(jni) jobject androidAssetManager "jobject"
%typemap(jtype) jobject androidAssetManager "android.content.res.AssetManager"
%typemap(jstype) jobject androidAssetManager "android.content.res.AssetManager"
%typemap(imtype) jobject androidAssetManager "System.IntPtr"
%typemap(cstype) jobject androidAssetManager "Android.Content.Res.AssetManager"

%include "utils/AssetUtils.h"

#endif
