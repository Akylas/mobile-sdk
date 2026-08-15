#ifndef _TILEDOWNLOADLISTENER_I
#define _TILEDOWNLOADLISTENER_I

%module(directors="1") TileDownloadListener

!proxy_imports(massif::TileDownloadListener, core.MapTile)

%{
#include "datasources/TileDownloadListener.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <massifswig.i>

%import "core/MapTile.i"

!polymorphic_shared_ptr(massif::TileDownloadListener, datasources.TileDownloadListener)

%feature("director") massif::TileDownloadListener;

%include "datasources/TileDownloadListener.h"

#endif
