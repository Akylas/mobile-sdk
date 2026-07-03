#ifndef _TERRAINOPTIONS_I
#define _TERRAINOPTIONS_I

%module TerrainOptions

!proxy_imports(carto::TerrainOptions, core.MapPos, core.MapPosVector, core.DoubleVector, datasources.TileDataSource, rastertiles.ElevationDecoder)

%{
#include "components/TerrainOptions.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "core/DoubleVector.i"
%import "datasources/TileDataSource.i"
%import "rastertiles/ElevationDecoder.i"

!shared_ptr(carto::TerrainOptions, components.TerrainOptions)

%attribute(carto::TerrainOptions, bool, Enabled, isEnabled, setEnabled)
%attribute(carto::TerrainOptions, float, Exaggeration, getExaggeration, setExaggeration)
%attribute(carto::TerrainOptions, int, MeshResolution, getMeshResolution, setMeshResolution)
%attribute(carto::TerrainOptions, bool, BillboardOcclusionEnabled, isBillboardOcclusionEnabled, setBillboardOcclusionEnabled)
%std_exceptions(carto::TerrainOptions::TerrainOptions)

%ignore carto::TerrainOptions::OnChangeListener;
%ignore carto::TerrainOptions::registerOnChangeListener;
%ignore carto::TerrainOptions::unregisterOnChangeListener;
%ignore carto::TerrainOptions::getElevationManager;
%ignore carto::TerrainOptions::getElevationCacheCapacity;
%ignore carto::TerrainOptions::setElevationCacheCapacity;

%include "components/TerrainOptions.h"

#endif
