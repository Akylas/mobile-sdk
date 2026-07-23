#include "DynamicMergedMBVTTileDataSource.h"
#include "datasources/MergedMBVTTileDataSource.h"
#include "core/MapTile.h"
#include "components/Exceptions.h"

#include <algorithm>

namespace carto {

    DynamicMergedMBVTTileDataSource::DynamicMergedMBVTTileDataSource(const std::shared_ptr<TileDataSource>& baseDataSource) :
        TileDataSource(),
        _baseDataSource(baseDataSource),
        _extraDataSources(),
        _mergedChain(baseDataSource),
        _mutex()
    {
        if (!baseDataSource) {
            throw NullArgumentException("Null baseDataSource");
        }
        _dataSourceListener = std::make_shared<DataSourceListener>(*this);
        _baseDataSource->registerOnChangeListener(_dataSourceListener);
    }

    DynamicMergedMBVTTileDataSource::~DynamicMergedMBVTTileDataSource() {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        for (const auto& entry : _extraDataSources) {
            entry.second->unregisterOnChangeListener(_dataSourceListener);
        }
        _baseDataSource->unregisterOnChangeListener(_dataSourceListener);
        _dataSourceListener.reset();
    }

    void DynamicMergedMBVTTileDataSource::addDataSource(const std::string& key, const std::shared_ptr<TileDataSource>& dataSource) {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            auto it = std::find_if(_extraDataSources.begin(), _extraDataSources.end(), [&](const auto& entry) { return entry.first == key; });
            if (it != _extraDataSources.end()) {
                if (it->second == dataSource) {
                    return;
                }
                it->second->unregisterOnChangeListener(_dataSourceListener);
                it->second = dataSource;
            } else {
                _extraDataSources.emplace_back(key, dataSource);
            }
            dataSource->registerOnChangeListener(_dataSourceListener);
            rebuildChain();
        }
        notifyTilesChanged(true);
    }

    bool DynamicMergedMBVTTileDataSource::removeDataSource(const std::string& key) {
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            auto it = std::find_if(_extraDataSources.begin(), _extraDataSources.end(), [&](const auto& entry) { return entry.first == key; });
            if (it == _extraDataSources.end()) {
                return false;
            }
            it->second->unregisterOnChangeListener(_dataSourceListener);
            _extraDataSources.erase(it);
            rebuildChain();
        }
        notifyTilesChanged(true);
        return true;
    }

    bool DynamicMergedMBVTTileDataSource::containsDataSource(const std::string& key) const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return std::any_of(_extraDataSources.begin(), _extraDataSources.end(), [&](const auto& entry) { return entry.first == key; });
    }

    void DynamicMergedMBVTTileDataSource::rebuildChain() {
        std::shared_ptr<TileDataSource> chain = _baseDataSource.get();
        for (const auto& entry : _extraDataSources) {
            chain = std::make_shared<MergedMBVTTileDataSource>(chain, entry.second);
        }
        _mergedChain = chain;
    }

    int DynamicMergedMBVTTileDataSource::getMinZoom() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _mergedChain->getMinZoom();
    }

    int DynamicMergedMBVTTileDataSource::getMaxZoom() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _mergedChain->getMaxZoom();
    }

    MapBounds DynamicMergedMBVTTileDataSource::getDataExtent() const {
        std::lock_guard<std::recursive_mutex> lock(_mutex);
        return _mergedChain->getDataExtent();
    }

    std::shared_ptr<TileData> DynamicMergedMBVTTileDataSource::loadTile(const MapTile& tile) {
        std::shared_ptr<TileDataSource> chain;
        {
            std::lock_guard<std::recursive_mutex> lock(_mutex);
            chain = _mergedChain;
        }
        return chain->loadTile(tile);
    }

    DynamicMergedMBVTTileDataSource::DataSourceListener::DataSourceListener(DynamicMergedMBVTTileDataSource& dataSource) :
        _dataSource(dataSource)
    {
    }

    void DynamicMergedMBVTTileDataSource::DataSourceListener::onTilesChanged(bool removeTiles) {
        _dataSource.notifyTilesChanged(removeTiles);
    }

}
