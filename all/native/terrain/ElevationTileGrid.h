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
#include <functional>
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
         * neighbouring grids (order: W, E, S, N, SW, SE, NW, NE). Same-level neighbours are
         * copied texel-exactly; coarser (ancestor) neighbour grids are sampled at the border
         * texel centers, which still gives real DEM data across the tile border. Only missing
         * neighbours fall back to duplicating this grid's edge texels.
         * Adjacent tiles then interpolate across the border from IDENTICAL texel pairs,
         * making same-level tile borders seam-free. The padded texture covers the grid
         * bounds extended by one texel on each side.
         */
        void encodeTextureWithBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, std::vector<std::uint8_t>& rgbaData, std::array<float, 4>& decode) const;

        /**
         * The four 2-texel-thick strips of the padded texture that depend on the NEIGHBOURS:
         * the border ring itself, plus this grid's own outermost row/column, which a coarser
         * neighbour box-filters (see encodeTextureWithBorders). Everything else in the texture
         * comes from this grid alone and cannot change when a neighbour arrives.
         *
         * A neighbour landing is by far the most common reason to re-encode - during a pan it is
         * continuous - and rebuilding a megabyte of texture for a 2-texel ring is most of what the
         * elevation texture pipeline costs. Patching these strips into the existing texture is the
         * same result for ~1.5% of the texels.
         *
         * Strip layout, rows south-to-north and columns west-to-east, as in the padded texture:
         * south/north are (width + 2) x 2, west/east are 2 x (height + 2). Corners are covered by
         * south and north, so the strips overlap there and agree.
         */
        struct BorderStrips {
            std::vector<std::uint8_t> south, north, west, east;
        };
        void encodeTextureBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, BorderStrips& strips, std::array<float, 4>& decode) const;

        /**
         * Decodes a DEM bitmap (mapbox/terrarium RGB encoded) into an elevation grid using
         * the given color component coefficients. Returns null if the bitmap has an unsupported format.
         */
        /** The constant term of the decode: meters = dot(sample, decode) + DecodeOffset(). */
        static float DecodeOffset() { return QUANT_OFFSET; }

        static std::shared_ptr<ElevationTileGrid> DecodeBitmap(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs);

    private:
        // The padded texture's texel value at (gx, gy), gx in [-1, width] and gy in [-1, height]:
        // this grid's own texel, a neighbour's, or a box-filtered edge value. Built once per
        // encode because the edge filters it needs are O(width + height) to compute; both the full
        // encode and the border patch go through it, so they cannot disagree.
        std::function<std::uint16_t(int, int)> makeTexelSampler(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours) const;

        // LUMINANCE_ALPHA: the quantized height's high byte in L, its low byte in A. Two bytes a
        // texel instead of four - the elevation texture working set is what makes extra DEM detail
        // expensive, and it is halved here at no cost in precision (the height was 16 bits either
        // way). The decode stays linear in both channels, so the shader's per-tap decode and the
        // manual bilinear are unchanged; only the constant term moved to a uniform of its own.
        static constexpr int TEXEL_BYTES = 2;

        static void WriteTexel(std::uint8_t* dst, std::uint16_t value) {
            dst[0] = static_cast<std::uint8_t>(value >> 8);
            dst[1] = static_cast<std::uint8_t>(value & 255);
        }

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
