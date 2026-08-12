#include "TerrainTileTransformer.h"
#include "core/MapTile.h"
#include "terrain/ElevationManager.h"
#include "terrain/ElevationTileGrid.h"

#include <algorithm>
#include <cmath>
#include <limits>

#ifdef __ANDROID__
#include <sys/system_properties.h>
#endif

namespace carto {

    // Measurement switch for what AREA subdivision has to cost. Fills subdivide to exactly one
    // surface grid cell so every sub-vertex lands on the grid; this multiplies that cell size, so
    // indices fall as 1/N^2 while the chord error grows as N^2. Tangram has no constant to copy
    // here - they do not subdivide at all - so the usable value is whatever the depth budget can
    // still clear, and that is a measurement, not a derivation.
    // Measured on device, north pan into the terrain, 45.244172/5.760595 z13.2:
    //   1 cell = 16.6 fps and 158k geometry indices per render tile
    //   2 cells = 20.6 fps and 48k      <- shipped
    //   4 cells = 21.2 fps and 19k
    // Two cells takes most of the frame rate back for half the chord error of four, and at
    // 45.244172/5.760595 z13.2 t26 neither shows the floating-fill patches that source density
    // does - the depth budget clears what is left. Four was clean too at that camera and is one
    // setprop away if the frame ever needs it.
    //   adb shell setprop debug.carto.areathreshold 4
    static constexpr float AREA_THRESHOLD_CELLS = 2.0f;
#ifdef __ANDROID__
    // The same measurement switch for LINES. Lines are the expensive half over a city - the fills
    // are draped and baked once, the lines are drawn as terrain geometry every frame - and their
    // threshold is a fraction of the mesh cell whatever relief the tile actually has.
    //   adb shell setprop debug.carto.linethreshold 4
    // Relief (metres of height range in the tile) under which the LATTICE split is skipped. The
    // split exists to stop a segment chording across a surface cell's anti-diagonal fold; the fold
    // is a fraction of the tile's relief, so on a valley floor it protects against nothing and
    // still cuts every line at every cell edge and diagonal. 0 = shipped behaviour (always split).
    //   adb shell setprop debug.carto.latticerelief 50
    static float latticeReliefThreshold() {
        static const float relief = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.latticerelief", property) > 0) {
                float value = static_cast<float>(std::atof(property));
                if (value >= 0.0f) {
                    return value;
                }
            }
            return 0.0f;
        }();
        return relief;
    }

    static float lineThresholdScale() {
        static const float scale = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.linethreshold", property) > 0) {
                float value = static_cast<float>(std::atof(property));
                if (value > 0.0f) {
                    return value;
                }
            }
            return 1.0f;
        }();
        return scale;
    }

    static float areaThresholdScale() {
        static const float scale = [] {
            char property[PROP_VALUE_MAX] = { 0 };
            if (__system_property_get("debug.carto.areathreshold", property) > 0) {
                float value = static_cast<float>(std::atof(property));
                if (value > 0.0f) {
                    return value;
                }
            }
            return AREA_THRESHOLD_CELLS;
        }();
        return scale;
    }
#else
    static float latticeReliefThreshold() {
        return 0.0f;
    }

    static float lineThresholdScale() {
        return 1.0f;
    }

    static float areaThresholdScale() {
        return AREA_THRESHOLD_CELLS;
    }
#endif

    TerrainTileTransformer::TerrainVertexTransformer::TerrainVertexTransformer(const vt::TileId& tileId, double scale, std::shared_ptr<ElevationTileGrid> grid, float exaggeration, float divideThreshold, float lineDivideThreshold, float latticeCell) :
        _tileId(tileId),
        _scale(scale),
        _grid(std::move(grid)),
        _exaggeration(exaggeration),
        _divideThreshold(divideThreshold),
        _lineDivideThreshold(lineDivideThreshold),
        _latticeCell(latticeCell)
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
                // Regular-grid mode: cut the segment exactly where it leaves a surface triangle
                // instead of halving it until it is small enough to hide the error. Every
                // sub-segment then lies IN a triangle of the surface, so it follows the surface
                // exactly rather than approximately - with fewer vertices than the fraction-of-a-cell
                // halving needed to keep the chord sag under the (zero) painter-order depth slack.
                if (_latticeCell > 0 && tesselateSegmentOnLattice(pos0, pos1, tesselatedPoints)) {
                    continue;
                }
                float dist = cglib::length(pos1 - pos0) * static_cast<float>(_tileScaleMeters);
                tesselateSegment(pos0, pos1, dist, _lineDivideThreshold, tesselatedPoints);
            }
        }
    }

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateLabelLineString(const cglib::vec2<float>* points, std::size_t count, vt::VertexArray<cglib::vec2<float>>& tesselatedPoints) const {
        // A label line is READ, never drawn: the lattice split keeps a DRAWN segment inside one
        // surface triangle, which buys a glyph run nothing, and neither does the finer line
        // threshold - the profile a run follows cannot carry more detail than the surface it is
        // laid on. Halve to the SURFACE cell instead. Every vertex dropped here is an elevation
        // sample dropped from every terrain re-anchor, which is the most expensive thing on the
        // render thread over 3D terrain (docs/rendering/06-labels.md). Measured: with no line
        // subdivision at all, 'prepare' goes 154 -> 68 ms on the north pan.
        if (count > 0) {
            tesselatedPoints.append(points[0]);
            for (std::size_t i = 0; i + 1 < count; i++) {
                const cglib::vec2<float>& pos0 = points[i + 0];
                const cglib::vec2<float>& pos1 = points[i + 1];
                float dist = cglib::length(pos1 - pos0) * static_cast<float>(_tileScaleMeters);
                tesselateSegment(pos0, pos1, dist, _divideThreshold, tesselatedPoints);
            }
        }
    }

    bool TerrainTileTransformer::TerrainVertexTransformer::tesselateSegmentOnLattice(const cglib::vec2<float>& pos0, const cglib::vec2<float>& pos1, vt::VertexArray<cglib::vec2<float>>& points) const {
        // The surface is a regular grid of _latticeCell cells, each split into two triangles.
        // The shader folds a cell along fg.x + fg.y = 1 in ELEVATION-UV space; these points are
        // in tile (u, v) space, and the surface builder emits its vertices at y = 1 - v, so the
        // same fold reads as u + v = const here. A segment therefore stays inside one triangle
        // as long as it crosses none of x = k*cell, y = k*cell, x + y = k*cell.
        const cglib::vec2<float> delta = pos1 - pos0;
        const float cell = _latticeCell;
        const float f0[3] = { pos0(0), pos0(1), pos0(0) + pos0(1) };
        const float f1[3] = { pos1(0), pos1(1), pos1(0) + pos1(1) };

        float ts[3 * MAX_LATTICE_SPLITS_PER_SEGMENT];
        std::size_t tCount = 0;
        for (int axis = 0; axis < 3; axis++) {
            float d = f1[axis] - f0[axis];
            if (std::abs(d) < 1.0e-9f) {
                continue;
            }
            float from = std::min(f0[axis], f1[axis]);
            float to = std::max(f0[axis], f1[axis]);
            double firstK = std::floor(from / cell) + 1;
            double lastK = std::ceil(to / cell) - 1;
            if (lastK - firstK + 1 > MAX_LATTICE_SPLITS_PER_SEGMENT) {
                return false; // spans too many cells: not worth enumerating
            }
            for (double k = firstK; k <= lastK; k += 1) {
                float t = (static_cast<float>(k * cell) - f0[axis]) / d;
                if (t > 1.0e-5f && t < 1.0f - 1.0e-5f) {
                    if (tCount >= sizeof(ts) / sizeof(ts[0])) {
                        return false;
                    }
                    ts[tCount++] = t;
                }
            }
        }

        std::sort(ts, ts + tCount);
        float prevT = 0.0f;
        for (std::size_t i = 0; i < tCount; i++) {
            if (ts[i] - prevT < 1.0e-5f) {
                continue; // the segment passes through a lattice node: one point, not three
            }
            points.append(pos0 + delta * ts[i]);
            prevT = ts[i];
        }
        points.append(pos1);
        return true;
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

    void TerrainTileTransformer::TerrainVertexTransformer::tesselateSegment(const cglib::vec2<float>& pos0, const cglib::vec2<float>& pos1, float dist, float threshold, vt::VertexArray<cglib::vec2<float>>& points) const {
        if (dist > threshold) {
            cglib::vec2<float> posM = (pos0 + pos1) * 0.5f;
            tesselateSegment(pos0, posM, dist * 0.5f, threshold, points);
            tesselateSegment(posM, pos1, dist * 0.5f, threshold, points);
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

    TerrainTileTransformer::TerrainTileTransformer(float scale, const std::shared_ptr<ElevationManager>& elevationManager, int meshResolution, int minZoom, bool regularGrid, bool sourceDensity, bool sourceDensityLines) :
        _scale(scale),
        _elevationManager(elevationManager),
        _meshResolution(std::max(1, meshResolution)),
        _minZoom(minZoom),
        _regularGrid(regularGrid),
        _sourceDensity(sourceDensity),
        _sourceDensityLines(sourceDensityLines)
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
        float lineDivideThreshold = std::numeric_limits<float>::infinity();
        float latticeCell = 0.0f;
        if (grid && grid->getMaxHeight() - grid->getMinHeight() > FLAT_HEIGHT_RANGE_EPSILON) {
            double tileScaleMeters = EARTH_CIRCUMFERENCE / (1 << tileId.zoom);
            double threshold = tileScaleMeters / _meshResolution;

            if (_regularGrid) {
                // Regular-grid surface mode: the reference surface is a shared grid of
                // _meshResolution cells built in the renderer, and it is used as a depth
                // pre-pass occluder. Draped geometry must therefore still follow that
                // surface: a fill left at its source density would be a few large flat
                // triangles that sag below the bulging grid surface over convex terrain and
                // get depth-occluded (landcover holes). Subdivide to exactly one grid cell
                // so every sub-vertex lattice-clamps onto the grid surface; the shared grid
                // (no per-tile red-green tesselation) and the lattice clamp are the wins.
                // Do NOT clamp to the DEM texel size here: the grid, not the DEM, is the
                // surface geometry the draped content is tested against.
                //
                // One cell puts every draped VERTEX on the grid surface (lattice clamp), but
                // the straight SEGMENT between two vertices in different cell triangles chords
                // across the cell's anti-diagonal fold and sags below it. Under painter-order
                // (zero depth slack) that sag is depth-occluded -> draped LINES crack over
                // convex terrain, worst at low zoom where the cell / fold amplitude is largest.
                // Raising _meshResolution shrinks the fold but costs O(res^2) in the shared grid
                // AND subdivides fills the same amount. Instead subdivide only the LINES finer
                // than the grid (cheap, 1D); the sag falls linearly with the sub-segment length
                // at no surface-grid cost. Fills stay at one cell (their sag is bounded there).
                //
                // Source-density (tangram) mode targets the expensive part - the fills (a full
                // tile fill red-green splits to ~meshResolution^2 triangles). It skips FILL
                // subdivision (source density, lifted by a per-draw fill slack in the renderer)
                // but KEEPS line subdivision: lines are cheap (1D) and MUST follow the terrain
                // closely (contours lie exactly on the surface - un-subdivided they need a huge
                // lift slack that shines everything through). So only the fill threshold goes to
                // infinity here; the line threshold is unchanged.
                divideThreshold = _sourceDensity ? std::numeric_limits<float>::infinity() : static_cast<float>(threshold * areaThresholdScale());
                // Draped lines are baked flat too, so skip their subdivision as well.
                // Otherwise the lattice split below cuts lines exactly at the surface triangle
                // boundaries, which removes the chord sag entirely - so the threshold only has to
                // bound the segment length at one cell (it is what the lattice split falls back
                // to for segments spanning very many cells).
                lineDivideThreshold = _sourceDensityLines ? std::numeric_limits<float>::infinity() : static_cast<float>(threshold * lineThresholdScale());
                // A tile whose relief cannot fold a cell enough to matter does not need the split
                // (see latticeReliefThreshold): it then falls back to the plain threshold below.
                bool latticeWorthIt = (grid->getMaxHeight() - grid->getMinHeight()) >= latticeReliefThreshold();
                latticeCell = (_sourceDensityLines || !latticeWorthIt) ? 0.0f : static_cast<float>(1.0 / _meshResolution);
            } else {
                // No point in subdividing FILLS finer than the elevation grid resolution
                double gridInternalWidth = grid->getInternalBounds().getMax().getX() - grid->getInternalBounds().getMin().getX();
                double demTexelMeters = gridInternalWidth / grid->getWidth() * EARTH_CIRCUMFERENCE / _scale;
                divideThreshold = static_cast<float>(std::max(threshold, demTexelMeters));
                // Lines are cut finer, and the DEM-texel floor deliberately does NOT apply to them.
                // Without the regular grid there is no lattice to cut against, so a sub-segment one
                // mesh cell long still chords across the cell's diagonal fold and sags below the
                // surface - the same sag the regular-grid branch above describes, and the reason a
                // route reads as sunk into a ridge at low zoom and straightens as you zoom in (the
                // threshold is proportional to the tile). The floor is about how much elevation
                // DETAIL exists; the sag is against the surface MESH, so it is the wrong bound.
                // Lines are 1D, so cutting them finer costs a fraction of what the same factor
                // would cost on a fill.
                lineDivideThreshold = static_cast<float>(threshold / LINE_SUBDIVISION_FACTOR * lineThresholdScale());
            }
        }

        return std::make_shared<TerrainVertexTransformer>(tileId, _scale, std::move(grid), _elevationManager->getExaggeration(), divideThreshold, lineDivideThreshold, latticeCell);
    }
}
