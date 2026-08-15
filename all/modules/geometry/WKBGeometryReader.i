#ifndef _WKBGEOMETRYREADER_I
#define _WKBGEOMETRYREADER_I

%module WKBGeometryReader

#ifdef _MASSIF_WKBT_SUPPORT

!proxy_imports(massif::WKBGeometryReader, core.BinaryData, geometry.Geometry)

%{
#include "geometry/WKBGeometryReader.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "geometry/Geometry.i"

%std_exceptions(massif::WKBGeometryReader::readGeometry)

%include "geometry/WKBGeometryReader.h"

#endif

#endif
