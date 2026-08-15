/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_CACHETILEDATASOURCE_H_
#define _CARTO_CACHETILEDATASOURCE_H_

#include "datasources/TileDataSource.h"
#include "components/DirectorPtr.h"

namespace carto {
    
    /**
     * A tile data source that loads tiles from another tile data source and caches them.
     */
    class CacheTileDataSource : public TileDataSource {
    public:
        virtual ~CacheTileDataSource();

        virtual int getMinZoom() const;
        virtual int getMaxZoom() const;

        virtual MapBounds getDataExtent() const;

        /**
         * Returns the encoding of this data source, falling back to the encoding
         * of the wrapped data source when not explicitly set on the cache.
         * @return The encoding type, or empty string if not set.
         */
        virtual std::string getEncoding() const;

        virtual std::string getMetaData(const std::string& key) const;

        virtual void notifyTilesChanged(bool removeTiles);

        /**
         * Returns the original data source that the cache uses.
         * @return The original data source.
         */
        std::shared_ptr<TileDataSource> getDataSource() const;
        
        /**
         * Clear the cache.
         */
        virtual void clear() = 0;
        
        /**
         * Returns the tile cache capacity.
         * @return The tile cache capacity in bytes.
         */
        virtual std::size_t getCapacity() const = 0;        
        /**
         * Sets the cache capacity.
         * @param capacityInBytes The new tile cache capacity in bytes.
         */
        virtual void setCapacity(std::size_t capacityInBytes) = 0;

    protected:
        class DataSourceListener : public TileDataSource::OnChangeListener {
        public:
            explicit DataSourceListener(CacheTileDataSource& cacheDataSource);
            
            virtual void onTilesChanged(bool removeTiles);
            
        private:
            CacheTileDataSource& _cacheDataSource;
        };
        
        CacheTileDataSource(const std::shared_ptr<TileDataSource>& dataSource);

        /**
         * Applies the metadata of the wrapped data source to a tile, overridden by the metadata
         * explicitly set on the cache itself. This mirrors getEncoding: a cache is transparent
         * unless it is configured explicitly. Does nothing if the tile is null.
         * Note: deliberately not implemented by overriding buildTileMetadata. A cache data source
         * constructed from Java/C# is a SWIG director object whose generated buildTileMetadata
         * stub calls TileDataSource::buildTileMetadata directly, so an override here would never
         * run for the objects applications actually create.
         * @param tileData The tile data to apply the metadata to.
         * @param tile The tile for which the metadata should be built.
         */
        void applyCacheTileMetadata(const std::shared_ptr<TileData>& tileData, const MapTile& tile) const;

        const DirectorPtr<TileDataSource> _dataSource;
        
    private:
        std::shared_ptr<DataSourceListener> _dataSourceListener;
    };
    
}

#endif
