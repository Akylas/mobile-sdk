#ifndef _BITMAPUTILS_I
#define _BITMAPUTILS_I

%module BitmapUtils

!proxy_imports(massif::BitmapUtils, graphics.Bitmap)
!objc_imports(massif::BitmapUtils, <UIKit/UIKit.h>)
!cs_imports(massif::BitmapUtils, UIKit)

%{
#include "utils/BitmapUtils.h"
#include "components/Exceptions.h"
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "graphics/Bitmap.i"

%typemap(objctype) UIImage* "UIImage*"
%typemap(objcin) UIImage* "(__bridge void*) $objcinput"
%typemap(objcout) UIImage* %{
    return (__bridge_transfer UIImage*)$imcall;
%}
%typemap(cstype) UIImage* "UIImage"
%typemap(csin) UIImage* "new HandleRef($csinput, $csinput.Handle)"
%typemap(csout, excode=SWIGEXCODE) UIImage* {
    var uiImage = $imcall; $excode;
    return ObjCRuntime.Runtime.GetNSObject<UIImage>(uiImage);
}
%typemap(in, canthrow=1) UIImage* %{
    $1 = (__bridge UIImage*)$input;
%}
%typemap(out) UIImage* %{
    $result = (__bridge_retained void*)$1;
%}

%std_exceptions(massif::BitmapUtils::CreateBitmapFromUIImage)
%std_exceptions(massif::BitmapUtils::CreateUIImageFromBitmap)

%include "utils/BitmapUtils.h"

#endif
