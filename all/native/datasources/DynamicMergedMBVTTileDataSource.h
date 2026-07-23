/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_DYNAMICMERGEDMBVTTILEDATASOURCE_H_
#define _CARTO_DYNAMICMERGEDMBVTTILEDATASOURCE_H_

#include "datasources/TileDataSource.h"
#include "components/DirectorPtr.h"

#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace carto {

    /**
     * A tile data source that merges a fixed base MBVT/protobuf source with a dynamically
     * mutable set of additional MBVT sources, keyed by name. Adding or removing a source
     * rebuilds an internal MergedMBVTTileDataSource chain and notifies listeners so tiles
     * are reloaded. Used internally by CompositeVectorTileLayer to support runtime add/remove
     * of merged vector sources (including ContourTileDataSource) while the owning layer's
     * data source pointer stays constant.
     *
     * As with MergedMBVTTileDataSource, the merged sources are assumed to have distinct
     * layer ids.
     */
    class DynamicMergedMBVTTileDataSource : public TileDataSource {
    public:
        explicit DynamicMergedMBVTTileDataSource(const std::shared_ptr<TileDataSource>& baseDataSource);
        virtual ~DynamicMergedMBVTTileDataSource();

        /**
         * Adds (or replaces, if the key already exists) a named MBVT source to merge on top
         * of the base source.
         */
        void addDataSource(const std::string& key, const std::shared_ptr<TileDataSource>& dataSource);
        /**
         * Removes the named source. Returns true if a source was removed.
         */
        bool removeDataSource(const std::string& key);
        /**
         * Returns true if a source with the given key is registered.
         */
        bool containsDataSource(const std::string& key) const;

        virtual int getMinZoom() const;
        virtual int getMaxZoom() const;
        virtual MapBounds getDataExtent() const;

        virtual std::shared_ptr<TileData> loadTile(const MapTile& tile);

    protected:
        class DataSourceListener : public TileDataSource::OnChangeListener {
        public:
            explicit DataSourceListener(DynamicMergedMBVTTileDataSource& dataSource);
            virtual void onTilesChanged(bool removeTiles);
        private:
            DynamicMergedMBVTTileDataSource& _dataSource;
        };

        // Rebuilds _mergedChain from _baseDataSource + _extraDataSources. Must hold _mutex.
        void rebuildChain();

        const DirectorPtr<TileDataSource> _baseDataSource;
        std::vector<std::pair<std::string, std::shared_ptr<TileDataSource> > > _extraDataSources;
        std::shared_ptr<TileDataSource> _mergedChain;

        mutable std::recursive_mutex _mutex;

    private:
        std::shared_ptr<DataSourceListener> _dataSourceListener;
    };

}

#endif
