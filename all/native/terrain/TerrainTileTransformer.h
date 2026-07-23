/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINTILETRANSFORMER_H_
#define _CARTO_TERRAINTILETRANSFORMER_H_

#include <memory>

#include <vt/TileTransformer.h>

namespace carto {
    class ElevationManager;
    class ElevationTileGrid;

    /**
     * A planar tile transformer that displaces geometry by terrain elevation.
     * Heights are sampled CPU-side from the decoded elevation grids of the ElevationManager
     * at tile build time. Heights are a pure function of world position (the same DEM data
     * and deterministic parent-fallback rules are used by all tiles), so neighboring tiles at
     * different zoom levels produce matching edge geometry.
     * Line strings and triangles are adaptively subdivided with world-unit thresholds
     * (binary halving) so that subdivision points of different-zoom tiles coincide.
     * Internal class, not exposed in the public API.
     */
    class TerrainTileTransformer final : public vt::TileTransformer {
    public:
        class TerrainVertexTransformer final : public VertexTransformer {
        public:
            TerrainVertexTransformer(const vt::TileId& tileId, double scale, std::shared_ptr<ElevationTileGrid> grid, float exaggeration, float divideThreshold, float lineDivideThreshold);
            virtual ~TerrainVertexTransformer() = default;

            virtual cglib::vec3<float> calculatePoint(const cglib::vec2<float>& pos) const override;
            virtual cglib::vec3<float> calculateNormal(const cglib::vec2<float>& pos) const override;
            virtual cglib::vec3<float> calculateVector(const cglib::vec2<float>& pos, const cglib::vec2<float>& vec) const override;
            virtual cglib::vec2<float> calculateTilePosition(const cglib::vec3<float>& pos) const override;
            virtual float calculateHeight(const cglib::vec2<float>& pos, float height) const override;

            virtual void tesselateLineString(const cglib::vec2<float>* points, std::size_t count, vt::VertexArray<cglib::vec2<float>>& tesselatedPoints) const override;
            virtual void tesselateTriangles(const std::size_t* indices, std::size_t count, vt::VertexArray<cglib::vec2<float>>& coords, vt::VertexArray<cglib::vec2<float>>& texCoords, vt::VertexArray<std::size_t>& tesselatedIndices) const override;

        private:
            double calculateLocalHeight(const cglib::vec2<float>& pos) const;
            double calculateMercatorCosine(double internalY) const;

            void tesselateSegment(const cglib::vec2<float>& pos0, const cglib::vec2<float>& pos1, float dist, vt::VertexArray<cglib::vec2<float>>& points) const;
            void tesselateTriangle(std::size_t i0, std::size_t i1, std::size_t i2, float dist01, float dist02, float dist12, vt::VertexArray<cglib::vec2<float>>& coords, vt::VertexArray<cglib::vec2<float>>& texCoords, vt::VertexArray<std::size_t>& indices) const;

            const vt::TileId _tileId;
            const double _scale;
            const std::shared_ptr<ElevationTileGrid> _grid;
            const float _exaggeration;
            const float _divideThreshold; // triangle subdivision, EPSG3857 meters; infinity disables subdivision
            const float _lineDivideThreshold; // line subdivision, EPSG3857 meters; finer than the triangle threshold in regular-grid mode
            cglib::vec2<double> _tileOffsetInternal; // internal coordinates of the tile origin (min x, min y)
            double _tileScaleInternal;
            double _tileScaleMeters;
            double _localFromInternal;
        };

        TerrainTileTransformer(float scale, const std::shared_ptr<ElevationManager>& elevationManager, int meshResolution, int minZoom, bool regularGrid, bool sourceDensity);
        virtual ~TerrainTileTransformer() = default;

        std::shared_ptr<ElevationManager> getElevationManager() const { return _elevationManager; }
        int getMeshResolution() const { return _meshResolution; }
        int getMinZoom() const { return _minZoom; }

        virtual cglib::vec3<double> calculateTileOrigin(const vt::TileId& tileId) const override;
        virtual cglib::bbox3<double> calculateTileBBox(const vt::TileId& tileId) const override;
        virtual cglib::mat4x4<double> calculateTileMatrix(const vt::TileId& tileId, float coordScale) const override;
        virtual cglib::mat4x4<float> calculateTileTransform(const vt::TileId& tileId, const cglib::vec2<float>& translate, float coordScale) const override;

        virtual std::shared_ptr<const VertexTransformer> createTileVertexTransformer(const vt::TileId& tileId) const override;

    private:
        static constexpr float FLAT_HEIGHT_RANGE_EPSILON = 0.001f;
        // Regular-grid draped LINES are subdivided this fraction of a surface grid cell
        // (< 1 = finer than the grid) so segments stop chording the cell's anti-diagonal fold
        // and cracking under zero-slack painter-order depth. Lower = fewer cracks, more line
        // vertices (lines are 1D, cheap). Triangles stay at one cell (their sag is bounded and
        // subdividing area content 1/factor^2 is expensive). Decoupled from the surface grid.
        static constexpr double REGULAR_GRID_LINE_SUBDIVISION = 0.35;

        const double _scale;
        const std::shared_ptr<ElevationManager> _elevationManager;
        const int _meshResolution;
        const int _minZoom; // tiles below this zoom level are rendered flat
        const bool _regularGrid; // shared regular-grid surface mode: subdivide draped geometry to one grid cell (follow the shared grid surface), no DEM-texel floor
        const bool _sourceDensity; // source-density (tangram) mode: do not subdivide draped content at all; GPU-displace at source density + lifting depth slack
    };
}

#endif
