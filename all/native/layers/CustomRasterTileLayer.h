/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CUSTOMRASTERTILELAYER_H_
#define _CARTO_CUSTOMRASTERTILELAYER_H_

#include "layers/RasterTileLayer.h"

#include <mutex>
#include <string>

namespace carto {

    /**
     * A raster tile layer that renders each tile through a custom GLSL fragment shader ("filter").
     * The data source can be any raster TileDataSource - most commonly an RGB-encoded elevation
     * (terrarium/mapbox) source, but any raster works. This is the general form of
     * HillshadeRasterTileLayer, which specializes it for DEM data (normal map + hillshade + contours).
     *
     * The shader must define:
     *   vec4 applyLighting(lowp vec4 color, mediump vec3 normal, mediump vec3 surfaceNormal, mediump float intensity)
     * and it can use these helpers (injected before it):
     *   getRawColor()  - the untouched RGBA texel of the source tile at this fragment
     *   getMapZoom()   - the current fractional map zoom (for per-zoom logic)
     * plus the shared uniforms/varyings vUV (tile uv), uUVScale (texture size) and uBitmap.
     * It must return a PREMULTIPLIED color (rgb already multiplied by alpha); return alpha 0 where the
     * output should be transparent so layers below show through.
     *
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class CustomRasterTileLayer : public RasterTileLayer {
    public:
        /**
         * Constructs a CustomRasterTileLayer object from a raster data source.
         * @param dataSource The raster data source from which this layer loads data.
         */
        explicit CustomRasterTileLayer(const std::shared_ptr<TileDataSource>& dataSource);
        virtual ~CustomRasterTileLayer();

        /**
         * Returns the custom fragment shader source.
         * @return The shader source. Empty means the default passthrough (outputs the raw tile).
         */
        std::string getShaderSource() const;
        /**
         * Sets the custom fragment shader source (the "filter"). See the class description for the
         * required entry point and available helpers. Empty resets to the passthrough shader.
         * @param shaderSource The GLSL shader source.
         */
        void setShaderSource(const std::string& shaderSource);

    protected:
        virtual bool onDrawFrame(float deltaSeconds, BillboardSorter& billboardSorter, const ViewState& viewState);

        virtual std::shared_ptr<vt::Tile> createVectorTile(const MapTile& subTile, const MapTile& tile, const std::shared_ptr<TileData>& tileData, const std::shared_ptr<Bitmap>& bitmap, const std::shared_ptr<vt::TileTransformer>& tileTransformer) const;

        // The shader source actually handed to the renderer. Falls back to PASSTHROUGH_SHADER.
        virtual std::string getEffectiveShaderSource() const;

        static const std::string PASSTHROUGH_SHADER;

        std::string _shaderSource;
        mutable std::recursive_mutex _customShaderMutex;
    };

}

#endif
