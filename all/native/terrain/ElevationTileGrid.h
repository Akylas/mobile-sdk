/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_ELEVATIONTILEGRID_H_
#define _CARTO_ELEVATIONTILEGRID_H_

#include "core/MapTile.h"
#include "core/MapBounds.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

namespace carto {
    class Bitmap;

    /**
     * A single decoded DEM tile: a grid of elevation samples.
     * Heights are stored quantized to 16 bits (0.25m resolution, well below typical DEM
     * accuracy) to halve the memory footprint of the decoded elevation cache.
     * Grid rows are stored south-to-north (row 0 corresponds to the minimum internal y).
     * Internal class, not exposed in the public API.
     */
    class ElevationTileGrid {
    public:
        ElevationTileGrid(const MapTile& tile, const MapBounds& internalBounds, int width, int height, std::vector<float> heights);

        const MapTile& getTile() const { return _tile; }
        const MapBounds& getInternalBounds() const { return _internalBounds; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        float getMinHeight() const { return _minHeight; }
        float getMaxHeight() const { return _maxHeight; }
        std::size_t getDataSize() const { return _heights.size() * sizeof(std::uint16_t) + sizeof(ElevationTileGrid); }

        /**
         * Bilinearly sampled elevation in meters at the given internal coordinates.
         * Coordinates are clamped to the grid bounds.
         */
        float sampleHeight(double internalX, double internalY) const;
        /**
         * Elevation gradient (dh/dx, dh/dy) in meters per internal unit at the given internal coordinates.
         */
        void sampleGradient(double internalX, double internalY, float& dhdx, float& dhdy) const;

        /**
         * Encodes the grid as RGBA texture data (R = high byte, G = low byte of the quantized
         * height, A = 255) plus decode coefficients so that
         * meters = dot(RGBA texture sample normalized to [0,1], decode).
         * The mapping is linear in every channel, so bilinear texture filtering commutes with
         * decoding and a GPU CLAMP_TO_EDGE + LINEAR sample at
         * uv = (pos - internalBounds.min) / internalBounds.size matches sampleHeight exactly.
         */
        void encodeTexture(std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const;

        /**
         * Like encodeTexture, but pads the texture with a 1-texel border taken from the
         * neighbouring grids (same DEM level, order: W, E, S, N, SW, SE, NW, NE; null or
         * differently-sized neighbours fall back to duplicating this grid's edge texels).
         * Adjacent tiles then interpolate across the border from IDENTICAL texel pairs,
         * making same-level tile borders seam-free. The padded texture covers the grid
         * bounds extended by one texel on each side.
         */
        void encodeTextureWithBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const;

        /**
         * Decodes a DEM bitmap (mapbox/terrarium RGB encoded) into an elevation grid using
         * the given color component coefficients. Returns null if the bitmap has an unsupported format.
         */
        static std::shared_ptr<ElevationTileGrid> DecodeBitmap(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs);

    private:
        // Fixed-point encoding: covers -1100m (Dead Sea + margin) to +15283m at 0.25m steps
        static constexpr float QUANT_OFFSET = -1100.0f;
        static constexpr float QUANT_SCALE = 0.25f;

        static std::uint16_t EncodeHeight(float height) {
            float value = (height - QUANT_OFFSET) / QUANT_SCALE;
            return static_cast<std::uint16_t>(value < 0 ? 0 : (value > 65535.0f ? 65535.0f : value + 0.5f));
        }

        float getHeight(int gx, int gy) const {
            return _heights[gy * _width + gx] * QUANT_SCALE + QUANT_OFFSET;
        }

        const MapTile _tile;
        const MapBounds _internalBounds;
        const int _width;
        const int _height;
        std::vector<std::uint16_t> _heights;
        float _minHeight;
        float _maxHeight;
    };
}

#endif
