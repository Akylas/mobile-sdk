/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_ELEVATIONTILEGRID_H_
#define _MASSIF_ELEVATIONTILEGRID_H_

#include "core/MapTile.h"
#include "core/MapBounds.h"
#include "graphics/Bitmap.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace massif {

    /**
     * A single decoded DEM tile.
     *
     * The grid IS the source raster: the encoded tile bitmap (mapbox or terrarium RGB) is kept as
     * it arrived and every height is decoded from it on the fly, which is tangram's model
     * (util/elevationManager.cpp reads its elevation straight out of the raster's texture buffer).
     * Nothing is re-quantised, so the height field has exactly the precision the data source
     * offers - 1/256m for terrarium, 0.1m for mapbox - and the GPU texture is a copy of the same
     * texels, decoded in the shader with the source's own coefficients.
     *
     * Grid rows are stored south-to-north (row 0 corresponds to the minimum internal y), which is
     * the Bitmap row order.
     * Internal class, not exposed in the public API.
     */
    class ElevationTileGrid {
    public:
        ElevationTileGrid(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs);

        const MapTile& getTile() const { return _tile; }
        const MapBounds& getInternalBounds() const { return _internalBounds; }
        int getWidth() const { return _width; }
        int getHeight() const { return _height; }
        float getMinHeight() const { return _minHeight; }
        float getMaxHeight() const { return _maxHeight; }
        std::size_t getDataSize() const;

        /** The texture built from this grid has the source raster's own format. */
        ColorFormat::ColorFormat getColorFormat() const;
        int getBytesPerTexel() const { return _bytesPerTexel; }

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
         * The decode the shader applies to a texture sample: meters = dot(sample, decode) +
         * getDecodeOffset(). The source coefficients apply to the raw 0..255 byte values, so they
         * are scaled by 255 for the normalized texture sample; the constant term is handed over
         * separately rather than riding on the alpha channel, so a source raster's alpha is
         * ignored rather than trusted.
         * The mapping is linear in every channel, so bilinear texture filtering commutes with
         * decoding and a GPU CLAMP_TO_EDGE + LINEAR sample at
         * uv = (pos - internalBounds.min) / internalBounds.size matches sampleHeight exactly.
         */
        std::array<float, 4> getDecode() const;
        float getDecodeOffset() const { return static_cast<float>(_coeffs[3]); }

        /**
         * Copies the source raster into a texture padded with a 1-texel border taken from the
         * neighbouring grids (order: W, E, S, N, SW, SE, NW, NE). Same-level neighbours are copied
         * TEXEL-EXACTLY (the raw encoded bytes, so no round trip through metres); coarser
         * (ancestor) neighbour grids are sampled at the border texel centers and re-encoded, which
         * still gives real DEM data across the tile border. Only missing neighbours fall back to
         * duplicating this grid's edge texels.
         * Adjacent tiles then interpolate across the border from IDENTICAL texel pairs, making
         * same-level tile borders seam-free. The padded texture covers the grid bounds extended by
         * one texel on each side.
         * This padding is the one place this deliberately does more than tangram, which samples the
         * raster unpadded and extrapolates at the edges: without it, adjacent DEM tiles disagree
         * within the outermost half texel and the terrain shows a ridge along every tile border.
         */
        void encodeTextureWithBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, std::vector<std::uint8_t>& textureData) const;

        /**
         * The four 2-texel-thick strips of the padded texture that depend on the NEIGHBOURS:
         * the border ring itself, plus this grid's own outermost row/column, which a coarser
         * neighbour box-filters (see encodeTextureWithBorders). Everything else in the texture
         * comes from this grid alone and cannot change when a neighbour arrives.
         *
         * A neighbour landing is by far the most common reason to rebuild a border - during a pan
         * it is continuous - and rebuilding the whole texture for a 2-texel ring is most of what
         * the elevation texture pipeline costs. Patching these strips into the existing texture is
         * the same result for ~1.5% of the texels.
         *
         * Strip layout, rows south-to-north and columns west-to-east, as in the padded texture:
         * south/north are (width + 2) x 2, west/east are 2 x (height + 2). Corners are covered by
         * south and north, so the strips overlap there and agree.
         */
        struct BorderStrips {
            std::vector<std::uint8_t> south, north, west, east;
        };
        void encodeTextureBorders(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours, BorderStrips& strips) const;

        /**
         * Wraps a DEM bitmap (mapbox/terrarium RGB encoded) in an elevation grid using the given
         * color component coefficients. Returns null if the bitmap has an unsupported format.
         */
        static std::shared_ptr<ElevationTileGrid> DecodeBitmap(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs);

    private:
        // The padded texture's texel at (gx, gy), gx in [-1, width] and gy in [-1, height], written
        // into 'dst': this grid's own texel, a neighbour's, or a box-filtered edge value. Built
        // once per encode because the edge filters it needs are O(width + height) to compute; both
        // the full copy and the border patch go through it, so they cannot disagree.
        std::function<void(int, int, std::uint8_t*)> makeTexelSampler(const std::array<std::shared_ptr<ElevationTileGrid>, 8>& neighbours) const;

        const std::uint8_t* texel(int gx, int gy) const {
            return &_pixelData[(static_cast<std::size_t>(gy) * _width + gx) * _bytesPerTexel];
        }

        float decodeTexel(const std::uint8_t* p) const {
            double h = _coeffs[3];
            for (int i = 0; i < _bytesPerTexel && i < 3; i++) {
                h += _coeffs[i] * p[i];
            }
            return static_cast<float>(h);
        }

        // The inverse of decodeTexel, for the border texels that have to be RESAMPLED from a
        // coarser neighbour rather than copied. Both supported encodings are positional in base
        // 256 (terrarium 256, 1, 1/256; mapbox 25.6, 0.1 with a x256 head), so the digits come out
        // of a plain greedy division by the coefficients, largest first.
        void encodeHeight(float height, std::uint8_t* dst) const;

        float getHeight(int gx, int gy) const { return decodeTexel(texel(gx, gy)); }

        const MapTile _tile;
        const MapBounds _internalBounds;
        const std::shared_ptr<Bitmap> _bitmap;
        const std::uint8_t* _pixelData;
        const std::array<double, 4> _coeffs;
        int _width;
        int _height;
        int _bytesPerTexel;
        float _minHeight;
        float _maxHeight;
    };
}

#endif
