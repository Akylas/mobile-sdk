#ifndef _WKTGEOMETRYREADER_I
#define _WKTGEOMETRYREADER_I

%module WKTGeometryReader

#ifdef _MASSIF_WKBT_SUPPORT

!proxy_imports(massif::WKTGeometryReader, geometry.Geometry)

%{
#include "geometry/WKTGeometryReader.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geometry/Geometry.i"

%std_exceptions(massif::WKTGeometryReader::readGeometry)

%include "geometry/WKTGeometryReader.h"

#endif

#endif
