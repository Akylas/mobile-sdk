#include "ElevationTileGrid.h"
#include "graphics/Bitmap.h"
#include "utils/Log.h"

#include <algorithm>
#include <cmath>

namespace carto {

    ElevationTileGrid::ElevationTileGrid(const MapTile& tile, const MapBounds& internalBounds, int width, int height, std::vector<float> heights) :
        _tile(tile),
        _internalBounds(internalBounds),
        _width(width),
        _height(height),
        _heights(),
        _minHeight(0),
        _maxHeight(0),
        _quantScale(QUANT_MIN_SCALE),
        _quantOffset(0)
    {
        if (!heights.empty()) {
            auto minmax = std::minmax_element(heights.begin(), heights.end());
            _minHeight = *minmax.first;
            _maxHeight = *minmax.second;

            // The 16 bits cover this tile's own range plus a margin for the border ring.
            float margin = QUANT_MARGIN_REL * (_maxHeight - _minHeight) + QUANT_MARGIN_ABS;
            _quantOffset = _minHeight - margin;
            _quantScale = std::max((_maxHeight - _minHeight + 2 * margin) / 65535.0f, QUANT_MIN_SCALE);

            _heights.resize(heights.size());
            for (std::size_t i = 0; i < heights.size(); i++) {
                _heights[i] = encodeHeight(heights[i]);
            }
        }
    }

    float ElevationTileGrid::sampleHeight(double internalX, double internalY) const {
        double boundsWidth = _internalBounds.getMax().getX() - _internalBounds.getMin().getX();
        double boundsHeight = _internalBounds.getMax().getY() - _internalBounds.getMin().getY();
        if (boundsWidth <= 0 || boundsHeight <= 0 || _width < 1 || _height < 1) {
            return 0.0f;
        }

        // Sample positions at pixel centers, bilinear interpolation between them, clamped at edges
        double fx = (internalX - _internalBounds.getMin().getX()) / boundsWidth * _width - 0.5;
        double fy = (internalY - _internalBounds.getMin().getY()) / boundsHeight * _height - 0.5;
        int gx0 = static_cast<int>(std::floor(fx));
        int gy0 = static_cast<int>(std::floor(fy));
        float dx = static_cast<float>(fx - gx0);
        float dy = static_cast<float>(fy - gy0);

        int gx1 = std::min(std::max(gx0 + 1, 0), _width - 1);
        int gy1 = std::min(std::max(gy0 + 1, 0), _height - 1);
        gx0 = std::min(std::max(gx0, 0), _width - 1);
        gy0 = std::min(std::max(gy0, 0), _height - 1);

        float h00 = getHeight(gx0, gy0);
        float h10 = getHeight(gx1, gy0);
        float h01 = getHeight(gx0, gy1);
        float h11 = getHeight(gx1, gy1);
        return (h00 * (1 - dx) + h10 * dx) * (1 - dy) + (h01 * (1 - dx) + h11 * dx) * dy;
    }

    void ElevationTileGrid::sampleGradient(double internalX, double internalY, float& dhdx, float& dhdy) const {
        double boundsWidth = _internalBounds.getMax().getX() - _internalBounds.getMin().getX();
        double boundsHeight = _internalBounds.getMax().getY() - _internalBounds.getMin().getY();
        dhdx = 0;
        dhdy = 0;
        if (boundsWidth <= 0 || boundsHeight <= 0 || _width < 2 || _height < 2) {
            return;
        }

        double texelX = boundsWidth / _width;
        double texelY = boundsHeight / _height;
        dhdx = static_cast<float>((sampleHeight(internalX + texelX, internalY) - sampleHeight(internalX - texelX, internalY)) / (2 * texelX));
        dhdy = static_cast<float>((sampleHeight(internalX, internalY + texelY) - sampleHeight(internalX, internalY - texelY)) / (2 * texelY));
    }

    void ElevationTileGrid::encodeTexture(std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const {
        rgbaData.resize(_heights.size() * TEXEL_BYTES);
        for (std::size_t i = 0; i < _heights.size(); i++) {
            WriteTexel(&rgbaData[i * TEXEL_BYTES], _heights[i]);
        }
        decode = { { 255.0f * 256.0f * _quantScale, 0.0f, 0.0f, 255.0f * _quantScale } };
    }

    std::function<std::uint16_t(int, int)> ElevationTileGrid::makeTexelSampler(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours) const {
        // Same DEM level and grid size: the border texel is one of the neighbour's own
        // texels, so it can be copied bit-exactly by index.
        auto sameLevel = [this](const std::shared_ptr<ElevationTileGrid>& grid) {
            // The same grid standing in for a neighbour (both tiles resolved to one ancestor)
            // must NOT be index-copied - that would wrap around to its opposite edge.
            return grid && grid->_width == _width && grid->_height == _height && grid->_tile.getZoom() == _tile.getZoom() && !(grid->_tile == _tile);
        };
        // Different level (a coarser ancestor grid stands in for the neighbour): sample the
        // neighbour's height field at the geographic position of the border texel center.
        // Real DEM data at the tile edge beats duplicating our own edge texel, which leaves
        // a full-texel height step (tens of meters on a slope) at the tile border.
        double texelX = (_internalBounds.getMax().getX() - _internalBounds.getMin().getX()) / _width;
        double texelY = (_internalBounds.getMax().getY() - _internalBounds.getMin().getY()) / _height;
        auto sampleValue = [&, this](const ElevationTileGrid* grid, int gx, int gy) -> std::uint16_t {
            double px = _internalBounds.getMin().getX() + (gx + 0.5) * texelX;
            double py = _internalBounds.getMin().getY() + (gy + 0.5) * texelY;
            return encodeHeight(grid->sampleHeight(px, py));
        };
        // EDGE BOX FILTER. A coarser neighbour's height field is the 2^k x 2^k average of this
        // level's (mapterhorn and every other overview pyramid downsamples that way), so along a
        // shared edge it only ever interpolates those averages while this tile interpolates its
        // own full-detail texels. Border backfill alone does not close that: this side meets the
        // border at (own texel + neighbour average) / 2 while the neighbour meets it at
        // (neighbour average + own average) / 2, and the difference - half the local high-frequency
        // detail - is the dotted speckle line along LOD-ring borders.
        // Averaging THIS tile's outermost texel row/column over the neighbour's texel footprint
        // removes that term: both sides then meet the border on the same average. What is left is
        // an eighth of the difference between the neighbour's texel and this tile's average over
        // it - the border texel is itself an interpolation of the coarse field, not one of its
        // texels - so the seam is reduced rather than eliminated. Only the outermost row/column is
        // touched, and only towards a coarser neighbour: everything else keeps full DEM detail.
        // Groups are found geographically rather than assumed to be a power of two, so an unaligned
        // or non-quadtree neighbour degrades to a no-op instead of a shift.
        // In the steady state this rarely fires: the elevation level cap usually gives two render
        // tiles of different zoom the SAME DEM level (see ElevationManager::clampTileZoom), and the
        // lattice mismatch that remains at a LOD ring is what TileEdgeStitchingEnabled handles.
        // It does fire while tiles stream in, when a tile is still standing on an ancestor grid.
        // alongY: the edge runs north-south (west/east edge), so texel ROWS are grouped and
        // fixedIndex is the column; otherwise columns are grouped and fixedIndex is the row.
        auto edgeFilter = [&, this](const std::shared_ptr<ElevationTileGrid>& neighbour, bool alongY, int fixedIndex) -> std::vector<std::uint16_t> {
            std::vector<std::uint16_t> result;
            if (!neighbour || sameLevel(neighbour) || neighbour->_width < 1 || neighbour->_height < 1) {
                return result;
            }
            double ourTexel = alongY ? texelY : texelX;
            double neighbourTexel = alongY
                ? (neighbour->_internalBounds.getMax().getY() - neighbour->_internalBounds.getMin().getY()) / neighbour->_height
                : (neighbour->_internalBounds.getMax().getX() - neighbour->_internalBounds.getMin().getX()) / neighbour->_width;
            if (!(neighbourTexel > ourTexel * 1.5)) {
                return result; // same resolution or finer: this tile is already the smooth side
            }
            double neighbourOrigin = alongY ? neighbour->_internalBounds.getMin().getY() : neighbour->_internalBounds.getMin().getX();
            double ourOrigin = alongY ? _internalBounds.getMin().getY() : _internalBounds.getMin().getX();
            auto groupOf = [&](int i) {
                return static_cast<long long>(std::floor((ourOrigin + (i + 0.5) * ourTexel - neighbourOrigin) / neighbourTexel));
            };
            int count = alongY ? _height : _width;
            result.resize(count);
            for (int i = 0; i < count; ) {
                long long group = groupOf(i);
                int last = i;
                double sum = 0;
                while (last < count && groupOf(last) == group) {
                    // The stored value is a linear encoding of the height, so averaging encoded
                    // values is averaging heights (up to half a quantum).
                    sum += alongY
                        ? _heights[static_cast<std::size_t>(last) * _width + fixedIndex]
                        : _heights[static_cast<std::size_t>(fixedIndex) * _width + last];
                    last++;
                }
                std::uint16_t average = static_cast<std::uint16_t>(sum / (last - i) + 0.5);
                for (int j = i; j < last; j++) {
                    result[j] = average;
                }
                i = last;
            }
            return result;
        };
        std::vector<std::uint16_t> westEdge = edgeFilter(neighbours[0], true, 0);
        std::vector<std::uint16_t> eastEdge = edgeFilter(neighbours[1], true, _width - 1);
        std::vector<std::uint16_t> southEdge = edgeFilter(neighbours[2], false, 0);
        std::vector<std::uint16_t> northEdge = edgeFilter(neighbours[3], false, _height - 1);

        // texel value at padded coordinates (gx, gy in [-1, width/height]); border texels
        // come from the neighbour that actually covers them, falling back to edge clamping.
        // Captured BY VALUE: the sampler outlives this call, and the edge filters are the
        // expensive part of it.
        return [this, neighbours, texelX, texelY, westEdge, eastEdge, southEdge, northEdge](int gx, int gy) -> std::uint16_t {
            auto sameLevel = [this](const std::shared_ptr<ElevationTileGrid>& grid) {
                return grid && grid->_width == _width && grid->_height == _height && grid->_tile.getZoom() == _tile.getZoom() && !(grid->_tile == _tile);
            };
            auto sampleValue = [&, this](const ElevationTileGrid* grid, int sx, int sy) -> std::uint16_t {
                double px = _internalBounds.getMin().getX() + (sx + 0.5) * texelX;
                double py = _internalBounds.getMin().getY() + (sy + 0.5) * texelY;
                return encodeHeight(grid->sampleHeight(px, py));
            };
            static const std::array<std::pair<int, int>, 8> DIRS = { {
                { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 }, { -1, -1 }, { 1, -1 }, { -1, 1 }, { 1, 1 }
            } };
            int dx = (gx < 0 ? -1 : (gx >= _width ? 1 : 0));
            int dy = (gy < 0 ? -1 : (gy >= _height ? 1 : 0));
            if (dx != 0 || dy != 0) {
                for (std::size_t i = 0; i < DIRS.size(); i++) {
                    if (DIRS[i].first != dx || DIRS[i].second != dy) {
                        continue;
                    }
                    const std::shared_ptr<ElevationTileGrid>& neighbour = neighbours[i];
                    if (sameLevel(neighbour)) {
                        int nx = gx - dx * _width;
                        int ny = gy - dy * _height;
                        // Re-encoded: every grid quantises over its OWN height range, so a raw
                        // index copy would carry the neighbour's scale into this texture.
                        return encodeHeight(neighbour->getHeight(nx, ny));
                    }
                    if (neighbour) {
                        return sampleValue(neighbour.get(), gx, gy);
                    }
                    break;
                }
            }
            // no neighbour data: duplicate our own edge texel
            int cx = std::min(std::max(gx, 0), _width - 1);
            int cy = std::min(std::max(gy, 0), _height - 1);
            // Own texel, but on an edge shared with a coarser neighbour: the box-filtered value.
            // A corner texel lies on two such edges and takes the mean of both, which is what the
            // two neighbours (and the diagonal one between them) average to as well.
            double filtered = 0;
            int filterCount = 0;
            if (!westEdge.empty() && cx == 0) {
                filtered += westEdge[cy];
                filterCount++;
            }
            if (!eastEdge.empty() && cx == _width - 1) {
                filtered += eastEdge[cy];
                filterCount++;
            }
            if (!southEdge.empty() && cy == 0) {
                filtered += southEdge[cx];
                filterCount++;
            }
            if (!northEdge.empty() && cy == _height - 1) {
                filtered += northEdge[cx];
                filterCount++;
            }
            if (filterCount > 0) {
                return static_cast<std::uint16_t>(filtered / filterCount + 0.5);
            }
            return _heights[static_cast<std::size_t>(cy) * _width + cx];
        };
    }

    void ElevationTileGrid::encodeTextureWithBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const {
        int paddedWidth = _width + 2;
        int paddedHeight = _height + 2;
        rgbaData.resize(static_cast<std::size_t>(paddedWidth) * paddedHeight * TEXEL_BYTES);

        std::function<std::uint16_t(int, int)> texelValue = makeTexelSampler(neighbours);

        // Only the border ring and the two outermost own rows/columns can come from anywhere but
        // this grid: the border ring by definition, the outermost own texels because a coarser
        // neighbour box-filters them (edgeFilter above). Everything else is this grid's own texel
        // at its own index. Running the general sampler over the whole tile instead - with its
        // neighbour dispatch and four edge-filter tests PER TEXEL - is what made encoding one
        // texture cost 79 ms on the device (measured, 514x514), which is most of a frame.
        std::size_t i = 0;
        for (int gy = -1; gy <= _height; gy++) {
            bool ownRow = (gy > 0 && gy < _height - 1);
            for (int gx = -1; gx <= _width; gx++) {
                if (ownRow && gx == 1) {
                    // The row's own span, straight from the height field.
                    const std::uint16_t* row = &_heights[static_cast<std::size_t>(gy) * _width];
                    for (; gx < _width - 1; gx++) {
                        WriteTexel(&rgbaData[i], row[gx]);
                        i += TEXEL_BYTES;
                    }
                }
                WriteTexel(&rgbaData[i], texelValue(gx, gy));
                i += TEXEL_BYTES;
            }
        }
        decode = { { 255.0f * 256.0f * _quantScale, 0.0f, 0.0f, 255.0f * _quantScale } };
    }

    void ElevationTileGrid::encodeTextureBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, BorderStrips& strips, std::array<float, 4>& decode) const {
        int paddedWidth = _width + 2;
        int paddedHeight = _height + 2;

        std::function<std::uint16_t(int, int)> texelValue = makeTexelSampler(neighbours);

        // South and north: two full-width rows each (gy = -1, 0 and height-1, height).
        strips.south.resize(static_cast<std::size_t>(paddedWidth) * 2 * TEXEL_BYTES);
        strips.north.resize(static_cast<std::size_t>(paddedWidth) * 2 * TEXEL_BYTES);
        for (int row = 0; row < 2; row++) {
            std::size_t s = static_cast<std::size_t>(row) * paddedWidth * TEXEL_BYTES;
            for (int gx = -1; gx <= _width; gx++, s += TEXEL_BYTES) {
                WriteTexel(&strips.south[s], texelValue(gx, -1 + row));
                WriteTexel(&strips.north[s], texelValue(gx, _height - 1 + row));
            }
        }
        // West and east: two full-height columns each (gx = -1, 0 and width-1, width).
        strips.west.resize(static_cast<std::size_t>(paddedHeight) * 2 * TEXEL_BYTES);
        strips.east.resize(static_cast<std::size_t>(paddedHeight) * 2 * TEXEL_BYTES);
        for (int gy = -1; gy <= _height; gy++) {
            std::size_t s = static_cast<std::size_t>(gy + 1) * 2 * TEXEL_BYTES;
            for (int col = 0; col < 2; col++) {
                WriteTexel(&strips.west[s + col * TEXEL_BYTES], texelValue(-1 + col, gy));
                WriteTexel(&strips.east[s + col * TEXEL_BYTES], texelValue(_width - 1 + col, gy));
            }
        }
        decode = { { 255.0f * 256.0f * _quantScale, 0.0f, 0.0f, 255.0f * _quantScale } };
    }

    std::shared_ptr<ElevationTileGrid> ElevationTileGrid::DecodeBitmap(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs) {
        if (!bitmap) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        int width = bitmap->getWidth();
        int height = bitmap->getHeight();
        if (width < 1 || height < 1) {
            return std::shared_ptr<ElevationTileGrid>();
        }

        int bytesPerPixel = 0;
        switch (bitmap->getColorFormat()) {
        case ColorFormat::COLOR_FORMAT_GRAYSCALE:
            bytesPerPixel = 1;
            break;
        case ColorFormat::COLOR_FORMAT_RGB:
            bytesPerPixel = 3;
            break;
        case ColorFormat::COLOR_FORMAT_RGBA:
            bytesPerPixel = 4;
            break;
        default:
            Log::Error("ElevationTileGrid::DecodeBitmap: Unsupported bitmap color format");
            return std::shared_ptr<ElevationTileGrid>();
        }

        // Bitmap pixel data rows are stored bottom-up relative to the image, which means
        // row 0 of the pixel data corresponds to the southern (minimum y) edge of the tile.
        // This matches the grid row order, so pixels can be converted sequentially.
        const std::vector<std::uint8_t>& pixelData = bitmap->getPixelData();
        std::vector<float> heights(static_cast<std::size_t>(width) * height);
        for (std::size_t i = 0; i < heights.size(); i++) {
            const std::uint8_t* ptr = &pixelData[i * bytesPerPixel];
            double r = 0, g = 0, b = 0, a = 255;
            switch (bytesPerPixel) {
            case 1:
                r = g = b = ptr[0];
                break;
            case 3:
                r = ptr[0]; g = ptr[1]; b = ptr[2];
                break;
            case 4:
                r = ptr[0]; g = ptr[1]; b = ptr[2]; a = ptr[3];
                break;
            }
            heights[i] = static_cast<float>(coeffs[0] * r + coeffs[1] * g + coeffs[2] * b + coeffs[3] * (a / 255.0));
        }

        auto grid = std::make_shared<ElevationTileGrid>(tile, internalBounds, width, height, std::move(heights));
        if (grid->getMinHeight() < -12000.0f || grid->getMaxHeight() > 10000.0f) {
            Log::Warnf("ElevationTileGrid::DecodeBitmap: Implausible elevation range %g..%g m for tile %d/%d/%d - check that the elevation data source encoding ('terrarium'/'mapbox') matches the data",
                       grid->getMinHeight(), grid->getMaxHeight(), tile.getZoom(), tile.getX(), tile.getY());
        }
        return grid;
    }
}
