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
        _maxHeight(0)
    {
        if (!heights.empty()) {
            auto minmax = std::minmax_element(heights.begin(), heights.end());
            _minHeight = *minmax.first;
            _maxHeight = *minmax.second;

            _heights.resize(heights.size());
            for (std::size_t i = 0; i < heights.size(); i++) {
                _heights[i] = EncodeHeight(heights[i]);
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
        rgbaData.resize(_heights.size() * 4);
        for (std::size_t i = 0; i < _heights.size(); i++) {
            std::uint16_t value = _heights[i];
            rgbaData[i * 4 + 0] = static_cast<std::uint8_t>(value >> 8);
            rgbaData[i * 4 + 1] = static_cast<std::uint8_t>(value & 255);
            rgbaData[i * 4 + 2] = 0;
            rgbaData[i * 4 + 3] = 255;
        }
        decode = { { 255.0f * 256.0f * QUANT_SCALE, 255.0f * QUANT_SCALE, 0.0f, QUANT_OFFSET } };
    }

    void ElevationTileGrid::encodeTextureWithBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const {
        int paddedWidth = _width + 2;
        int paddedHeight = _height + 2;
        rgbaData.resize(static_cast<std::size_t>(paddedWidth) * paddedHeight * 4);

        auto compatible = [this](const std::shared_ptr<ElevationTileGrid>& grid) {
            return grid && grid->_width == _width && grid->_height == _height;
        };
        // texel value at padded coordinates (gx, gy in [-1, width/height]); border texels
        // come from the neighbour that actually covers them, falling back to edge clamping
        auto rawValue = [&, this](int gx, int gy) -> std::uint16_t {
            const ElevationTileGrid* grid = this;
            if (gx < 0 && gy >= 0 && gy < _height && compatible(neighbours[0])) {
                grid = neighbours[0].get(); gx += _width;
            } else if (gx >= _width && gy >= 0 && gy < _height && compatible(neighbours[1])) {
                grid = neighbours[1].get(); gx -= _width;
            } else if (gy < 0 && gx >= 0 && gx < _width && compatible(neighbours[2])) {
                grid = neighbours[2].get(); gy += _height;
            } else if (gy >= _height && gx >= 0 && gx < _width && compatible(neighbours[3])) {
                grid = neighbours[3].get(); gy -= _height;
            } else if (gx < 0 && gy < 0 && compatible(neighbours[4])) {
                grid = neighbours[4].get(); gx += _width; gy += _height;
            } else if (gx >= _width && gy < 0 && compatible(neighbours[5])) {
                grid = neighbours[5].get(); gx -= _width; gy += _height;
            } else if (gx < 0 && gy >= _height && compatible(neighbours[6])) {
                grid = neighbours[6].get(); gx += _width; gy -= _height;
            } else if (gx >= _width && gy >= _height && compatible(neighbours[7])) {
                grid = neighbours[7].get(); gx -= _width; gy -= _height;
            }
            gx = std::min(std::max(gx, 0), grid->_width - 1);
            gy = std::min(std::max(gy, 0), grid->_height - 1);
            return grid->_heights[static_cast<std::size_t>(gy) * grid->_width + gx];
        };

        std::size_t i = 0;
        for (int gy = -1; gy <= _height; gy++) {
            for (int gx = -1; gx <= _width; gx++) {
                std::uint16_t value = rawValue(gx, gy);
                rgbaData[i + 0] = static_cast<std::uint8_t>(value >> 8);
                rgbaData[i + 1] = static_cast<std::uint8_t>(value & 255);
                rgbaData[i + 2] = 0;
                rgbaData[i + 3] = 255;
                i += 4;
            }
        }
        decode = { { 255.0f * 256.0f * QUANT_SCALE, 255.0f * QUANT_SCALE, 0.0f, QUANT_OFFSET } };
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
