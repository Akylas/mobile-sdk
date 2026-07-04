/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_TERRAINOPTIONS_H_
#define _CARTO_TERRAINOPTIONS_H_

#include "core/MapPos.h"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace carto {
    class TileDataSource;
    class ElevationDecoder;
    class ElevationManager;

    /**
     * 3D terrain configuration, attached to the map via Options::setTerrainOptions.
     * The elevation data source can be shared with a HillshadeRasterTileLayer, in which case
     * both features use the same tiles (ideally the data source should be wrapped in a
     * MemoryCacheTileDataSource to avoid duplicate loads).
     * Note: this class is experimental and may change or even be removed in future SDK versions.
     */
    class TerrainOptions {
    public:
        /**
         * Interface for monitoring terrain option change events. Internal.
         */
        struct OnChangeListener {
            virtual ~OnChangeListener() { }

            /**
             * Listener method that gets called when a terrain option has changed.
             * @param optionName The name of the option that has changed.
             */
            virtual void onTerrainOptionChanged(const std::string& optionName) = 0;
        };

        /**
         * Constructs a TerrainOptions object from an elevation data source.
         * The elevation decoder is resolved from the data source "encoding" setting
         * ("mapbox" or "terrarium"), defaulting to the MapBox encoding.
         * @param dataSource The data source with RGB-encoded elevation tiles.
         */
        explicit TerrainOptions(const std::shared_ptr<TileDataSource>& dataSource);
        /**
         * Constructs a TerrainOptions object from an elevation data source and an explicit decoder.
         * @param dataSource The data source with RGB-encoded elevation tiles.
         * @param elevationDecoder The decoder for the elevation tile encoding.
         */
        TerrainOptions(const std::shared_ptr<TileDataSource>& dataSource, const std::shared_ptr<ElevationDecoder>& elevationDecoder);
        virtual ~TerrainOptions();

        /**
         * Returns the elevation data source.
         * @return The elevation data source.
         */
        std::shared_ptr<TileDataSource> getDataSource() const;
        /**
         * Returns the elevation decoder.
         * @return The elevation decoder.
         */
        std::shared_ptr<ElevationDecoder> getElevationDecoder() const;

        /**
         * Returns the enabled state of the terrain.
         * @return True if 3D terrain rendering is enabled. The default is true.
         */
        bool isEnabled() const;
        /**
         * Sets the enabled state of the terrain. If disabled, the map renders flat,
         * but the elevation data source stays attached.
         * @param enabled The new enabled state.
         */
        void setEnabled(bool enabled);

        /**
         * Returns the terrain height exaggeration factor.
         * @return The exaggeration factor. The default is 1.0.
         */
        float getExaggeration() const;
        /**
         * Sets the terrain height exaggeration factor. 1.0 means true-to-scale heights.
         * Note: changing the exaggeration triggers a re-tesselation of loaded tiles, which is a relatively expensive operation.
         * @param exaggeration The new exaggeration factor.
         */
        void setExaggeration(float exaggeration);

        /**
         * Returns the terrain mesh resolution.
         * @return The maximum number of grid cells per tile edge used for terrain geometry. The default is 32.
         */
        int getMeshResolution() const;
        /**
         * Sets the terrain mesh resolution. Higher values give more detailed terrain
         * at the cost of memory and CPU. The effective resolution is also limited by
         * the resolution of the elevation tiles.
         * @param meshResolution The new mesh resolution (clamped to 2..256).
         */
        void setMeshResolution(int meshResolution);

        /**
         * Returns the minimum tile zoom level with 3D terrain.
         * @return The minimum zoom level. The default is 5.
         */
        int getMinZoom() const;
        /**
         * Sets the minimum tile zoom level with 3D terrain. Tiles below this zoom level render flat
         * and do not fetch elevation data. Terrain displacement is invisible at low zoom levels anyway,
         * so this limits the number of elevation tiles fetched and processed for far-away/zoomed-out views.
         * @param minZoom The new minimum zoom level (clamped to 0..24).
         */
        void setMinZoom(int minZoom);

        /**
         * Returns the clip-space depth bias used when depth-testing draped 2D geometry against the terrain.
         * @return The depth bias. The default is 0.0005.
         */
        float getDepthBias() const;
        /**
         * Sets the clip-space depth bias used when depth-testing draped 2D geometry against the terrain.
         * Larger values prevent draped layers from being clipped by the terrain surface itself,
         * at the cost of geometry slightly behind terrain ridges 'shining through' near silhouettes.
         * @param depthBias The new depth bias (clamped to 0..0.01).
         */
        void setDepthBias(float depthBias);

        /**
         * Returns the billboard/label terrain occlusion state.
         * @return True if billboards and labels hidden behind terrain are faded out. The default is true.
         */
        bool isBillboardOcclusionEnabled() const;
        /**
         * Sets the billboard/label terrain occlusion state.
         * @param enabled The new occlusion state.
         */
        void setBillboardOcclusionEnabled(bool enabled);

        /**
         * Returns the capacity of the decoded elevation tile cache in bytes.
         * @return The cache capacity in bytes. The default is 32MB.
         */
        std::size_t getElevationCacheCapacity() const;
        /**
         * Sets the capacity of the decoded elevation tile cache in bytes.
         * @param capacityInBytes The new cache capacity in bytes.
         */
        void setElevationCacheCapacity(std::size_t capacityInBytes);

        /**
         * Returns the terrain elevation in meters at the given position.
         * The position is expected to be in WGS84 coordinates.
         * Note: this method may block on network/IO if the elevation tile is not cached.
         * @param pos The position to query.
         * @return The elevation in meters, or -1000000 if no elevation data is available.
         */
        double getElevation(const MapPos& pos) const;
        /**
         * Returns terrain elevations in meters at the given positions (WGS84).
         * One value is returned for every input position, in the input order.
         * Note: this method may block on network/IO if the elevation tiles are not cached.
         * @param poses The positions to query.
         * @return The elevations in meters (-1000000 where no data is available).
         */
        std::vector<double> getElevations(const std::vector<MapPos>& poses) const;

        /**
         * Returns the elevation manager. Internal method.
         * @return The elevation manager.
         */
        std::shared_ptr<ElevationManager> getElevationManager() const;

        /**
         * Registers listener for terrain option change events. Internal method.
         * @param listener The listener for change events.
         */
        void registerOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);
        /**
         * Unregisters listener from terrain option change events. Internal method.
         * @param listener The previously added listener.
         */
        void unregisterOnChangeListener(const std::shared_ptr<OnChangeListener>& listener);

    private:
        void notifyOptionChanged(const std::string& optionName);

        const std::shared_ptr<TileDataSource> _dataSource;
        const std::shared_ptr<ElevationManager> _elevationManager;

        std::atomic<bool> _enabled;
        std::atomic<int> _meshResolution;
        std::atomic<int> _minZoom;
        std::atomic<float> _depthBias;
        std::atomic<bool> _billboardOcclusionEnabled;

        std::vector<std::shared_ptr<OnChangeListener> > _onChangeListeners;
        mutable std::mutex _onChangeListenersMutex;
    };
}

#endif
