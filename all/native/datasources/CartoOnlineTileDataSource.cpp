#include "CartoOnlineTileDataSource.h"
#include "core/BinaryData.h"
#include "core/MapTile.h"
#include "components/LicenseManager.h"
#include "utils/Log.h"
#include "utils/GeneralUtils.h"
#include "utils/PlatformUtils.h"
#include "utils/NetworkUtils.h"

#include <picojson/picojson.h>

namespace carto {
    
    CartoOnlineTileDataSource::CartoOnlineTileDataSource(const std::string& source) :
        TileDataSource(),
        _source(source),
        _cache(DEFAULT_CACHED_TILES),
        _httpClient(Log::IsShowDebug()),
        _tmsScheme(false),
        _tileURLs(),
        _randomGenerator(),
        _mutex()
    {
        _maxZoom = (isMapZenSource() ? 17 : 14);
    }
    
    CartoOnlineTileDataSource::~CartoOnlineTileDataSource() {
    }

    std::shared_ptr<TileData> CartoOnlineTileDataSource::loadTile(const MapTile& mapTile) {
        std::unique_lock<std::recursive_mutex> lock(_mutex);

        std::shared_ptr<TileData> tileData;
        if (_cache.read(mapTile.getTileId(), tileData)) {
            if (tileData->getMaxAge() != 0) {
                return tileData;
            }
            _cache.remove(mapTile.getTileId());
        }

        if (_tileURLs.empty()) {
            if (!loadTileURLs()) {
                return std::shared_ptr<TileData>();
            }
        }
        std::size_t randomIndex = std::uniform_int_distribution<std::size_t>(0, _tileURLs.size() - 1)(_randomGenerator);
        std::string tileURL = _tileURLs[randomIndex];

        lock.unlock();
        tileData = loadOnlineTile(tileURL, mapTile);
        lock.lock();
        
        if (tileData) {
            if (tileData->getMaxAge() != 0 && tileData->getData() && !tileData->isReplaceWithParent()) {
                _cache.put(mapTile.getTileId(), tileData, 1);
            }
        }
        
        return tileData;
    }

    bool CartoOnlineTileDataSource::isMapZenSource() const {
        return _source.substr(0, 7) == "mapzen.";
    }

    std::string CartoOnlineTileDataSource::buildTileURL(const std::string& baseURL, const MapTile& tile) const {
        std::string appToken;
        if (!LicenseManager::GetInstance().getParameter("appToken", appToken, true)) {
            return std::string();
        }

        std::map<std::string, std::string> tagValues = buildTagValues(_tmsScheme ? tile.getFlipped() : tile);
        tagValues["key"] = appToken;
        return GeneralUtils::ReplaceTags(baseURL, tagValues, "{", "}", true);
    }

    bool CartoOnlineTileDataSource::loadTileURLs() {
        std::map<std::string, std::string> params;
        params["deviceId"] = PlatformUtils::GetDeviceId();
        params["platform"] = PlatformUtils::GetPlatformId();
        params["sdk_build"] = _CARTO_MOBILE_SDK_VERSION;
        std::string appToken;
        if (LicenseManager::GetInstance().getParameter("appToken", appToken, true)) {
            params["appToken"] = appToken;
        }

        std::string baseURL = TILE_SERVICE_URL + NetworkUtils::URLEncode(_source) + "/1/tiles.json";
        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);
        Log::Debugf("CartoOnlineTileDataSource::loadTileURLs: Loading %s", url.c_str());

        std::shared_ptr<BinaryData> responseData;
        if (!NetworkUtils::GetHTTP(url, responseData, Log::IsShowDebug())) {
            Log::Error("CartoOnlineTileDataSource: Failed to fetch tile source configuration");
            return false;
        }
           
        std::string result(reinterpret_cast<const char*>(responseData->data()), responseData->size());
        picojson::value config;
        std::string err = picojson::parse(config, result);
        if (!err.empty()) {
            Log::Errorf("CartoOnlineTileDataSource: Failed to parse tile source configuration: %s", err.c_str());
            return false;
        }

        if (!config.get("tiles").is<picojson::array>()) {
            Log::Error("CartoOnlineTileDataSource: Tile URLs missing from configuration");
            return false;
        }
        for (const picojson::value& val : config.get("tiles").get<picojson::array>()) {
            if (val.is<std::string>()) {
                _tileURLs.push_back(val.get<std::string>());
            }
        }

        if (config.get("scheme").is<std::string>()) {
            _tmsScheme = config.get("scheme").get<std::string>() == "tms";
        }
        if (config.get("minzoom").is<std::int64_t>()) {
            int minZoom = static_cast<int>(config.get("minzoom").get<std::int64_t>());
            if (minZoom != _minZoom) {
                _minZoom = minZoom;
                notifyTilesChanged(false);
            }
        }
        if (config.get("maxzoom").is<std::int64_t>()) {
            int maxZoom = static_cast<int>(config.get("maxzoom").get<std::int64_t>());
            if (maxZoom != _maxZoom) {
                _maxZoom = maxZoom;
                notifyTilesChanged(false);
            }
        }
        return !_tileURLs.empty();
    }
    
    std::shared_ptr<TileData> CartoOnlineTileDataSource::loadOnlineTile(const std::string& tileURL, const MapTile& mapTile) {
        Log::Infof("CartoOnlineTileDataSource::loadOnlineTile: Loading tile %d/%d/%d", mapTile.getZoom(), mapTile.getX(), mapTile.getY());

        std::string url = buildTileURL(tileURL, mapTile);
        if (url.empty()) {
            Log::Error("CartoOnlineTileDataSource::loadOnlineTile: Online service not available (license issue?)");
            return std::shared_ptr<TileData>();
        }
        Log::Debugf("CartoOnlineTileDataSource::loadOnlineTile: Loading %s", url.c_str());

        std::map<std::string, std::string> requestHeaders;
        std::map<std::string, std::string> responseHeaders;
        std::shared_ptr<BinaryData> responseData;
        int statusCode = -1;
        try {
            if (_httpClient.get(url, requestHeaders, responseHeaders, responseData, &statusCode) != 0) {
                Log::Errorf("CartoOnlineTileDataSource::loadOnlineTile: Failed to load tile %d/%d/%d", mapTile.getZoom(), mapTile.getX(), mapTile.getY());
                return std::shared_ptr<TileData>();
            }
        } catch (const std::exception& ex) {
            Log::Errorf("CartoOnlineTileDataSource::loadOnlineTile: Exception while loading tile %d/%d/%d: %s", mapTile.getZoom(), mapTile.getX(), mapTile.getY(), ex.what());
            return std::shared_ptr<TileData>();
        }
        int maxAge = NetworkUtils::GetMaxAgeHTTPHeader(responseHeaders);
        auto tileData = std::make_shared<TileData>(responseData);
        if (maxAge >= 0 && !isMapZenSource()) { // set max age header but only for non-mapzen sources as mapzen always sets max-age to 0
            Log::Infof("CartoOnlineTileDataSource::loadOnlineTile: Setting tile %d/%d/%d maxage=%d", mapTile.getZoom(), mapTile.getX(), mapTile.getY(), maxAge);
            tileData->setMaxAge(maxAge * 1000);
        }
        if (statusCode == 204) { // special case - empty tile, replace with parent tile
            Log::Infof("CartoOnlineTileDataSource::loadOnlineTile: Replacing tile %d/%d/%d with parent", mapTile.getZoom(), mapTile.getX(), mapTile.getY());
            tileData->setReplaceWithParent(true);
        }
        return tileData;
    }

    const int CartoOnlineTileDataSource::DEFAULT_CACHED_TILES = 8;

    const std::string CartoOnlineTileDataSource::TILE_SERVICE_URL = "http://api.nutiteq.com/v2/";
    
}
