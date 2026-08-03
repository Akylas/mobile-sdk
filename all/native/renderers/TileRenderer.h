/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TILERENDERER_H_
#define _CARTO_TILERENDERER_H_

#include "graphics/Color.h"
#include "components/StyleEnvironment.h"
#include "graphics/ViewState.h"
#include "renderers/utils/GLResource.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <map>
#include <tuple>
#include <vector>
#include <regex>
#include <optional>

#include <cglib/ray.h>

#include <vt/TileId.h>
#include <vt/Tile.h>
#include <vt/Bitmap.h>
#include <vt/Styles.h>
#include <vt/GLTileRenderer.h>

namespace carto {
    class ElevationTextureCache;
    class Options;
    class MapRenderer;
    class TerrainOptions;
    class TileDrawData;
    class ViewState;
    class VTRenderer;
    namespace vt {
        class LabelCuller;
        class TileTransformer;
    }
    
    class TileRenderer {
    public:
        TileRenderer();
        virtual ~TileRenderer();
    
        void setComponents(const std::weak_ptr<Options>& options, const std::weak_ptr<MapRenderer>& mapRenderer);

        std::shared_ptr<vt::TileTransformer> getTileTransformer() const;
        void setTileTransformer(const std::shared_ptr<vt::TileTransformer>& tileTransformer);
    
        void setInteractionMode(bool enabled);
        void setTerrainDepthWriteMode(bool enabled);
        void setTerrainRenderOrder(int order);
        void setLayerBlendingSpeed(float speed);
        void setLabelBlendingSpeed(float speed);
        void setLabelOrder(int order);
        void setBuildingOrder(int order);
        void setRasterFilterMode(vt::RasterFilterMode filterMode);
        void setNormalMapShadowColor(const Color& color);
        void setNormalMapHighlightColor(const Color& color);
        void setNormalMapAccentColor(const Color& color);
        void setNormalMapLightingShader(const std::string& shader);
        void setNormalMapElevationEncoded(bool enabled);
        void setNormalMapContourInterval(float interval);
        void setNormalMapContourColor(const Color& color);
        void setNormalMapContourWidth(float width);
        void setNormalIlluminationMapRotationEnabled(bool enabled);
        void setNormalIlluminationDirection(MapVec direction);
        void setHillshadeMethod(int method);
        void setHillshadeExaggeration(float exaggeration);
        void setHillshadeIntensity(float intensity);
        void setRendererLayerFilter(const std::optional<std::regex>& filter);
        void setClickHandlerLayerFilter(const std::optional<std::regex>& filter);

        void offsetLayerHorizontally(double offset);
    
        /**
         * Starts the vt frame (tile set, blending, compiled resources) without drawing anything.
         * Cross-layer draping needs every participating layer's render tiles ready BEFORE any of
         * them draws, so the shared drape can be baked first. onDrawFrame calls this itself when
         * it has not already run for this frame.
         */
        bool prepareFrame(float deltaSeconds, const ViewState& viewState);


        /**
         * Cross-layer drape support. The shared cache owns the textures; this renderer only
         * reports what it would drape and bakes its own content into a bound target.
         */
        void setExternalDrapeTarget(bool enabled);
        void setExternalDrapeTiles(const std::vector<vt::TileId>& tileIds);
        void setTerrainGroundTiles(const std::vector<vt::TileId>& tileIds);
        void setTerrainLayerOrdinalBase(int base);
        int getStyleLayerCount() const;
        int renderTerrainGround(const Color& color);
        bool isDrapeEnabled() const;
        void collectDrapeTiles(std::map<vt::TileId, std::size_t>& drapeTiles) const;
        int bakeDrapeTile(const vt::TileId& tileId);
        int renderDrapedSurface(const vt::TileId& tileId, unsigned int drapeTexture, float uvOffsetX, float uvOffsetY, float uvScale);
        int renderDrapedSurfaceFill(const vt::TileId& tileId, const Color& color);
        int blitDrapeTexture(unsigned int srcTexture, float dstOffsetX, float dstOffsetY, float dstScale, float uvOffsetX, float uvOffsetY, float uvScale);
        bool calculateShadowViewProj(const std::vector<vt::TileId>& tileIds, const std::vector<vt::TileId>& casterTileIds, const cglib::vec3<float>& sunDir, const std::vector<std::pair<double, double> >& tileHeights, double minHeight, double maxHeight, float maxDistanceMeters, int mapSize, int cascade, int cascadeCount, std::vector<vt::TileId>& boxCasterTileIds, double& depthRangeMeters, double& texelMeters, cglib::mat4x4<double>& lightViewProj) const;
        float shadowCasterFadeSignature() const;
        int renderShadowCasters(const std::vector<vt::TileId>& tileIds, const cglib::mat4x4<double>& lightViewProj, bool castGround);
        void setTerrainShadowMap(unsigned int texture, int mapSize, int cascades, const std::array<float, 4>& depthBiases, float strength, float softness, const std::array<cglib::mat4x4<double>, 4>& lightViewProjs);
        // Pushed by the owner BEFORE the shared terrain surface is drawn. onDrawFrame sets the same
        // state, but it runs after that draw, so the surface would light itself with the PREVIOUS
        // frame's sun - invisible while the map redrew continuously, and a change that appears not
        // to apply at all once it goes idle.
        void setTerrainSunLighting(bool enabled, const cglib::vec3<float>& sunDir, const Color& sunColor, float sunIntensity, float ambientIntensity);
        // Turns this renderer into a terrain paint baker: it shades the shared terrain elevation
        // texture into the drape texture, at its own place in the layer order, instead of holding
        // a tile set of its own. The fingerprint must cover every value the paint's appearance
        // depends on, including the lighting shader's own uniforms, or already-baked drape
        // textures survive a parameter change.
        // The terrain tiles a paint draws itself on when there is no drape to bake into.
        void setTerrainPaintTiles(const std::vector<vt::TileId>& tileIds);
        void setTerrainPaint(bool enabled, bool fullDetail, float heightScale, bool exaggerateHeightScale, bool legacyHeightScale, float contrast, float opacity, std::size_t fingerprint);

        bool onDrawFrame(float deltaSeconds, const ViewState& viewState);
        bool onDrawFrame3D(float deltaSeconds, const ViewState& viewState);
    
        bool cullLabels(vt::LabelCuller& culler, const ViewState& viewState);

        bool refreshTiles(const std::vector<std::shared_ptr<TileDrawData> >& drawDatas);

        void calculateRayIntersectedElements(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const;
        void calculateRayIntersectedElements3D(const cglib::ray3<double>& ray, const ViewState& viewState, float radius, std::vector<vt::GLTileRenderer::GeometryIntersectionInfo>& results) const;
        void calculateRayIntersectedBitmaps(const cglib::ray3<double>& ray, const ViewState& viewState, std::vector<vt::GLTileRenderer::BitmapIntersectionInfo>& results) const;
    
        // The style's own sun/shadow/fog values for this frame, pushed by the layer that owns
        // this renderer. What the style leaves unset comes from LightOptions/TerrainOptions.
        void setStyleEnvironment(const StyleEnvironment& env);

        static Color evaluateColorFunc(const vt::ColorFunction& colorFunc, const ViewState& viewState);
        static float evaluateFloatFunc(const vt::FloatFunction& floatFunc, const ViewState& viewState);

    private:
        struct LabelOcclusionState;

        bool initializeRenderer();
        bool isPlanarTerrainMode() const;
        // Tangram-model measurement switch, read once from debug.carto.depthshift (Android only).
        static float getTerrainContentDepthShift();
        // Measurement override for the paint's DEM level: debug.carto.paintdetail 0 forces the
        // mesh level, whatever the layer asks for. Read once (Android only).
        static bool isTerrainPaintFullDetailAllowed();
        void updateLabelOcclusionTest(const std::shared_ptr<vt::GLTileRenderer>& tileRenderer, const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions);

        static constexpr int SURFACE_RESET_DELAY = 500; // minimum interval (ms) between elevation-driven tile surface rebuilds

        static constexpr float OCCLUSION_SAMPLE_OFFSET = 8.0f; // screen pixels sampled around a label anchor for the terrain depth
        static constexpr float MIN_OCCLUSION_TOLERANCE = 0.01f; // relative depth slack a label anchored on the terrain always gets

        static const std::string LIGHTING_SHADER_2D;
        static const std::string LIGHTING_SHADER_3D;
        static const std::string LIGHTING_SHADER_NORMALMAP;

        std::weak_ptr<MapRenderer> _mapRenderer;
        std::weak_ptr<Options> _options;
        StyleEnvironment _styleEnvironment;
        std::shared_ptr<vt::TileTransformer> _tileTransformer;

        std::shared_ptr<VTRenderer> _vtRenderer;
        bool _interactionMode;
        float _layerBlendingSpeed;
        float _labelBlendingSpeed;
        int _labelOrder;
        int _buildingOrder;
        vt::RasterFilterMode _rasterFilterMode;
        Color _normalMapShadowColor;
        Color _normalMapAccentColor;
        Color _normalMapHighlightColor;
        std::string _normalMapLightingShader;
        bool _normalMapElevationEncoded = false;
        float _normalMapContourInterval = 0.0f; // meters; <= 0 disables contour lines
        Color _normalMapContourColor;
        float _normalMapContourWidth = 0.75f; // contour half-width in screen pixels
        std::optional<std::regex> _rendererLayerFilter;
        std::optional<std::regex> _clickHandlerLayerFilter;

        double _horizontalLayerOffset;
        cglib::vec3<float> _viewDir;
        cglib::vec3<float> _mainLightDir;
        // The sun as RESOLVED (style over LightOptions), captured each frame for the 3D lighting
        // shader callback, which runs at draw time and cannot resolve it itself.
        cglib::vec3<float> _resolvedSunDir = cglib::vec3<float>(0, 0, 1);
        bool _sunLightingEnabled = false;
        float _sunIntensity = 0.0f;
        float _sunAmbient = 0.35f;
        cglib::vec3<float> _normalLightDir;
        MapVec _normalIlluminationDirection;
        bool _normalIlluminationMapRotationEnabled;
        double _mapRotation;
        int _hillshadeMethod;
        float _hillshadeExaggeration;
        float _hillshadeIntensity;
        bool _terrainDepthWriteMode = false;
        bool _terrainPaintEnabled = false; // this renderer shades the DEM instead of drawing tiles
        bool _terrainPaintFullDetail = true; // shade from the DEM's own max zoom, not the mesh's level
        bool prepareFrameUnsafe(float deltaSeconds, const ViewState& viewState); // caller holds _mutex

        bool _framePrepared = false;   // startFrame already ran this frame (cross-layer drape ordering)
        bool _framePrepareResult = false;
        bool _externalDrapeTarget = false;
        bool _terrainGroundActive = false; // a shared ground cover is set: this stack draws a terrain surface without a drape
        int _terrainRenderOrder = 0;
        int _maxVertexTextureUnits = -1; // lazily queried GL capability (-1 = not queried yet)
        std::shared_ptr<ElevationTextureCache> _elevationTextureCache;
        unsigned int _elevationVersion = 0;
        std::optional<std::chrono::steady_clock::time_point> _lastSurfaceResetTime;
        std::shared_ptr<LabelOcclusionState> _labelOcclusionState;

        std::map<vt::TileId, std::shared_ptr<const vt::Tile> > _tiles;
        
        mutable std::mutex _mutex;
    };
    
}

#endif
