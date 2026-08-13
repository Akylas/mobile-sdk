#ifndef _TERRAINOPTIONS_I
#define _TERRAINOPTIONS_I

%module TerrainOptions

!proxy_imports(carto::TerrainOptions, core.MapPos, core.MapPosVector, core.DoubleVector, datasources.TileDataSource, graphics.Color, rastertiles.ElevationDecoder)

%{
#include "components/TerrainOptions.h"
#include "components/Exceptions.h"
#include <memory>
%}

%include <std_string.i>
%include <std_shared_ptr.i>
%include <cartoswig.i>

%import "core/MapPos.i"
%import "core/DoubleVector.i"
%import "graphics/Color.i"
%import "datasources/TileDataSource.i"
%import "rastertiles/ElevationDecoder.i"

!shared_ptr(carto::TerrainOptions, components.TerrainOptions)

%attribute(carto::TerrainOptions, bool, Enabled, isEnabled, setEnabled)
%attribute(carto::TerrainOptions, float, Exaggeration, getExaggeration, setExaggeration)
%attribute(carto::TerrainOptions, bool, SeamlessTileEdgesEnabled, isSeamlessTileEdgesEnabled, setSeamlessTileEdgesEnabled)
%attribute(carto::TerrainOptions, bool, ElevationPrefetchEnabled, isElevationPrefetchEnabled, setElevationPrefetchEnabled)
%attribute(carto::TerrainOptions, int, MeshResolution, getMeshResolution, setMeshResolution)
%attribute(carto::TerrainOptions, bool, RegularGridEnabled, isRegularGridEnabled, setRegularGridEnabled)
%attribute(carto::TerrainOptions, bool, TileEdgeStitchingEnabled, isTileEdgeStitchingEnabled, setTileEdgeStitchingEnabled)
%attribute(carto::TerrainOptions, bool, PainterOrderDepthEnabled, isPainterOrderDepthEnabled, setPainterOrderDepthEnabled)
%attribute(carto::TerrainOptions, bool, DrapeFillsEnabled, isDrapeFillsEnabled, setDrapeFillsEnabled)
%attribute(carto::TerrainOptions, bool, DrapeLinesEnabled, isDrapeLinesEnabled, setDrapeLinesEnabled)
%attribute(carto::TerrainOptions, int, DrapeResolution, getDrapeResolution, setDrapeResolution)
%attributestring(carto::TerrainOptions, std::string, NoDrapeLayerFilter, getNoDrapeLayerFilter, setNoDrapeLayerFilter)
%attribute(carto::TerrainOptions, float, ElementTerrainSlack, getElementTerrainSlack, setElementTerrainSlack)
%attribute(carto::TerrainOptions, int, MinZoom, getMinZoom, setMinZoom)
%attribute(carto::TerrainOptions, int, MaxTileZoomOffset, getMaxTileZoomOffset, setMaxTileZoomOffset)
%attributeval(carto::TerrainOptions, carto::Color, BackgroundColor, getBackgroundColor, setBackgroundColor)
%attributeval(carto::TerrainOptions, carto::Color, FogColor, getFogColor, setFogColor)
%attribute(carto::TerrainOptions, float, FogStartDistance, getFogStartDistance, setFogStartDistance)
%attribute(carto::TerrainOptions, float, FogDistance, getFogDistance, setFogDistance)
%attribute(carto::TerrainOptions, float, ViewDistanceFactor, getViewDistanceFactor, setViewDistanceFactor)
%attribute(carto::TerrainOptions, float, ViewDistance, getViewDistance, setViewDistance)
%attribute(carto::TerrainOptions, int, MaxTileZoomCoarsening, getMaxTileZoomCoarsening, setMaxTileZoomCoarsening)
%attribute(carto::TerrainOptions, float, DepthBias, getDepthBias, setDepthBias)
%attribute(carto::TerrainOptions, bool, BillboardOcclusionEnabled, isBillboardOcclusionEnabled, setBillboardOcclusionEnabled)
%attribute(carto::TerrainOptions, float, BillboardOcclusionTolerance, getBillboardOcclusionTolerance, setBillboardOcclusionTolerance)
%attributestring(carto::TerrainOptions, std::string, SurfaceShaderSource, getSurfaceShaderSource, setSurfaceShaderSource)
%std_exceptions(carto::TerrainOptions::TerrainOptions)

%ignore carto::TerrainOptions::getSurfaceParameters;
%ignore carto::TerrainOptions::getSurfaceColorParameters;

%ignore carto::TerrainOptions::OnChangeListener;
%ignore carto::TerrainOptions::registerOnChangeListener;
%ignore carto::TerrainOptions::unregisterOnChangeListener;
%ignore carto::TerrainOptions::getElevationManager;
%ignore carto::TerrainOptions::getElevationCacheCapacity;
%ignore carto::TerrainOptions::setElevationCacheCapacity;

%include "components/TerrainOptions.h"

#endif
