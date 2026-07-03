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
    class TerrainOptions;
    class FrameBuffer;
    class Shader;
    class GLResourceManager;
    class VBO;

    /**
     * Renders a terrain-only depth pre-pass into an offscreen buffer: per-tile grid meshes
     * (with skirts) displaced by the elevation data, output as packed 24-bit linear depth
     * (RGB, relative to the far plane) plus terrain coverage (A). Used by post-process effects.
     * Internal class, not exposed in the public API.
     */
    class TerrainRenderer {
    public:
        TerrainRenderer();
        virtual ~TerrainRenderer();

        /**
         * Renders the terrain depth pre-pass. Returns true on success.
         * Leaves the previously bound framebuffer bound again on return.
         */
        bool onDrawFrame(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager);

        /**
         * Returns the GL texture id of the packed depth buffer (0 if not rendered).
         */
        unsigned int getDepthTextureId() const;

    private:
        struct TileMesh;

        static constexpr int BUFFER_DOWNSCALE = 2;   // pre-pass runs at half resolution
        static constexpr int MESH_GRID_SIZE = 32;    // grid cells per tile edge
        static constexpr int MAX_CACHED_MESHES = 128;

        static const std::string TERRAIN_DEPTH_VERTEX_SHADER;
        static const std::string TERRAIN_DEPTH_FRAGMENT_SHADER;

        void calculateVisibleTiles(const ViewState& viewState, const std::shared_ptr<ElevationManager>& elevationManager, const MapTile& tile, std::vector<MapTile>& tiles) const;
        std::shared_ptr<TileMesh> buildTileMesh(const MapTile& tile, const std::shared_ptr<ElevationManager>& elevationManager) const;
        cglib::mat4x4<double> calculateTileMatrix(const MapTile& tile) const;

        std::shared_ptr<FrameBuffer> _frameBuffer;
        std::shared_ptr<Shader> _shader;
        std::map<long long, std::pair<unsigned int, std::shared_ptr<TileMesh> > > _meshCache; // tileId -> (elevation version, mesh)
        unsigned int _elevationVersion = 0;
    };
}

#endif
