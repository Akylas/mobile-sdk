#ifndef _WKBGEOMETRYWRITER_I
#define _WKBGEOMETRYWRITER_I

%module WKBGeometryWriter

#ifdef _MASSIF_WKBT_SUPPORT

!proxy_imports(massif::WKBGeometryWriter, core.BinaryData, geometry.Geometry)

%{
#include "geometry/WKBGeometryWriter.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/BinaryData.i"
%import "geometry/Geometry.i"

%attribute(massif::WKBGeometryWriter, bool, Z, getZ, setZ)
%attribute(massif::WKBGeometryWriter, bool, BigEndian, getBigEndian, setBigEndian)
%std_exceptions(massif::WKBGeometryWriter::writeGeometry)

%include "geometry/WKBGeometryWriter.h"

#endif

#endif
