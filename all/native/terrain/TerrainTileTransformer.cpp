#include "TerrainTileTransformer.h"
#include "core/MapTile.h"
#include "terrain/ElevationManager.h"
#include "terrain/ElevationTileGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace carto {

    TerrainTileTransformer::TerrainVertexTransformer::TerrainVertexTransformer(const vt::TileId& tileId, double scale, std::shared_ptr<ElevationTileGrid> grid, float exaggeration, float divideThreshold) :
        _tileId(tileId),
        _scale(scale),
        _grid(std::move(grid)),
        _exaggeration(exaggeration),
        _divideThreshold(divideThreshold)
    {
        int tileMask = (1 << tileId.zoom) - 1;
        double zoomScale = 1.0 / (1 << tileId.zoom);
        _tileOffsetInternal = cglib::vec2<double>((tileId.x * zoomScale - 0.5) * _scale, ((tileMask - tileId.y) * zoomScale - 0.5) * _scale);
        _tileScaleInternal = zoomScale * _scale;
        _tileScaleMeters = EARTH_CIRCUMFERENCE * zoomScale;
        _localFromInternal = (1 << tileId.zoom) / _scale;
    }

    cglib::vec3<float> TerrainTileTransformer::TerrainVertexTransformer::calculatePoint(const cglib::vec2<float>& pos) const {
        return cglib::vec3<float>(pos(0), 1 - pos(1), static_cast<float>(calculateLocalHeight(pos)));
    }

    cglib::vec3<float> TerrainTileTransformer::TerrainVertexTransformer::calculateNormal(const cglib::vec2<float>& pos) const {
        // Keep 'up' as the normal: it is the extrusion direction for 3D geometry (buildings must
        // stay vertical) and keeps hillshade/lighting behavior identical to the flat planar case.
        return cglib::vec3<float>(0, 0, 1);
    }

    cglib::vec3<float> TerrainTileTransformer::TerrainVertexTransformer::calculateVector(const cglib::vec2<float>& pos, const cglib::vec2<float>& vec) const {
        return cglib::vec3<float>(vec(0), -vec(1), 0);
    }

    cglib::vec2<float> TerrainTileTransformer::TerrainVertexTransformer::calculateTilePosition(const cglib::vec3<float>& pos) const {
        return cglib::vec2<float>(pos(0), 1 - pos(1));
    }

    float TerrainTileTransformer::TerrainVertexTransformer::calculateHeight(const cglib::vec2<float>& pos, float height) const {
        double internalY = _tileOffsetInternal(1) + (1 - pos(1)) * _tileScaleInternal;
        double cosLatitude = calculateMercatorCosine(internalY);
        return static_cast<float>(height / cosLatitude * (1 << _tileId.zoom) / EARTH_CIRCUMFERENCE);
    }

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateLineString(const cglib::vec2<float>* points, std::size_t count, vt::VertexArray<cglib::vec2<float>>& tesselatedPoints) const {
        if (count > 0) {
            tesselatedPoints.append(points[0]);
            for (std::size_t i = 0; i + 1 < count; i++) {
                const cglib::vec2<float>& pos0 = points[i + 0];
                const cglib::vec2<float>& pos1 = points[i + 1];
                float dist = cglib::length(pos1 - pos0) * static_cast<float>(_tileScaleMeters);
                tesselateSegment(pos0, pos1, dist, tesselatedPoints);
            }
        }
    }

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateTriangles(const std::size_t* indices, std::size_t count, vt::VertexArray<cglib::vec2<float>>& coords, vt::VertexArray<cglib::vec2<float>>& texCoords, vt::VertexArray<std::size_t>& tesselatedIndices) const {
        for (std::size_t i = 0; i + 2 < count; i += 3) {
            std::size_t i0 = indices[i + 0];
            std::size_t i1 = indices[i + 1];
            std::size_t i2 = indices[i + 2];
            float dist01 = cglib::length(coords[i1] - coords[i0]) * static_cast<float>(_tileScaleMeters);
            float dist02 = cglib::length(coords[i2] - coords[i0]) * static_cast<float>(_tileScaleMeters);
            float dist12 = cglib::length(coords[i2] - coords[i1]) * static_cast<float>(_tileScaleMeters);
            tesselateTriangle(i0, i1, i2, dist01, dist02, dist12, coords, texCoords, tesselatedIndices);
        }
    }

    double TerrainTileTransformer::TerrainVertexTransformer::calculateLocalHeight(const cglib::vec2<float>& pos) const {
        // Tile geometry is built FLAT: the GPU draping shader replaces the z of every
        // draped vertex with the shared elevation texture sample, so sampling heights at
        // build time would be wasted work (this was by far the most expensive part of
        // terrain tile decodes and surface builds). Label anchors get their heights
        // dynamically (GLTileRenderer label elevation provider), and hit test rays are
        // pre-intersected with the terrain by the host renderer.
        return 0.0;
    }

    double TerrainTileTransformer::TerrainVertexTransformer::calculateMercatorCosine(double internalY) const {
        double sin = std::tanh(internalY * 2 * PI / _scale);
        return std::sqrt(std::max(1.0e-6, 1.0 - sin * sin));
    }

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateSegment(const cglib::vec2<float>& pos0, const cglib::vec2<float>& pos1, float dist, vt::VertexArray<cglib::vec2<float>>& points) const {
        if (dist > _divideThreshold) {
            cglib::vec2<float> posM = (pos0 + pos1) * 0.5f;
            tesselateSegment(pos0, posM, dist * 0.5f, points);
            tesselateSegment(posM, pos1, dist * 0.5f, points);
        }
        else {
            points.append(pos1);
        }
    }

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateTriangle(std::size_t i0, std::size_t i1, std::size_t i2, float dist01, float dist02, float dist12, vt::VertexArray<cglib::vec2<float>>& coords, vt::VertexArray<cglib::vec2<float>>& texCoords, vt::VertexArray<std::size_t>& indices) const {
        // Red-green refinement with an EDGE-LOCAL split rule: an edge is split at its
        // midpoint if and only if IT is longer than the threshold. Both triangles sharing
        // an edge therefore always make the same decision and the tesselation contains no
        // T-vertices. This matters because the vertices are displaced (on the GPU) by
        // sampled terrain heights: a T-vertex displaces to its sampled height while the
        // neighbouring triangle's unsplit edge crosses that point at the interpolated
        // height, opening background-colored cracks all over rugged terrain (the
        // long-standing 'white triangles when zooming out' artifact).
        bool split01 = dist01 > _divideThreshold;
        bool split02 = dist02 > _divideThreshold;
        bool split12 = dist12 > _divideThreshold;
        if (!split01 && !split02 && !split12) {
            indices.append(i0, i1, i2);
            return;
        }

        auto splitEdge = [&](std::size_t ia, std::size_t ib) -> std::size_t {
            std::size_t iM = coords.size();
            coords.append((coords[ia] + coords[ib]) * 0.5f);
            if (!texCoords.empty()) {
                texCoords.append((texCoords[ia] + texCoords[ib]) * 0.5f);
            }
            return iM;
        };
        auto edgeDist = [&](std::size_t ia, std::size_t ib) -> float {
            return cglib::length(coords[ib] - coords[ia]) * static_cast<float>(_tileScaleMeters);
        };

        if (split01 && split02 && split12) {
            // regular 1-to-4 split; the midsegments are exactly half the opposite edges
            std::size_t m01 = splitEdge(i0, i1);
            std::size_t m02 = splitEdge(i0, i2);
            std::size_t m12 = splitEdge(i1, i2);
            tesselateTriangle(i0, m01, m02, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(m01, i1, m12, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(m02, m12, i2, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(m01, m12, m02, dist02 * 0.5f, dist12 * 0.5f, dist01 * 0.5f, coords, texCoords, indices);
        }
        else if (split01 && split02) {
            std::size_t m01 = splitEdge(i0, i1);
            std::size_t m02 = splitEdge(i0, i2);
            float distM01_2 = edgeDist(m01, i2);
            tesselateTriangle(i0, m01, m02, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(m01, i1, i2, dist01 * 0.5f, distM01_2, dist12, coords, texCoords, indices);
            tesselateTriangle(m01, i2, m02, distM01_2, dist12 * 0.5f, dist02 * 0.5f, coords, texCoords, indices);
        }
        else if (split01 && split12) {
            std::size_t m01 = splitEdge(i0, i1);
            std::size_t m12 = splitEdge(i1, i2);
            float distM01_2 = edgeDist(m01, i2);
            tesselateTriangle(m01, i1, m12, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(i0, m01, i2, dist01 * 0.5f, dist02, distM01_2, coords, texCoords, indices);
            tesselateTriangle(m01, m12, i2, dist02 * 0.5f, distM01_2, dist12 * 0.5f, coords, texCoords, indices);
        }
        else if (split02 && split12) {
            std::size_t m02 = splitEdge(i0, i2);
            std::size_t m12 = splitEdge(i1, i2);
            float distM02_1 = edgeDist(m02, i1);
            tesselateTriangle(i0, i1, m02, dist01, dist02 * 0.5f, distM02_1, coords, texCoords, indices);
            tesselateTriangle(i1, m12, m02, dist12 * 0.5f, distM02_1, dist01 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(m02, m12, i2, dist01 * 0.5f, dist02 * 0.5f, dist12 * 0.5f, coords, texCoords, indices);
        }
        else if (split01) {
            std::size_t m01 = splitEdge(i0, i1);
            float distM = edgeDist(m01, i2);
            tesselateTriangle(i2, i0, m01, dist02, distM, dist01 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(i1, i2, m01, dist12, dist01 * 0.5f, distM, coords, texCoords, indices);
        }
        else if (split02) {
            std::size_t m02 = splitEdge(i0, i2);
            float distM = edgeDist(m02, i1);
            tesselateTriangle(i0, i1, m02, dist01, dist02 * 0.5f, distM, coords, texCoords, indices);
            tesselateTriangle(i1, i2, m02, dist12, distM, dist02 * 0.5f, coords, texCoords, indices);
        }
        else {
            std::size_t m12 = splitEdge(i1, i2);
            float distM = edgeDist(m12, i0);
            tesselateTriangle(i0, i1, m12, dist01, distM, dist12 * 0.5f, coords, texCoords, indices);
            tesselateTriangle(i2, i0, m12, dist02, dist12 * 0.5f, distM, coords, texCoords, indices);
        }
    }

    TerrainTileTransformer::TerrainTileTransformer(float scale, const std::shared_ptr<ElevationManager>& elevationManager, int meshResolution, int minZoom) :
        _scale(scale),
        _elevationManager(elevationManager),
        _meshResolution(std::max(1, meshResolution)),
        _minZoom(minZoom)
    {
    }

    cglib::vec3<double> TerrainTileTransformer::calculateTileOrigin(const vt::TileId& tileId) const {
        int tileMask = (1 << tileId.zoom) - 1;
        double zoomScale = 1.0 / (1 << tileId.zoom);
        cglib::vec3<double> p;
        p(0) = (tileId.x * zoomScale - 0.5) * _scale;
        p(1) = ((tileMask - tileId.y) * zoomScale - 0.5) * _scale;
        p(2) = 0;
        return p;
    }

    cglib::bbox3<double> TerrainTileTransformer::calculateTileBBox(const vt::TileId& tileId) const {
        cglib::bbox3<double> bbox = cglib::transform_bbox(cglib::bbox3<double>(cglib::vec3<double>(0, 0, 0), cglib::vec3<double>(1, 1, 0)), calculateTileMatrix(tileId, 1.0f));
        if (tileId.zoom >= _minZoom) {
            int tileMask = (1 << tileId.zoom) - 1;
            MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
            double minZ = 0, maxZ = 0;
            _elevationManager->getMinMaxDisplayHeight(mapTile, minZ, maxZ);
            bbox.add(cglib::vec3<double>(bbox.min(0), bbox.min(1), minZ));
            bbox.add(cglib::vec3<double>(bbox.max(0), bbox.max(1), maxZ));
        }
        return bbox;
    }

    cglib::mat4x4<double> TerrainTileTransformer::calculateTileMatrix(const vt::TileId& tileId, float coordScale) const {
        double s = _scale * coordScale / (1 << tileId.zoom);
        cglib::vec3<double> p = calculateTileOrigin(tileId);

        cglib::mat4x4<double> m = cglib::mat4x4<double>::zero();
        m(0, 0) = s;
        m(1, 1) = s;
        m(2, 2) = s;
        m(0, 3) = p(0);
        m(1, 3) = p(1);
        m(2, 3) = p(2);
        m(3, 3) = 1;
        return m;
    }

    cglib::mat4x4<float> TerrainTileTransformer::calculateTileTransform(const vt::TileId& tileId, const cglib::vec2<float>& translate, float coordScale) const {
        return cglib::translate4_matrix(cglib::vec3<float>(translate(0) / coordScale, -translate(1) / coordScale, 0));
    }

    std::shared_ptr<const vt::TileTransformer::VertexTransformer> TerrainTileTransformer::createTileVertexTransformer(const vt::TileId& tileId) const {
        std::shared_ptr<ElevationTileGrid> grid;
        if (tileId.zoom >= _minZoom) {
            int tileMask = (1 << tileId.zoom) - 1;
            MapTile mapTile(tileId.x & tileMask, std::min(std::max(tileId.y, 0), tileMask), tileId.zoom, 0);
            grid = _elevationManager->getTileGrid(mapTile, ElevationManager::LoadMode::CACHED_ONLY);
        }

        float divideThreshold = std::numeric_limits<float>::infinity();
        if (grid && grid->getMaxHeight() - grid->getMinHeight() > FLAT_HEIGHT_RANGE_EPSILON) {
            double tileScaleMeters = EARTH_CIRCUMFERENCE / (1 << tileId.zoom);
            double threshold = tileScaleMeters / _meshResolution;

            // No point in subdividing finer than the elevation grid resolution
            double gridInternalWidth = grid->getInternalBounds().getMax().getX() - grid->getInternalBounds().getMin().getX();
            double demTexelMeters = gridInternalWidth / grid->getWidth() * EARTH_CIRCUMFERENCE / _scale;
            divideThreshold = static_cast<float>(std::max(threshold, demTexelMeters));
        }

        return std::make_shared<TerrainVertexTransformer>(tileId, _scale, std::move(grid), _elevationManager->getExaggeration(), divideThreshold);
    }
}
