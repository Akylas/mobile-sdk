#include "ContourTileDataSource.h"
#include "core/BinaryData.h"
#include "core/MapTile.h"
#include "core/MapPos.h"
#include "core/MapBounds.h"
#include "core/Variant.h"
#include "components/Exceptions.h"
#include "graphics/Bitmap.h"
#include "projections/Projection.h"
#include "rastertiles/ElevationDecoder.h"
#include "rastertiles/TerrariumElevationDataDecoder.h"
#include "rastertiles/MapBoxElevationDataDecoder.h"
#include "utils/TileUtils.h"
#include "utils/Log.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <vector>

#include <mbvtbuilder/MBVTTileBuilder.h>

#include <mapnikvt/mbvtpackage/MBVTPackage.pb.h>

namespace {

    using GridPoint = std::pair<double, double>; // (gx, gy) in grid node coordinates
    using Polyline = std::vector<GridPoint>;

    // Linear crossing position of 'level' between corner values va (at ta) and vb (at tb).
    inline double lerpT(float va, float vb, double level) {
        double d = static_cast<double>(vb) - static_cast<double>(va);
        if (d == 0.0) {
            return 0.5;
        }
        double t = (level - va) / d;
        if (t < 0.0) t = 0.0;
        if (t > 1.0) t = 1.0;
        return t;
    }

    // Marching squares over a WxH height grid (row-major, row 0 = south) for a single level.
    // Segments are linked into polylines using shared cell-edge ids (no floating point matching).
    std::vector<Polyline> marchingSquares(const std::vector<float>& heights, int W, int H, double level) {
        // Edge id: horizontal edge (between (x,y) and (x+1,y)) -> (y*W + x)*2 + 0
        //          vertical   edge (between (x,y) and (x,y+1)) -> (y*W + x)*2 + 1
        auto hEdge = [W](int x, int y) -> std::int64_t { return (static_cast<std::int64_t>(y) * W + x) * 2 + 0; };
        auto vEdge = [W](int x, int y) -> std::int64_t { return (static_cast<std::int64_t>(y) * W + x) * 2 + 1; };

        std::unordered_map<std::int64_t, GridPoint> points; // edge id -> crossing point
        // adjacency: edge id -> the (up to two) edge ids it is connected to by a segment
        std::unordered_map<std::int64_t, std::vector<std::int64_t>> adj;

        auto addSegment = [&](std::int64_t ea, GridPoint pa, std::int64_t eb, GridPoint pb) {
            points[ea] = pa;
            points[eb] = pb;
            adj[ea].push_back(eb);
            adj[eb].push_back(ea);
        };

        for (int y = 0; y + 1 < H; y++) {
            for (int x = 0; x + 1 < W; x++) {
                float v00 = heights[static_cast<std::size_t>(y) * W + x];         // SW
                float v10 = heights[static_cast<std::size_t>(y) * W + (x + 1)];   // SE
                float v01 = heights[static_cast<std::size_t>(y + 1) * W + x];     // NW
                float v11 = heights[static_cast<std::size_t>(y + 1) * W + (x + 1)]; // NE

                int idx = 0;
                if (v00 >= level) idx |= 1; // SW
                if (v10 >= level) idx |= 2; // SE
                if (v11 >= level) idx |= 4; // NE
                if (v01 >= level) idx |= 8; // NW
                if (idx == 0 || idx == 15) {
                    continue;
                }

                // Crossing points on the 4 cell edges (only some are used per case).
                std::int64_t eB = hEdge(x, y);       GridPoint pB(x + lerpT(v00, v10, level), y);           // bottom (S)
                std::int64_t eT = hEdge(x, y + 1);   GridPoint pT(x + lerpT(v01, v11, level), y + 1);       // top (N)
                std::int64_t eL = vEdge(x, y);       GridPoint pL(x, y + lerpT(v00, v01, level));           // left (W)
                std::int64_t eR = vEdge(x + 1, y);   GridPoint pR(x + 1, y + lerpT(v10, v11, level));       // right (E)

                switch (idx) {
                    case 1:  case 14: addSegment(eL, pL, eB, pB); break;
                    case 2:  case 13: addSegment(eB, pB, eR, pR); break;
                    case 3:  case 12: addSegment(eL, pL, eR, pR); break;
                    case 4:  case 11: addSegment(eR, pR, eT, pT); break;
                    case 6:  case 9:  addSegment(eB, pB, eT, pT); break;
                    case 7:  case 8:  addSegment(eL, pL, eT, pT); break;
                    case 5: // saddle: two segments (consistent resolution)
                        addSegment(eL, pL, eT, pT);
                        addSegment(eB, pB, eR, pR);
                        break;
                    case 10: // saddle
                        addSegment(eL, pL, eB, pB);
                        addSegment(eR, pR, eT, pT);
                        break;
                    default: break;
                }
            }
        }

        // Link segments into polylines. Walk each unvisited connection, extending from endpoints
        // with degree 1 first (open chains), then remaining loops.
        std::vector<Polyline> polylines;
        std::unordered_map<std::int64_t, std::vector<char>> used; // parallels adj: consumed flags
        for (const auto& e : adj) {
            used[e.first] = std::vector<char>(e.second.size(), 0);
        }

        auto takeEdge = [&](std::int64_t from, std::int64_t to) -> bool {
            auto it = adj.find(from);
            if (it == adj.end()) return false;
            auto& usedVec = used[from];
            for (std::size_t i = 0; i < it->second.size(); i++) {
                if (!usedVec[i] && it->second[i] == to) {
                    usedVec[i] = 1;
                    return true;
                }
            }
            return false;
        };

        auto degree = [&](std::int64_t e) -> int {
            auto it = adj.find(e);
            return it == adj.end() ? 0 : static_cast<int>(it->second.size());
        };

        // Helper: from a starting edge, greedily follow unused connections to build a chain.
        auto buildChain = [&](std::int64_t start) {
            Polyline line;
            std::int64_t cur = start;
            line.push_back(points[cur]);
            while (true) {
                auto it = adj.find(cur);
                if (it == adj.end()) break;
                std::int64_t next = -1;
                auto& usedVec = used[cur];
                for (std::size_t i = 0; i < it->second.size(); i++) {
                    if (!usedVec[i]) {
                        next = it->second[i];
                        usedVec[i] = 1;
                        // consume the reverse edge too
                        takeEdge(next, cur);
                        break;
                    }
                }
                if (next < 0) break;
                line.push_back(points[next]);
                cur = next;
            }
            return line;
        };

        // Open chains first (endpoints on the tile border have degree 1).
        for (const auto& e : adj) {
            if (degree(e.first) == 1) {
                bool anyLeft = false;
                for (char c : used[e.first]) { if (!c) { anyLeft = true; break; } }
                if (!anyLeft) continue;
                Polyline line = buildChain(e.first);
                if (line.size() >= 2) polylines.push_back(std::move(line));
            }
        }
        // Remaining closed loops.
        for (const auto& e : adj) {
            bool anyLeft = false;
            for (char c : used[e.first]) { if (!c) { anyLeft = true; break; } }
            if (!anyLeft) continue;
            Polyline line = buildChain(e.first);
            if (line.size() >= 2) polylines.push_back(std::move(line));
        }

        return polylines;
    }

}

namespace carto {

    ContourTileDataSource::ContourTileDataSource(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder) :
        TileDataSource(),
        _dataSource(dataSource),
        _elevationDecoder(elevationDecoder),
        _baseInterval(10.0f),
        _simplifyTolerance(1.0f),
        _resolution(128),
        _minVisibleZoom(12),
        _seamlessEdges(false),
        _layerName("contour"),
        _mutex(),
        _dataSourceListener()
    {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }
        _dataSourceListener = std::make_shared<DataSourceListener>(*this);
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }

    ContourTileDataSource::ContourTileDataSource(const std::shared_ptr<TileDataSource>& dataSource) :
        ContourTileDataSource(dataSource, std::shared_ptr<ElevationDecoder>())
    {
    }

    ContourTileDataSource::~ContourTileDataSource() {
        _dataSource->unregisterOnChangeListener(_dataSourceListener);
        _dataSourceListener.reset();
    }

    std::string ContourTileDataSource::getLayerName() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _layerName;
    }

    void ContourTileDataSource::setLayerName(const std::string& name) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _layerName = name;
        }
        notifyTilesChanged(false);
    }

    float ContourTileDataSource::getBaseInterval() const {
        return _baseInterval;
    }

    void ContourTileDataSource::setBaseInterval(float interval) {
        if (interval <= 0.0f) {
            throw InvalidArgumentException("Base interval must be positive");
        }
        _baseInterval = interval;
        notifyTilesChanged(false);
    }

    int ContourTileDataSource::getResolution() const {
        return _resolution;
    }

    void ContourTileDataSource::setResolution(int resolution) {
        _resolution = std::max(8, resolution);
        notifyTilesChanged(false);
    }

    int ContourTileDataSource::getMinVisibleZoom() const {
        return _minVisibleZoom;
    }

    void ContourTileDataSource::setMinVisibleZoom(int zoom) {
        _minVisibleZoom = zoom;
        notifyTilesChanged(false);
    }

    bool ContourTileDataSource::isSeamlessEdgesEnabled() const {
        return _seamlessEdges;
    }

    void ContourTileDataSource::setSeamlessEdgesEnabled(bool enabled) {
        _seamlessEdges = enabled;
        notifyTilesChanged(false);
    }

    float ContourTileDataSource::getSimplifyTolerance() const {
        return _simplifyTolerance;
    }

    void ContourTileDataSource::setSimplifyTolerance(float tolerance) {
        _simplifyTolerance = tolerance;
        notifyTilesChanged(false);
    }

    int ContourTileDataSource::getMinZoom() const {
        // Report the DEM's real min zoom rather than clamping up to MinVisibleZoom: if we clamped up, then
        // when the camera is below that zoom the layer would fill the whole viewport with min-zoom tiles (an
        // exponential tile-count blowup as you zoom out). Instead the layer requests few tiles at the camera
        // zoom, and loadTile returns an empty tile cheaply below MinVisibleZoom (no DEM fetch, no tracing).
        return _dataSource->getMinZoom();
    }

    int ContourTileDataSource::getMaxZoom() const {
        return _dataSource->getMaxZoom();
    }

    MapBounds ContourTileDataSource::getDataExtent() const {
        return _dataSource->getDataExtent();
    }

    std::string ContourTileDataSource::getEncoding() const {
        return _dataSource->getEncoding();
    }

    std::shared_ptr<ElevationDecoder> ContourTileDataSource::resolveDecoder(const std::shared_ptr<TileData>& tileData) const {
        if (_elevationDecoder) {
            return _elevationDecoder;
        }
        std::string encoding = _dataSource->getEncoding();
        if (tileData) {
            std::shared_ptr<Variant> encVariant = tileData->getMetadata("encoding");
            if (encVariant && encVariant->getType() == VariantType::VARIANT_TYPE_STRING) {
                encoding = encVariant->getString();
            }
        }
        if (encoding == "mapbox") {
            static std::shared_ptr<ElevationDecoder> mapboxDecoder = std::make_shared<MapBoxElevationDataDecoder>();
            return mapboxDecoder;
        }
        static std::shared_ptr<ElevationDecoder> terrariumDecoder = std::make_shared<TerrariumElevationDataDecoder>();
        return terrariumDecoder;
    }

    double ContourTileDataSource::getIntervalForZoom(int zoom) const {
        double base = _baseInterval;
        if (zoom <= 9)  return base * 50.0; // very coarse (e.g. 500m) - only relevant if MinVisibleZoom lowered
        if (zoom <= 11) return base * 20.0; // coarse (e.g. 200m)
        if (zoom <= 12) return base * 10.0; // e.g. 100m
        if (zoom == 13) return base * 5.0;  // e.g. 50m
        return base;                        // full detail (e.g. 10m)
    }

    long long ContourTileDataSource::computeDiv(long long ele) {
        // Matches the gdal_contour based pipeline: largest "nice" divisor of the elevation.
        long long a = std::llabs(ele);
        if (a % 1000 == 0) return 1000;
        if (a % 500 == 0) return 500;
        if (a % 250 == 0) return 250;
        if (a % 200 == 0) return 200;
        if (a % 100 == 0) return 100;
        if (a % 50 == 0) return 50;
        if (a % 20 == 0) return 20;
        return 10;
    }

    std::shared_ptr<TileData> ContourTileDataSource::loadTile(const MapTile& mapTile) {
        int zoom = mapTile.getZoom();

        std::string layerName;
        float simplifyTolerance;
        {
            std::lock_guard<std::mutex> lock(_mutex);
            layerName = _layerName;
            simplifyTolerance = _simplifyTolerance;
        }

        // Below the useful contour zoom, emit an empty (but valid) tile without fetching or decoding the
        // DEM. This keeps zoomed-out frames cheap even though the layer may request many such tiles.
        if (zoom < _minVisibleZoom.load()) {
            try {
                mbvtbuilder::MBVTTileBuilder emptyBuilder(zoom, zoom);
                emptyBuilder.createLayer(layerName);
                protobuf::encoded_message encodedTile;
                emptyBuilder.buildTile(zoom, mapTile.getX(), mapTile.getY(), encodedTile);
                auto data = std::make_shared<BinaryData>(reinterpret_cast<const unsigned char*>(encodedTile.data().data()), encodedTile.data().size());
                auto tileData = std::make_shared<TileData>(data);
                applyTileMetadata(tileData, mapTile);
                return tileData;
            }
            catch (const std::exception& ex) {
                return std::shared_ptr<TileData>();
            }
        }

        std::shared_ptr<TileData> elevTileData = _dataSource->loadTile(mapTile);
        if (!elevTileData || !elevTileData->getData()) {
            return std::shared_ptr<TileData>();
        }
        // Propagate parent-replacement/overzoom to keep behaviour consistent with the wrapped source.
        if (elevTileData->isReplaceWithParent()) {
            auto emptyData = std::make_shared<BinaryData>(std::vector<unsigned char>());
            auto tileData = std::make_shared<TileData>(emptyData);
            tileData->setReplaceWithParent(true);
            return tileData;
        }

        std::shared_ptr<ElevationDecoder> decoder = resolveDecoder(elevTileData);
        std::array<double, 4> coeffs = decoder->getColorComponentCoefficients();

        std::shared_ptr<Bitmap> bitmap = Bitmap::CreateFromCompressed(elevTileData->getData());
        if (!bitmap) {
            Log::Errorf("ContourTileDataSource::loadTile: Failed to decode elevation bitmap for %s", mapTile.toString().c_str());
            return std::shared_ptr<TileData>();
        }

        int fullW = bitmap->getWidth();
        int fullH = bitmap->getHeight();
        if (fullW < 2 || fullH < 2) {
            return std::shared_ptr<TileData>();
        }

        int bytesPerPixel = 0;
        switch (bitmap->getColorFormat()) {
            case ColorFormat::COLOR_FORMAT_GRAYSCALE: bytesPerPixel = 1; break;
            case ColorFormat::COLOR_FORMAT_RGB:       bytesPerPixel = 3; break;
            case ColorFormat::COLOR_FORMAT_RGBA:      bytesPerPixel = 4; break;
            default:
                Log::Error("ContourTileDataSource::loadTile: Unsupported bitmap color format");
                return std::shared_ptr<TileData>();
        }

        // Subsample the DEM before tracing: full 256x256 tracing produces far more vertices than
        // 100/50/10 m contours need, and every extra vertex is re-simplified, uploaded and draped
        // over the 3D terrain mesh each frame. Trace on an at-most 'resolution'-per-side grid.
        // The nodes are spread evenly INCLUDING both endpoints (pixel 0 and fullW-1), so grid node 0
        // maps to the tile's west/south edge and node W-1/H-1 to the east/north edge. That makes
        // adjacent contour tiles share their boundary samples and meet without holes.
        int resolution = std::max(8, _resolution.load());
        int W = std::min(fullW, resolution);
        int H = std::min(fullH, resolution);
        if (W < 2 || H < 2) {
            return std::shared_ptr<TileData>();
        }

        // Optionally fetch neighbour DEM tiles so the tile's east/north edges use the neighbours' own
        // edge samples, making contour lines meet across tile boundaries. The DEM bitmap is stored
        // south-to-north / west-to-east and the tile bounds use mapTile.getFlipped(). In that flipped
        // (projection) tile scheme north = flipped.y + 1, which flips back to datasource y - 1. So the
        // geographic east/north/north-east neighbours are datasource tiles (x+1, y) / (x, y-1) / (x+1, y-1).
        bool seamless = _seamlessEdges.load();
        std::shared_ptr<Bitmap> eastBitmap, northBitmap, neBitmap;
        if (seamless) {
            auto fetchNeighbour = [&](int dx, int dy) -> std::shared_ptr<Bitmap> {
                MapTile nt(mapTile.getX() + dx, mapTile.getY() + dy, zoom, mapTile.getFrameNr());
                std::shared_ptr<TileData> td = _dataSource->loadTile(nt);
                if (!td || !td->getData() || td->isReplaceWithParent()) {
                    return std::shared_ptr<Bitmap>();
                }
                std::shared_ptr<Bitmap> bm = Bitmap::CreateFromCompressed(td->getData());
                if (!bm || bm->getWidth() != fullW || bm->getHeight() != fullH || bm->getColorFormat() != bitmap->getColorFormat()) {
                    return std::shared_ptr<Bitmap>(); // fall back to edge duplication
                }
                return bm;
            };
            eastBitmap = fetchNeighbour(1, 0);
            northBitmap = fetchNeighbour(0, -1);
            neBitmap = fetchNeighbour(1, -1);
        }

        const std::vector<std::uint8_t>& pixelData = bitmap->getPixelData();
        auto decodePixel = [&](const std::vector<std::uint8_t>& pd, int lx, int ly) -> float {
            const std::uint8_t* ptr = &pd[(static_cast<std::size_t>(ly) * fullW + lx) * bytesPerPixel];
            double r = 0, g = 0, b = 0, a = 255;
            switch (bytesPerPixel) {
                case 1: r = g = b = ptr[0]; break;
                case 3: r = ptr[0]; g = ptr[1]; b = ptr[2]; break;
                case 4: r = ptr[0]; g = ptr[1]; b = ptr[2]; a = ptr[3]; break;
            }
            return static_cast<float>(coeffs[0] * r + coeffs[1] * g + coeffs[2] * b + coeffs[3] * (a / 255.0));
        };
        // Sample the DEM at pixel (px, py). With seamless edges px may reach fullW and py may reach fullH,
        // which pull from the east/north/north-east neighbours' opposite edge (or duplicate our own edge
        // if a neighbour is missing).
        auto sampleHeight = [&](int px, int py) -> float {
            bool east = (px >= fullW);
            bool north = (py >= fullH);
            if (east && north) {
                if (neBitmap) return decodePixel(neBitmap->getPixelData(), 0, 0);
                return decodePixel(pixelData, fullW - 1, fullH - 1);
            }
            if (east) {
                if (eastBitmap) return decodePixel(eastBitmap->getPixelData(), 0, py);
                return decodePixel(pixelData, fullW - 1, py);
            }
            if (north) {
                if (northBitmap) return decodePixel(northBitmap->getPixelData(), px, 0);
                return decodePixel(pixelData, px, fullH - 1);
            }
            return decodePixel(pixelData, px, py);
        };

        // Decode DEM into a resampled height grid (row 0 = south). Nodes span [0, spanW]/[0, spanH]:
        // without seamless edges spanW = fullW-1 (last sample = last pixel), with seamless edges spanW = fullW
        // (last sample = east neighbour's first column), so node W-1 lands exactly on the tile's east edge.
        int spanW = seamless ? fullW : (fullW - 1);
        int spanH = seamless ? fullH : (fullH - 1);
        std::vector<float> heights(static_cast<std::size_t>(W) * H);
        float minH = 1.0e9f, maxH = -1.0e9f;
        for (int gy = 0; gy < H; gy++) {
            int py = static_cast<int>(std::llround(static_cast<double>(gy) * spanH / (H - 1)));
            for (int gx = 0; gx < W; gx++) {
                int px = static_cast<int>(std::llround(static_cast<double>(gx) * spanW / (W - 1)));
                float h = sampleHeight(px, py);
                heights[static_cast<std::size_t>(gy) * W + gx] = h;
                if (h < minH) minH = h;
                if (h > maxH) maxH = h;
            }
        }

        // Geographic bounds of this tile (EPSG3857). Note the getFlipped(): the DEM bitmap's
        // south edge (grid row 0) corresponds to the flipped tile's minimum-y bound, matching
        // ElevationManager. The MBVT builder is called with the raw tile x/y (as GeoJSONVectorTileDataSource
        // does), so the same footprint is reproduced without a double flip.
        std::shared_ptr<Projection> projection = getProjection();
        MapBounds bounds = TileUtils::CalculateMapTileBounds(mapTile.getFlipped(), projection);
        double minX = bounds.getMin().getX(), minY = bounds.getMin().getY();
        double sizeX = bounds.getMax().getX() - minX;
        double sizeY = bounds.getMax().getY() - minY;

        auto gridToWgs84 = [&](const GridPoint& p) -> mbvtbuilder::MBVTTileBuilder::Point {
            // Node 0 -> tile min edge, node W-1/H-1 -> tile max edge (even resample spans full extent).
            double fx = p.first / static_cast<double>(W - 1);
            double fy = p.second / static_cast<double>(H - 1);
            if (fx > 1.0) fx = 1.0;
            if (fy > 1.0) fy = 1.0;
            MapPos pos3857(minX + fx * sizeX, minY + fy * sizeY);
            MapPos wgs84 = projection->toWgs84(pos3857);
            return mbvtbuilder::MBVTTileBuilder::Point(wgs84.getX(), wgs84.getY());
        };

        double interval = getIntervalForZoom(zoom);

        mbvtbuilder::MBVTTileBuilder tileBuilder(zoom, zoom);
        tileBuilder.setSimplifyTolerance(simplifyTolerance);
        int layerIndex = tileBuilder.createLayer(layerName);

        // Generate one feature (a MultiLineString) per contour level.
        long long firstLevel = static_cast<long long>(std::ceil(minH / interval));
        long long lastLevel = static_cast<long long>(std::floor(maxH / interval));
        // Safety cap: a very low-zoom tile can span kilometres of relief. Beyond this many levels the
        // tile is unreadable anyway, so bound the tracing cost. (Raise base interval to see more range.)
        const long long MAX_LEVELS = 200;
        if (lastLevel - firstLevel + 1 > MAX_LEVELS) {
            Log::Warnf("ContourTileDataSource::loadTile: %s spans %lld contour levels at interval %g m; capping to %lld. Increase base interval or restrict min zoom.",
                       mapTile.toString().c_str(), lastLevel - firstLevel + 1, interval, MAX_LEVELS);
            lastLevel = firstLevel + MAX_LEVELS - 1;
        }
        for (long long l = firstLevel; l <= lastLevel; l++) {
            double level = l * interval;
            long long ele = static_cast<long long>(std::llround(level));

            std::vector<Polyline> polylines = marchingSquares(heights, W, H, level);
            if (polylines.empty()) {
                continue;
            }

            mbvtbuilder::MBVTTileBuilder::MultiLineString lines;
            lines.reserve(polylines.size());
            for (const Polyline& pl : polylines) {
                std::vector<mbvtbuilder::MBVTTileBuilder::Point> line;
                line.reserve(pl.size());
                for (const GridPoint& gp : pl) {
                    line.push_back(gridToWgs84(gp));
                }
                lines.push_back(std::move(line));
            }

            picojson::object props;
            props["ele"] = picojson::value(static_cast<std::int64_t>(ele));
            props["div"] = picojson::value(static_cast<std::int64_t>(computeDiv(ele)));
            tileBuilder.addMultiLineString(layerIndex, std::move(lines), picojson::value(static_cast<std::int64_t>(ele)), picojson::value(props), false);
        }

        try {
            protobuf::encoded_message encodedTile;
            tileBuilder.buildTile(zoom, mapTile.getX(), mapTile.getY(), encodedTile);
            auto data = std::make_shared<BinaryData>(reinterpret_cast<const unsigned char*>(encodedTile.data().data()), encodedTile.data().size());
            auto tileData = std::make_shared<TileData>(data);
            applyTileMetadata(tileData, mapTile);
            return tileData;
        }
        catch (const std::exception& ex) {
            Log::Errorf("ContourTileDataSource::loadTile: Failed to build contour tile %s: %s", mapTile.toString().c_str(), ex.what());
            return std::shared_ptr<TileData>();
        }
    }

    ContourTileDataSource::DataSourceListener::DataSourceListener(ContourTileDataSource& dataSource) :
        _dataSource(dataSource)
    {
    }

    void ContourTileDataSource::DataSourceListener::onTilesChanged(bool removeTiles) {
        _dataSource.notifyTilesChanged(removeTiles);
    }

}
