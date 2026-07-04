/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINRENDERER_H_
#define _CARTO_TERRAINRENDERER_H_

#include "core/MapTile.h"
#include "graphics/ViewState.h"

#include <map>
#include <memory>
#include <vector>

#include <cglib/vec.h>
#include <cglib/mat.h>

namespace carto {
    class ElevationManager;
    class ElevationTileGrid;
    class TerrainOptions;
    class FrameBuffer;
    class Shader;
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
         * Renders the packed terrain depth texture for post-processing. Returns true on success.
         * Leaves the previously bound framebuffer bound again on return.
         */
        bool renderDepthTexture(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);

        /**
         * Returns the GL texture id of the packed depth buffer (0 if not rendered).
         */
        unsigned int getDepthTextureId() const;

    private:
        struct TileMesh;
        struct MeshCacheEntry;

        static constexpr int BUFFER_DOWNSCALE = 2;    // packed depth texture runs at half resolution
        static constexpr int MIN_MESH_GRID_SIZE = 4;  // grid cells per tile edge, lower bound
        static constexpr int MAX_MESH_GRID_SIZE = 96; // grid cells per tile edge, upper bound
        static constexpr int MAX_CACHED_MESHES = 160;

        static const std::string TERRAIN_DEPTH_VERTEX_SHADER;
        static const std::string TERRAIN_DEPTH_FRAGMENT_SHADER;

        bool renderTiles(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);
        void calculateVisibleTiles(const ViewState& viewState, const std::shared_ptr<ElevationManager>& elevationManager, const MapTile& tile, std::vector<MapTile>& tiles) const;
        std::shared_ptr<TileMesh> buildTileMesh(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, const std::shared_ptr<ElevationManager>& elevationManager, int gridSize) const;
        int calculateMeshGridSize(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, const ViewState& viewState, int meshResolution) const;
        cglib::mat4x4<double> calculateTileMatrix(const MapTile& tile) const;

        std::shared_ptr<FrameBuffer> _frameBuffer;
        std::shared_ptr<Shader> _shader;
        std::map<long long, MeshCacheEntry> _meshCache;
    };
}

#endif
