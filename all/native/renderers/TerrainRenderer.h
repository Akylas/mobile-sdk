/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINRENDERER_H_
#define _CARTO_TERRAINRENDERER_H_

#include "core/MapTile.h"
#include "graphics/Color.h"
#include "graphics/ViewState.h"

#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <vector>

#include <cglib/vec.h>
#include <cglib/mat.h>

namespace carto {
    class Bitmap;
    class ElevationManager;
    class ElevationTileGrid;
    class TerrainOptions;
    class FrameBuffer;
    class Shader;
    class Texture;
    class GLResourceManager;

    /**
     * Renders the displaced terrain surface as per-tile grid meshes (with skirts).
     * Used in two ways:
     * 1. renderDepthPrepass: renders terrain depth into the currently bound framebuffer
     *    (color writes disabled) before the tile layers are drawn. The 2D tile geometry
     *    then depth-tests against this single consistent depth source (with a small bias),
     *    which gives terrain self-occlusion without z-fighting between layers.
     * 2. renderDepthTexture: renders packed 24-bit linear depth (RGB, relative to the
     *    far plane) plus terrain coverage (A) into a half-resolution offscreen buffer,
     *    consumed by post-process effects.
     * Internal class, not exposed in the public API.
     */
    class TerrainRenderer {
    public:
        TerrainRenderer();
        virtual ~TerrainRenderer();

        /**
         * Renders terrain depth into the currently bound framebuffer. Color writes are
         * disabled during the pass and GL state is restored on return. Returns true on success.
         */
        bool renderDepthPrepass(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);

        /**
         * Renders the terrain surface as an opaque solid color into the currently bound
         * framebuffer. The fill is always depth-resolved internally (near slopes win over
         * far slopes). With keepDepth, the terrain depth stays in the depth buffer and
         * subsumes renderDepthPrepass (used when no tile layer provides the terrain
         * depth). Without keepDepth the depth buffer is cleared afterwards: the fill is
         * color-only and can not depth-clip the differently-tesselated tile layer
         * content drawn above it - the tile layer surface pre-passes provide the depth.
         * GL state is restored on return. Returns true on success.
         */
        bool renderBackground(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager, const Color& color, bool keepDepth);

        /**
         * Renders the terrain surface with the given repeating background bitmap draped
         * over it (the same world-anchored tiling the flat-map BackgroundRenderer uses) -
         * the bitmap variant of the color background, with the same keepDepth semantics.
         * GL state is restored on return. Returns true on success.
         */
        bool renderBackground(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager, const std::shared_ptr<Bitmap>& bitmap, bool keepDepth);

        /**
         * Renders the packed terrain depth texture for post-processing. Returns true on success.
         * Leaves the previously bound framebuffer bound again on return.
         */
        bool renderDepthTexture(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);

        /**
         * Returns the GL texture id of the packed depth buffer (0 if not rendered).
         */
        unsigned int getDepthTextureId() const;

        /**
         * Renders the terrain depth texture and reads it back into a CPU buffer for
         * pixel-exact occlusion queries (getDepthW). Returns true on success.
         */
        bool updateDepthBuffer(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);

        /**
         * Returns the linear eye depth (view w, internal units) of the terrain at the
         * given screen position from the last updateDepthBuffer call. Returns a huge
         * value for sky pixels or when no depth data is available.
         */
        float getDepthW(float screenX, float screenY) const;

    private:
        struct TileMesh;
        struct MeshCacheEntry;

        static constexpr int BUFFER_DOWNSCALE = 2;    // packed depth texture runs at half resolution
        static constexpr int DEPTH_READBACK_THROTTLE = 60; // minimum interval (ms) between occlusion depth read-backs while the camera moves
        static constexpr int MIN_MESH_GRID_SIZE = 4;  // grid cells per tile edge, lower bound
        static constexpr int MAX_MESH_GRID_SIZE = 96; // grid cells per tile edge, upper bound
        static constexpr int MAX_CACHED_MESHES = 160;

        static const std::string TERRAIN_DEPTH_VERTEX_SHADER;
        static const std::string TERRAIN_DEPTH_FRAGMENT_SHADER;
        static const std::string TERRAIN_COLOR_FRAGMENT_SHADER;
        static const std::string TERRAIN_BITMAP_VERTEX_SHADER;
        static const std::string TERRAIN_BITMAP_FRAGMENT_SHADER;

        bool renderTiles(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager, const std::shared_ptr<Shader>& shader, const std::function<void(const MapTile&)>& tileUniformsFn = std::function<void(const MapTile&)>());
        void calculateVisibleTiles(const ViewState& viewState, const std::shared_ptr<ElevationManager>& elevationManager, const MapTile& tile, std::vector<MapTile>& tiles) const;
        std::shared_ptr<TileMesh> buildTileMesh(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, const std::shared_ptr<ElevationManager>& elevationManager, int gridSize) const;
        int calculateMeshGridSize(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, int meshResolution) const;
        cglib::mat4x4<double> calculateTileMatrix(const MapTile& tile) const;

        std::shared_ptr<FrameBuffer> _frameBuffer;
        std::shared_ptr<Shader> _shader;
        std::shared_ptr<Shader> _colorShader;
        std::shared_ptr<Shader> _bitmapShader;
        std::shared_ptr<Bitmap> _backgroundBitmap; // source of _backgroundTex, for change detection
        std::shared_ptr<Texture> _backgroundTex;
        std::map<long long, MeshCacheEntry> _meshCache;

        std::vector<std::uint8_t> _depthData; // read-back packed depth (RGBA, BUFFER_DOWNSCALE resolution)
        int _depthWidth = 0;
        int _depthHeight = 0;
        float _depthFar = 0.0f;
        cglib::mat4x4<double> _depthMVPMatrix = cglib::mat4x4<double>::zero(); // camera state of the last read-back
        unsigned int _depthElevationVersion = 0;
        std::chrono::steady_clock::time_point _depthReadbackTime; // throttles read-backs while the camera moves
    };
}

#endif
