#include "ValhallaRoutingService.h"
#include "utils/ValhallaRoutingProxy.h"
#include "../datasource/MBTilesDataSource.h"
#include "../exceptions/Exceptions.h"
#include "../log/Log.h"

#include <algorithm>
#include <picojson/picojson.h>
#include <sqlite3.h>

namespace routing {

    // -----------------------------------------------------------------------
    ValhallaRoutingService::ValhallaRoutingService(
            std::vector<std::shared_ptr<IDataSource>> sources) :
        _sources(std::move(sources)),
        _profile("pedestrian"),
        _configuration(ValhallaRoutingProxy::GetDefaultConfiguration())
    {
    }

    ValhallaRoutingService::~ValhallaRoutingService() = default;

    // -----------------------------------------------------------------------
    // Source management
    // -----------------------------------------------------------------------

    void ValhallaRoutingService::addSource(std::shared_ptr<IDataSource> source) {
        std::lock_guard<std::mutex> lk(_mutex);
        _sources.push_back(std::move(source));
    }

    bool ValhallaRoutingService::removeSource(const std::shared_ptr<IDataSource>& source) {
        std::lock_guard<std::mutex> lk(_mutex);
        auto it = std::find(_sources.begin(), _sources.end(), source);
        if (it == _sources.end()) return false;
        _sources.erase(it);
        return true;
    }

    std::vector<std::shared_ptr<IDataSource>> ValhallaRoutingService::getSources() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _sources;
    }

    // -----------------------------------------------------------------------
    // Profile
    // -----------------------------------------------------------------------

    std::string ValhallaRoutingService::getProfile() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _profile;
    }

    void ValhallaRoutingService::setProfile(const std::string& profile) {
        std::lock_guard<std::mutex> lk(_mutex);
        _profile = profile;
    }

    // -----------------------------------------------------------------------
    // Configuration (dot-delimited key access, no boost)
    // -----------------------------------------------------------------------

    std::vector<std::string> ValhallaRoutingService::splitKeys(const std::string& param) {
        std::vector<std::string> result;
        std::string cur;
        for (char c : param) {
            if (c == '.') { result.push_back(cur); cur.clear(); }
            else cur += c;
        }
        result.push_back(cur);
        return result;
    }

    Variant ValhallaRoutingService::getConfigurationParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lk(_mutex);
        auto keys = splitKeys(param);
        picojson::value sub = _configuration.toPicoJSON();
        for (const auto& key : keys) {
            if (!sub.is<picojson::object>()) return Variant();
            sub = sub.get(key);
        }
        return Variant::FromPicoJSON(sub);
    }

    void ValhallaRoutingService::setConfigurationParameter(const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lk(_mutex);
        auto keys = splitKeys(param);
        picojson::value config = _configuration.toPicoJSON();
        picojson::value* sub = &config;
        for (const auto& key : keys) {
            if (!sub->is<picojson::object>()) sub->set(picojson::object());
            sub = &sub->get<picojson::object>()[key];
        }
        *sub = value.toPicoJSON();
        _configuration = Variant::FromPicoJSON(config);
    }

    // -----------------------------------------------------------------------
    // Locale
    // -----------------------------------------------------------------------

    void ValhallaRoutingService::addLocale(const std::string& key, const std::string& json) {
        ValhallaRoutingProxy::AddLocale(key, json);
    }

    // -----------------------------------------------------------------------
    // Routing
    // -----------------------------------------------------------------------

    // Collect sqlite3 handles from all registered MBTilesDataSource instances.
    // Sources that do not expose a sqlite3* (e.g. future PMTiles sources) are
    // skipped — they should ultimately be surfaced to valhalla via the
    // IDataSource adapter mechanism.
    static std::vector<sqlite3*> collectDatabases(
            const std::vector<std::shared_ptr<IDataSource>>& sources) {
        std::vector<sqlite3*> dbs;
        for (const auto& src : sources) {
            auto* mbtiles = dynamic_cast<MBTilesDataSource*>(src.get());
            if (mbtiles) {
                sqlite3* db = mbtiles->getDatabase();
                if (db) dbs.push_back(db);
            }
        }
        return dbs;
    }

    std::shared_ptr<RoutingResult> ValhallaRoutingService::calculateRoute(
            const std::shared_ptr<RoutingRequest>& request) const {
        std::string profile;
        Variant config;
        std::vector<std::shared_ptr<IDataSource>> sources;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            profile  = _profile;
            config   = _configuration;
            sources  = _sources;
        }

        if (sources.empty()) {
            throw GenericException("No data sources registered");
        }
        auto databases = collectDatabases(sources);
        if (databases.empty()) {
            throw GenericException("No MBTiles data sources with open databases");
        }

        Log::debugf("ValhallaRoutingService::calculateRoute: profile=%s, sources=%d",
                    profile.c_str(), static_cast<int>(sources.size()));

        return ValhallaRoutingProxy::CalculateRoute(databases, profile, config, request);
    }

    std::shared_ptr<RouteMatchingResult> ValhallaRoutingService::matchRoute(
            const std::shared_ptr<RouteMatchingRequest>& request) const {
        std::string profile;
        Variant config;
        std::vector<std::shared_ptr<IDataSource>> sources;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            profile  = _profile;
            config   = _configuration;
            sources  = _sources;
        }

        if (sources.empty()) {
            throw GenericException("No data sources registered");
        }
        auto databases = collectDatabases(sources);
        if (databases.empty()) {
            throw GenericException("No MBTiles data sources with open databases");
        }

        Log::debugf("ValhallaRoutingService::matchRoute: profile=%s, sources=%d",
                    profile.c_str(), static_cast<int>(sources.size()));

        return ValhallaRoutingProxy::MatchRoute(databases, profile, config, request);
    }

} // namespace routing
