/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_HILLSHADERASTERTILELAYER_H_
#define _CARTO_HILLSHADERASTERTILELAYER_H_

#include "graphics/Color.h"
#include "components/DirectorPtr.h"
#include "layers/CustomRasterTileLayer.h"
#include "rastertiles/ElevationDecoder.h"

#include <atomic>

namespace carto {
    class ElevationManager;

    namespace HillshadeMethod {
        /**
        * Hillshade rendering method.
        */
        enum HillshadeMethod {
            /**
            * MapLibre's legacy hillshade algorithm.
            */
            STANDARD,
            /**
            * Combined hillshade algorithm based on GDAL.
            */
            COMBINED,
            /**
            * Igor hillshade algorithm based on GDAL.
            */
            IGOR,
            /**
            * Multi-directional hillshade with multiple light sources.
            */
            MULTIDIRECTIONAL,
            /**
            * Basic hillshade algorithm based on GDAL.
            */
            BASIC
        };
    }
    
    /**
     * A tile layer that displays an overlay hillshading. Should be used together with corresponding data source that encodes height in RGBA image.
     * The shading is based on the direction of the main light source, which can be configured using Options class.
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class HillshadeRasterTileLayer : public CustomRasterTileLayer {
    public:
        /**
         * Constructs a HillshadeRasterTileLayer object from a data source.
         * @param dataSource The data source from which this layer loads data.
         */
        explicit HillshadeRasterTileLayer(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder);
        explicit HillshadeRasterTileLayer(const std::shared_ptr<TileDataSource>& dataSource);
        virtual ~HillshadeRasterTileLayer();

        /**
         * Returns the contrast of the hillshade overlay.
         * @return The contrast value (between 0..1). Default is 0.5.
         */
        float getContrast() const;
        /**
         * Sets the contrast of the hillshade overlay.
         * @param contrast The contrast value (between 0..1).
         */
        void setContrast(float contrast);

        /**
         * Returns the height scale of the hillshade overlay.
         * @return The relative height scale. Default is 1.0.
         */
        float getHeightScale() const;
        /**
         * Sets the height scale of the hillshade overlay.
         * @param heightScale The relative height scale. Actual height is multiplied by this values.
         */
        void setHeightScale(float heightScale);

        /**
         * Returns the shading color of areas that face away from the light source.
         * @return The shadow color of the layer.
         */
        Color getShadowColor() const;
        /**
         * Sets the shading color of areas that face away from the light source.
         * @param color The new shadow color of the layer.
         */
        void setShadowColor(const Color& color);
        /**
         * Returns the shading color used to accentuate rugged terrain like sharp cliffs and gorges.
         * @return The accent color of the layer.
         */
        Color getAccentColor() const;
        /**
         * Sets the shading color used to accentuate rugged terrain like sharp cliffs and gorges.
         * @param color The new accent color of the layer.
         */
        void setAccentColor(const Color& color);

        /**
         * Returns the shading color of areas that faces towards the light source.
         * @return The highlight color of the layer.
         */
        Color getHighlightColor() const;

        /**
         * Sets the shading color of areas that faces towards the light source.
         * @param color The new highlight color of the layer.
         */
        void setHighlightColor(const Color& color);

        std::string getNormalMapLightingShader() const;
        /**
         * Sets a custom normalmap lighting shader.
         * @param shader The custom shader.
         */
        void setNormalMapLightingShader(const std::string& shader);

        /**
         * Returns the illumination direction of the layer.
         * @return The direction vector for the hillshade illumination
         */
        MapVec getIlluminationDirection() const;
        /**
         * Sets the illumination direction.
         * @param The new direction vector for the illumination light. (0,0,-1) means straight down, (-0.707,0,-0.707) means
         *        from east with a 45 degree angle. The direction vector will be normalized.
         */
        void setIlluminationDirection(MapVec direction);
        /**
         * Returns wheter the illumination direction should change with the map rotation.
         * @return enabled
         */
        bool getIlluminationMapRotationEnabled() const;
        /**
         * Sets wheter the illumination direction should change with the map rotation.
         * @param enabled whether to enable or not.
         */
        void setIlluminationMapRotationEnabled(bool enabled);
        /**
         * Returns the normal vector tile should be exagerated based on the zoom level.
         * @return enabled
         */
        bool getExagerateHeightScaleEnabled() const;

        /**
         * Sets wheter the normal vector tile should be exagerated based on the zoom level.
         * @param enabled whether to enable or not.
         */
        void setExagerateHeightScaleEnabled(bool enabled);

        /**
         * Returns the hillshade rendering method.
         * @return The hillshade method. Default is STANDARD.
         */
        HillshadeMethod::HillshadeMethod getHillshadeMethod() const;
        /**
         * Sets the hillshade rendering method.
         * @param method The hillshade method to use.
         */
        void setHillshadeMethod(HillshadeMethod::HillshadeMethod method);

        /**
         * Returns whether the normal map encodes absolute elevation (so a custom normal-map lighting
         * shader can call getElevation()).
         * @return True if elevation encoding is enabled. Default is false.
         */
        bool isElevationEncodingEnabled() const;
        /**
         * Sets whether the normal map encodes absolute elevation in addition to the surface normal.
         * Required for a custom normal-map lighting shader that reads getElevation()/getMapZoom() (e.g.
         * to draw its own per-zoom contour lines). Enabling contour lines turns this on implicitly.
         * @param enabled True to encode elevation into the normal map.
         */
        void setElevationEncodingEnabled(bool enabled);

        /**
         * Returns whether GPU contour lines are drawn over the hillshade.
         * @return True if contour lines are enabled. Default is false.
         */
        bool isContourEnabled() const;
        /**
         * Sets whether to draw anti-aliased contour lines over the hillshade, computed in the shader
         * from the elevation data. Enabling this makes the normal map encode absolute elevation (which
         * is also available to a custom normal-map lighting shader). Note: this class is experimental.
         * @param enabled True to draw contour lines.
         */
        void setContourEnabled(bool enabled);
        /**
         * Returns the spacing between contour lines in meters.
         * @return The contour interval in meters. Default is 100.
         */
        float getContourInterval() const;
        /**
         * Sets the spacing between contour lines in meters.
         * @param interval The contour interval in meters.
         */
        void setContourInterval(float interval);
        /**
         * Returns the contour line color.
         * @return The contour line color.
         */
        Color getContourColor() const;
        /**
         * Sets the contour line color.
         * @param color The contour line color.
         */
        void setContourColor(const Color& color);
        /**
         * Returns the contour line half-width in screen pixels.
         * @return The contour line width. Default is 0.75.
         */
        float getContourWidth() const;
        /**
         * Sets the contour line half-width in screen pixels.
         * @param width The contour line width in pixels.
         */
        void setContourWidth(float width);

        double getElevation(const MapPos& pos) const;
        std::vector<double> getElevations(const std::vector<MapPos> poses) const;

    protected:
        virtual bool onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState);

        virtual std::shared_ptr<vt::Tile> createVectorTile(const MapTile& subTile, const MapTile& tile, const std::shared_ptr<TileData>& tileData, const std::shared_ptr<Bitmap>& bitmap, const std::shared_ptr<vt::TileTransformer>& tileTransformer) const;

        std::shared_ptr<Bitmap> getTileDataBitmap(std::shared_ptr<TileData> tileData) const;
        std::shared_ptr<Bitmap> getMapTileBitmap(const MapTile& mapTile) const;
        std::shared_ptr<ElevationManager> getElevationManager() const;

        const std::shared_ptr<ElevationDecoder> _elevationDecoder;

        mutable std::shared_ptr<ElevationManager> _elevationManager;
   
        std::atomic<float> _contrast;
        std::atomic<bool> _exagerateHeightScaleEnabled;
        std::atomic<float> _heightScale;
        std::string _normalMapLightingShader;
        std::atomic<Color> _shadowColor;
        std::atomic<Color> _accentColor;
        std::atomic<Color> _highlightColor;
        std::atomic<MapVec> _illuminationDirection;
        std::atomic<bool> _illuminationMapRotationEnabled;
        std::atomic<HillshadeMethod::HillshadeMethod> _hillshadeMethod;
        std::atomic<bool> _contourEnabled;
        std::atomic<bool> _elevationEncodingEnabled;
        std::atomic<float> _contourInterval;
        std::atomic<Color> _contourColor;
        std::atomic<float> _contourWidth;

        // Elevation is packed into the normal map when contours are on or when explicitly requested
        // for a custom shader.
        bool isElevationEncoded() const { return _contourEnabled.load() || _elevationEncodingEnabled.load(); }
    };
    
}

#endif
