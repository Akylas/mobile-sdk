#include "ValhallaRoutingService.h"
#include "utils/ValhallaRoutingProxy.h"
#include "../datasource/MBTilesDataSource.h"
#include "../exceptions/Exceptions.h"
#include "../log/Log.h"
#include "../utils/StringUtils.h"

#include <algorithm>
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
    // Configuration (dot-delimited key access, pure Variant API)
    // -----------------------------------------------------------------------

    std::vector<std::string> ValhallaRoutingService::splitKeys(const std::string& param) {
        return routing::splitKeys(param, '.');
    }

    Variant ValhallaRoutingService::getConfigurationParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lk(_mutex);
        auto keys = splitKeys(param);
        Variant sub = _configuration;
        for (const auto& key : keys) {
            if (sub.getType() != VariantType::VARIANT_TYPE_OBJECT) return Variant();
            sub = sub.getObjectElement(key);
        }
        return sub;
    }

    // Recursively build a new Variant with the value set at the given key path.
    Variant ValhallaRoutingService::setNestedValue(Variant obj,
                                                    const std::vector<std::string>& keys,
                                                    int idx,
                                                    const Variant& value) {
        const std::string& key = keys[static_cast<std::size_t>(idx)];
        if (idx == static_cast<int>(keys.size()) - 1) {
            obj.setObjectElement(key, value);
            return obj;
        }
        Variant sub = obj.getObjectElement(key);
        if (sub.getType() != VariantType::VARIANT_TYPE_OBJECT) {
            sub = Variant(std::map<std::string, Variant>{});
        }
        obj.setObjectElement(key, setNestedValue(std::move(sub), keys, idx + 1, value));
        return obj;
    }

    void ValhallaRoutingService::setConfigurationParameter(const std::string& param,
                                                            const Variant& value) {
        std::lock_guard<std::mutex> lk(_mutex);
        auto keys = splitKeys(param);
        _configuration = setNestedValue(std::move(_configuration), keys, 0, value);
    }

    // -----------------------------------------------------------------------
    // Locale
    // -----------------------------------------------------------------------

    void ValhallaRoutingService::addLocale(const std::string& key, const std::string& json) {
        ValhallaRoutingProxy::AddLocale(key, json);
    }

    // -----------------------------------------------------------------------
    // Internal helpers
    // -----------------------------------------------------------------------

    // Collect sqlite3 handles from all registered MBTilesDataSource instances.
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

    // -----------------------------------------------------------------------
    // Routing API
    // -----------------------------------------------------------------------

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

        if (sources.empty()) throw GenericException("No data sources registered");
        auto databases = collectDatabases(sources);
        if (databases.empty()) throw GenericException("No MBTiles data sources with open databases");

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

        if (sources.empty()) throw GenericException("No data sources registered");
        auto databases = collectDatabases(sources);
        if (databases.empty()) throw GenericException("No MBTiles data sources with open databases");

        Log::debugf("ValhallaRoutingService::matchRoute: profile=%s, sources=%d",
                    profile.c_str(), static_cast<int>(sources.size()));

        return ValhallaRoutingProxy::MatchRoute(databases, profile, config, request);
    }

    std::string ValhallaRoutingService::callRaw(const std::string& endpoint,
                                                 const std::string& jsonBody) const {
        Variant config;
        std::vector<std::shared_ptr<IDataSource>> sources;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            config  = _configuration;
            sources = _sources;
        }

        if (sources.empty()) throw GenericException("No data sources registered");
        auto databases = collectDatabases(sources);
        if (databases.empty()) throw GenericException("No MBTiles data sources with open databases");

        Log::debugf("ValhallaRoutingService::callRaw: endpoint=%s, sources=%d",
                    endpoint.c_str(), static_cast<int>(sources.size()));

        return ValhallaRoutingProxy::CallRaw(databases, config, endpoint, jsonBody);
    }

} // namespace routing
