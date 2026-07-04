#include "TerrainRenderer.h"
#include "components/Options.h"
#include "components/TerrainOptions.h"
#include "datasources/TileDataSource.h"
#include "renderers/utils/FrameBuffer.h"
#include "renderers/utils/GLContext.h"
#include "renderers/utils/GLResourceManager.h"
#include "renderers/utils/Shader.h"
#include "terrain/ElevationManager.h"
#include "terrain/ElevationTileGrid.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <algorithm>
#include <limits>
#include <cmath>

namespace carto {

    struct TerrainRenderer::TileMesh {
        std::vector<float> vertices; // x, y in tile coordinates [0..1], z in tile-local units
        std::vector<unsigned short> indices;
    };

    struct TerrainRenderer::MeshCacheEntry {
        std::shared_ptr<ElevationTileGrid> grid; // the grid the mesh was built from
        float exaggeration = 1.0f;
        int gridSize = 0;
        std::shared_ptr<TileMesh> mesh;
    };

    TerrainRenderer::TerrainRenderer() :
        _frameBuffer(),
        _shader(),
        _meshCache()
    {
    }

    TerrainRenderer::~TerrainRenderer() {
    }

    bool TerrainRenderer::renderDepthPrepass(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager) {
        if (!terrainOptions || !glResourceManager || viewState.getWidth() <= 0 || viewState.getHeight() <= 0) {
            return false;
        }

        // Depth-only pass into the current framebuffer: this is the single source of truth
        // that 2D draped geometry depth-tests against (with a bias towards the viewer).
        // Slope-scaled polygon offset pushes the pre-pass depth slightly away from the
        // viewer: the pre-pass mesh and the draped tile meshes are different tesselations
        // of the same height field, and near the camera (steep, glancing surfaces) their
        // difference exceeds any practical constant clip-space bias.
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_BLEND);
        glDisable(GL_STENCIL_TEST);
        glDisable(GL_CULL_FACE); // displaced surfaces can face away near ridge crests
        // Keep the factor moderate: it scales with the per-pixel depth slope, which gets
        // large at ridge silhouettes - too much offset lets geometry behind ridges
        // 'shine through' in a band along every silhouette.
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 2.0f);

        bool result = renderTiles(viewState, terrainOptions, glResourceManager);

        // Restore state expected by the layer renderers
        glDisable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(0.0f, 0.0f);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
        glEnable(GL_BLEND);
        glEnable(GL_CULL_FACE);

        GLContext::CheckGLError("TerrainRenderer::renderDepthPrepass");
        return result;
    }

    bool TerrainRenderer::renderDepthTexture(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager) {
        if (!terrainOptions || !glResourceManager || viewState.getWidth() <= 0 || viewState.getHeight() <= 0) {
            return false;
        }

        int bufferWidth = std::max(1, viewState.getWidth() / BUFFER_DOWNSCALE);
        int bufferHeight = std::max(1, viewState.getHeight() / BUFFER_DOWNSCALE);
        if (!_frameBuffer || !_frameBuffer->isValid() || _frameBuffer->getWidth() != bufferWidth || _frameBuffer->getHeight() != bufferHeight) {
            _frameBuffer = glResourceManager->create<FrameBuffer>(bufferWidth, bufferHeight, true, true, false);
        }
        if (!_frameBuffer) {
            return false;
        }

        GLint prevFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer->getFBOId());
        glViewport(0, 0, bufferWidth, bufferHeight);

        // Clear to 'sky': maximum depth, zero coverage
        glClearColor(1.0f, 1.0f, 1.0f, 0.0f);
        glDepthMask(GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE); // displaced surfaces can face away near ridge crests

        bool result = renderTiles(viewState, terrainOptions, glResourceManager);

        // Restore state
        glEnable(GL_CULL_FACE);
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);
        glViewport(0, 0, viewState.getWidth(), viewState.getHeight());
        glEnable(GL_BLEND);
        glDepthMask(GL_FALSE);

        GLContext::CheckGLError("TerrainRenderer::renderDepthTexture");
        return result;
    }

    unsigned int TerrainRenderer::getDepthTextureId() const {
        return _frameBuffer && _frameBuffer->isValid() ? _frameBuffer->getColorTexId() : 0;
    }

    bool TerrainRenderer::updateDepthBuffer(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager) {
        _depthWidth = 0;
        _depthHeight = 0;
        if (!renderDepthTexture(viewState, terrainOptions, glResourceManager)) {
            return false;
        }

        int bufferWidth = std::max(1, viewState.getWidth() / BUFFER_DOWNSCALE);
        int bufferHeight = std::max(1, viewState.getHeight() / BUFFER_DOWNSCALE);
        _depthData.resize(static_cast<std::size_t>(bufferWidth) * bufferHeight * 4);

        GLint prevFBO = 0;
        glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, _frameBuffer->getFBOId());
        glReadPixels(0, 0, bufferWidth, bufferHeight, GL_RGBA, GL_UNSIGNED_BYTE, _depthData.data());
        glBindFramebuffer(GL_FRAMEBUFFER, prevFBO);

        _depthWidth = bufferWidth;
        _depthHeight = bufferHeight;
        _depthFar = viewState.getFar();
        GLContext::CheckGLError("TerrainRenderer::updateDepthBuffer");
        return true;
    }

    float TerrainRenderer::getDepthW(float screenX, float screenY) const {
        if (_depthWidth < 1 || _depthHeight < 1) {
            return std::numeric_limits<float>::max();
        }
        int x = std::min(std::max(static_cast<int>(screenX) / BUFFER_DOWNSCALE, 0), _depthWidth - 1);
        int y = std::min(std::max(static_cast<int>(screenY) / BUFFER_DOWNSCALE, 0), _depthHeight - 1);
        // The framebuffer rows start at the bottom of the screen; screen y grows downwards
        const std::uint8_t* ptr = &_depthData[(static_cast<std::size_t>(_depthHeight - 1 - y) * _depthWidth + x) * 4];
        if (ptr[3] == 0) {
            return std::numeric_limits<float>::max(); // sky pixel (zero coverage)
        }
        float depth = ptr[0] / 255.0f + ptr[1] / 65025.0f + ptr[2] / 16581375.0f;
        return depth * _depthFar;
    }

    bool TerrainRenderer::renderTiles(const ViewState& viewState, const std::shared_ptr<TerrainOptions>& terrainOptions, const std::shared_ptr<GLResourceManager>& glResourceManager) {
        std::shared_ptr<ElevationManager> elevationManager = terrainOptions->getElevationManager();

        if (!_shader || !_shader->isValid()) {
            _shader = glResourceManager->create<Shader>("terraindepth", TERRAIN_DEPTH_VERTEX_SHADER, TERRAIN_DEPTH_FRAGMENT_SHADER);
        }
        if (!_shader) {
            return false;
        }

        // Calculate visible terrain tiles
        std::vector<MapTile> tiles;
        calculateVisibleTiles(viewState, elevationManager, MapTile(0, 0, 0, 0), tiles);

        glUseProgram(_shader->getProgId());
        GLuint aCoord = _shader->getAttribLoc("a_coord");
        GLuint uMVPMat = _shader->getUniformLoc("u_mvpMat");
        glEnableVertexAttribArray(aCoord);
        glUniform1f(_shader->getUniformLoc("u_far"), viewState.getFar());

        float exaggeration = elevationManager->getExaggeration();
        int minZoom = terrainOptions->getMinZoom();
        int meshResolution = terrainOptions->getMeshResolution();
        const cglib::mat4x4<double>& mvpMat = viewState.getModelviewProjectionMat();
        for (const MapTile& tile : tiles) {
            long long tileId = tile.getTileId();
            std::shared_ptr<ElevationTileGrid> grid;
            if (tile.getZoom() >= minZoom) {
                grid = elevationManager->getTileGrid(tile, ElevationManager::LoadMode::CACHED_ONLY);
            }
            int gridSize = calculateMeshGridSize(tile, grid, meshResolution);

            // Rebuild the mesh only when its inputs actually changed. This avoids rebuilding
            // every cached mesh each time a new elevation tile arrives during loading.
            auto it = _meshCache.find(tileId);
            if (it == _meshCache.end() || it->second.grid != grid || it->second.exaggeration != exaggeration || it->second.gridSize != gridSize) {
                if (_meshCache.size() >= MAX_CACHED_MESHES) {
                    _meshCache.clear(); // simple full flush; meshes are cheap to rebuild
                    it = _meshCache.end();
                }
                MeshCacheEntry entry;
                entry.grid = grid;
                entry.exaggeration = exaggeration;
                entry.gridSize = gridSize;
                entry.mesh = buildTileMesh(tile, grid, elevationManager, gridSize);
                it = _meshCache.insert_or_assign(tileId, std::move(entry)).first;
            }
            const std::shared_ptr<TileMesh>& mesh = it->second.mesh;
            if (!mesh || mesh->indices.empty()) {
                continue;
            }

            cglib::mat4x4<float> tileMVPMat = cglib::mat4x4<float>::convert(mvpMat * calculateTileMatrix(tile));
            glUniformMatrix4fv(uMVPMat, 1, GL_FALSE, tileMVPMat.data());
            glVertexAttribPointer(aCoord, 3, GL_FLOAT, GL_FALSE, 0, mesh->vertices.data());
            glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(mesh->indices.size()), GL_UNSIGNED_SHORT, mesh->indices.data());
        }

        glDisableVertexAttribArray(aCoord);
        return true;
    }

    void TerrainRenderer::calculateVisibleTiles(const ViewState& viewState, const std::shared_ptr<ElevationManager>& elevationManager, const MapTile& tile, std::vector<MapTile>& tiles) const {
        if (tile.getZoom() > Const::MAX_SUPPORTED_ZOOM_LEVEL) {
            return;
        }

        // Tile bounds in internal coordinates (same convention as DefaultTileTransformer)
        int tileMask = (1 << tile.getZoom()) - 1;
        double zoomScale = 1.0 / (1 << tile.getZoom());
        double minX = (tile.getX() * zoomScale - 0.5) * Const::WORLD_SIZE;
        double minY = ((tileMask - tile.getY()) * zoomScale - 0.5) * Const::WORLD_SIZE;
        double size = zoomScale * Const::WORLD_SIZE;
        double minZ = 0, maxZ = 0;
        elevationManager->getMinMaxDisplayHeight(tile, minZ, maxZ);
        cglib::bbox3<double> tileBounds(cglib::vec3<double>(minX, minY, minZ), cglib::vec3<double>(minX + size, minY + size, maxZ));

        if (!viewState.getFrustum().inside(tileBounds)) {
            return;
        }

        // Same distance-based subdivision criterion as TileLayer::calculateVisibleTilesRecursive.
        // Like there, the LOD center is at surface level so decisions are stable while
        // elevation data streams in.
        cglib::vec3<double> lodCenter(minX + size * 0.5, minY + size * 0.5, 0);
        const cglib::mat4x4<double>& mvpMat = viewState.getModelviewProjectionMat();
        double tileW = lodCenter(0) * mvpMat(3, 0) + lodCenter(1) * mvpMat(3, 1) + lodCenter(2) * mvpMat(3, 2) + mvpMat(3, 3);
        double zoomDistance = tileW * std::pow(2.0, static_cast<double>(tile.getZoom()));
        bool subDivide = zoomDistance < Const::WORLD_SIZE * Const::SQRT_2;

        // No point in subdividing beyond the resolution of the elevation data + mesh grid
        int maxUsefulZoom = Const::MAX_SUPPORTED_ZOOM_LEVEL;
        if (std::shared_ptr<TileDataSource> dataSource = elevationManager->getDataSource()) {
            maxUsefulZoom = dataSource->getMaxZoom() + 3;
        }
        int targetTileZoom = std::min(maxUsefulZoom, static_cast<int>(viewState.getZoom() + 0.001f));
        if (targetTileZoom <= tile.getZoom()) {
            subDivide = false;
        }

        if (subDivide) {
            for (int n = 0; n < 4; n++) {
                calculateVisibleTiles(viewState, elevationManager, tile.getChild(n), tiles);
            }
        } else {
            tiles.push_back(tile);
        }
    }

    int TerrainRenderer::calculateMeshGridSize(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, int meshResolution) const {
        if (!grid || grid->getMaxHeight() - grid->getMinHeight() <= 0) {
            return 1;
        }

        // The pre-pass mesh must never be FINER than the draped tile surfaces: a coarser
        // draped surface would linearly cut through ridges of a finer pre-pass mesh and
        // fail the depth test (see-through holes in the terrain). The draped surfaces are
        // tesselated to min(meshResolution, elevation texels per tile) cells per tile edge,
        // so the pre-pass uses the same bound and only gets coarser with distance (a
        // coarser pre-pass merely weakens ridge occlusion, which is safe).
        double tileSize = Const::WORLD_SIZE / (1 << tile.getZoom());
        double gridWidth = grid->getInternalBounds().getMax().getX() - grid->getInternalBounds().getMin().getX();
        int texelsPerTile = MAX_MESH_GRID_SIZE;
        if (gridWidth > 0) {
            texelsPerTile = static_cast<int>(grid->getWidth() * tileSize / gridWidth + 0.5);
        }
        // No distance-based coarsening: a pre-pass mesh coarser than the draped surfaces
        // smooths out ridges in the depth buffer and lets geometry behind them show
        // through (weak occlusion, visible especially at low zoom levels/grazing angles).
        int gridSize = std::min(std::min(texelsPerTile, meshResolution), MAX_MESH_GRID_SIZE);
        return std::max(gridSize, MIN_MESH_GRID_SIZE);
    }

    std::shared_ptr<TerrainRenderer::TileMesh> TerrainRenderer::buildTileMesh(const MapTile& tile, const std::shared_ptr<ElevationTileGrid>& grid, const std::shared_ptr<ElevationManager>& elevationManager, int gridSize) const {
        auto mesh = std::make_shared<TileMesh>();

        int tileMask = (1 << tile.getZoom()) - 1;
        double zoomScale = 1.0 / (1 << tile.getZoom());
        double originX = (tile.getX() * zoomScale - 0.5) * Const::WORLD_SIZE;
        double originY = ((tileMask - tile.getY()) * zoomScale - 0.5) * Const::WORLD_SIZE;
        double size = zoomScale * Const::WORLD_SIZE;
        double localFromInternal = 1.0 / size;

        float exaggeration = elevationManager->getExaggeration();

        gridSize = std::max(1, gridSize);
        int rowSize = gridSize + 1;

        mesh->vertices.reserve((rowSize * rowSize + 8 * rowSize) * 3); // grid + skirt vertices
        double minLocalZ = 0;
        for (int gy = 0; gy <= gridSize; gy++) {
            for (int gx = 0; gx <= gridSize; gx++) {
                double x = static_cast<double>(gx) / gridSize;
                double y = static_cast<double>(gy) / gridSize;
                double internalX = originX + x * size;
                double internalY = originY + y * size;
                double localZ = 0;
                if (grid) {
                    double meters = grid->sampleHeight(internalX, internalY);
                    localZ = meters * exaggeration * elevationManager->getDisplayScale(internalY) * localFromInternal;
                }
                minLocalZ = std::min(minLocalZ, localZ);
                mesh->vertices.push_back(static_cast<float>(x));
                mesh->vertices.push_back(static_cast<float>(y));
                mesh->vertices.push_back(static_cast<float>(localZ));
            }
        }

        mesh->indices.reserve(gridSize * gridSize * 6 + gridSize * 4 * 6);
        for (int gy = 0; gy < gridSize; gy++) {
            for (int gx = 0; gx < gridSize; gx++) {
                unsigned short i00 = static_cast<unsigned short>(gy * rowSize + gx);
                unsigned short i10 = i00 + 1;
                unsigned short i01 = static_cast<unsigned short>((gy + 1) * rowSize + gx);
                unsigned short i11 = i01 + 1;
                mesh->indices.insert(mesh->indices.end(), { i00, i10, i11, i00, i11, i01 });
            }
        }

        // Skirts: extrude the tile edges downwards to cover cracks between neighboring
        // tiles of different resolutions in the depth buffer.
        if (grid) {
            double skirtZ = minLocalZ - 0.05;
            auto addSkirt = [&](const std::vector<unsigned short>& edge, bool flip) {
                for (std::size_t i = 0; i + 1 < edge.size(); i++) {
                    unsigned short i0 = edge[i];
                    unsigned short i1 = edge[i + 1];
                    unsigned short s0 = static_cast<unsigned short>(mesh->vertices.size() / 3);
                    for (unsigned short idx : { i0, i1 }) {
                        mesh->vertices.push_back(mesh->vertices[idx * 3 + 0]);
                        mesh->vertices.push_back(mesh->vertices[idx * 3 + 1]);
                        mesh->vertices.push_back(static_cast<float>(skirtZ));
                    }
                    if (flip) {
                        mesh->indices.insert(mesh->indices.end(), { i0, s0, static_cast<unsigned short>(s0 + 1), i0, static_cast<unsigned short>(s0 + 1), i1 });
                    } else {
                        mesh->indices.insert(mesh->indices.end(), { i0, static_cast<unsigned short>(s0 + 1), s0, i0, i1, static_cast<unsigned short>(s0 + 1) });
                    }
                }
            };
            std::vector<unsigned short> south, north, west, east;
            for (int g = 0; g <= gridSize; g++) {
                south.push_back(static_cast<unsigned short>(g));
                north.push_back(static_cast<unsigned short>(gridSize * rowSize + g));
                west.push_back(static_cast<unsigned short>(g * rowSize));
                east.push_back(static_cast<unsigned short>(g * rowSize + gridSize));
            }
            addSkirt(south, false);
            addSkirt(north, true);
            addSkirt(west, true);
            addSkirt(east, false);
        }

        return mesh;
    }

    cglib::mat4x4<double> TerrainRenderer::calculateTileMatrix(const MapTile& tile) const {
        int tileMask = (1 << tile.getZoom()) - 1;
        double zoomScale = 1.0 / (1 << tile.getZoom());
        double s = zoomScale * Const::WORLD_SIZE;
        cglib::mat4x4<double> m = cglib::mat4x4<double>::zero();
        m(0, 0) = s;
        m(1, 1) = s;
        m(2, 2) = s;
        m(0, 3) = (tile.getX() * zoomScale - 0.5) * Const::WORLD_SIZE;
        m(1, 3) = ((tileMask - tile.getY()) * zoomScale - 0.5) * Const::WORLD_SIZE;
        m(2, 3) = 0;
        m(3, 3) = 1;
        return m;
    }

    const std::string TerrainRenderer::TERRAIN_DEPTH_VERTEX_SHADER = R"GLSL(
        #version 100
        attribute vec3 a_coord;
        uniform mat4 u_mvpMat;
        uniform float u_far;
        varying float v_depth;
        void main() {
            vec4 pos = u_mvpMat * vec4(a_coord, 1.0);
            v_depth = pos.w / u_far;
            gl_Position = pos;
        }
    )GLSL";

    const std::string TerrainRenderer::TERRAIN_DEPTH_FRAGMENT_SHADER = R"GLSL(
        #version 100
        #ifdef GL_FRAGMENT_PRECISION_HIGH
        precision highp float;
        #else
        precision mediump float;
        #endif
        varying float v_depth;
        void main() {
            float depth = clamp(v_depth, 0.0, 1.0);
            vec3 enc = vec3(1.0, 255.0, 65025.0) * depth;
            enc = fract(enc);
            enc -= enc.yzz * vec3(1.0 / 255.0, 1.0 / 255.0, 0.0);
            gl_FragColor = vec4(enc, 1.0);
        }
    )GLSL";
}
