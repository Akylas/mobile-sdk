#include "CacheTileDataSource.h"
#include "core/MapTile.h"
#include "core/Variant.h"
#include "datasources/components/TileData.h"
#include "components/Exceptions.h"
#include "utils/Log.h"

#include <memory>

namespace carto {
    
    CacheTileDataSource::CacheTileDataSource(const std::shared_ptr<TileDataSource>& dataSource) :
        TileDataSource(),
        _dataSource(dataSource)
    {
        if (!dataSource) {
            throw NullArgumentException("Null dataSource");
        }

        _dataSourceListener = std::make_shared<DataSourceListener>(*this);
        _dataSource->registerOnChangeListener(_dataSourceListener);
    }
    
    CacheTileDataSource::~CacheTileDataSource() {
        _dataSource->unregisterOnChangeListener(_dataSourceListener);
        _dataSourceListener.reset();
    }

    int CacheTileDataSource::getMinZoom() const {
        return _dataSource->getMinZoom();
    }

    int CacheTileDataSource::getMaxZoom() const {
        return _dataSource->getMaxZoom();
    }

    MapBounds CacheTileDataSource::getDataExtent() const {
        return _dataSource->getDataExtent();
    }

    std::string CacheTileDataSource::getEncoding() const {
        std::string encoding = TileDataSource::getEncoding();
        if (encoding.empty()) {
            encoding = _dataSource->getEncoding();
        }
        return encoding;
    }

    void CacheTileDataSource::applyCacheTileMetadata(const std::shared_ptr<TileData>& tileData, const MapTile& tile) const {
        if (!tileData) {
            return;
        }

        std::map<std::string, std::shared_ptr<Variant> > metadata = _dataSource->buildTileMetadata(tile);
        for (const auto& entry : TileDataSource::buildTileMetadata(tile)) {
            metadata[entry.first] = entry.second; // the cache's own settings win, as in getEncoding
        }
        for (const auto& entry : metadata) {
            tileData->setMetadata(entry.first, entry.second);
        }
    }

    void CacheTileDataSource::notifyTilesChanged(bool removeTiles) {
        clear();
        TileDataSource::notifyTilesChanged(removeTiles);
    }

    std::shared_ptr<TileDataSource> CacheTileDataSource::getDataSource() const {
        return _dataSource.get();
    }
    
    CacheTileDataSource::DataSourceListener::DataSourceListener(CacheTileDataSource& cacheDataSource) :
        _cacheDataSource(cacheDataSource)
    {
    }
    
    void CacheTileDataSource::DataSourceListener::onTilesChanged(bool removeTiles) {
        _cacheDataSource.notifyTilesChanged(removeTiles);
    }

}
