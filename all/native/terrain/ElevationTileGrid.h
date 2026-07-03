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
#include <memory>
#include <vector>

namespace carto {
    class Bitmap;

    /**
     * A single decoded DEM tile: a grid of elevation samples in meters.
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
        std::size_t getDataSize() const { return _heights.size() * sizeof(float) + sizeof(ElevationTileGrid); }

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
         * Decodes a DEM bitmap (mapbox/terrarium RGB encoded) into an elevation grid using
         * the given color component coefficients. Returns null if the bitmap has an unsupported format.
         */
        static std::shared_ptr<ElevationTileGrid> DecodeBitmap(const MapTile& tile, const MapBounds& internalBounds, const std::shared_ptr<Bitmap>& bitmap, const std::array<double, 4>& coeffs);

    private:
        float getHeight(int gx, int gy) const {
            return _heights[gy * _width + gx];
        }

        const MapTile _tile;
        const MapBounds _internalBounds;
        const int _width;
        const int _height;
        std::vector<float> _heights;
        float _minHeight;
        float _maxHeight;
    };
}

#endif
