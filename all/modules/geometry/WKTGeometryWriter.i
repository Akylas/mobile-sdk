#ifndef _WKTGEOMETRYWRITER_I
#define _WKTGEOMETRYWRITER_I

%module WKTGeometryWriter

#ifdef _MASSIF_WKBT_SUPPORT

!proxy_imports(massif::WKTGeometryWriter, geometry.Geometry)

%{
#include "geometry/WKTGeometryWriter.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <std_string.i>
%include <massifswig.i>

%import "geometry/Geometry.i"

%attribute(massif::WKTGeometryWriter, bool, Z, getZ, setZ)
%std_exceptions(massif::WKTGeometryWriter::writeGeometry)

%include "geometry/WKTGeometryWriter.h"

#endif

#endif
