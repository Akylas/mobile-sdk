#include "ValhallaRoutingService.h"
#include "utils/ValhallaRoutingProxy.h"
#include "../exceptions/Exceptions.h"
#include "../log/Log.h"
#include "../utils/StringUtils.h"

#include <sqlite3.h>
#include <stdexcept>

namespace routing {

    // -----------------------------------------------------------------------
    ValhallaRoutingService::ValhallaRoutingService(std::vector<std::string> paths) :
        _paths(std::move(paths)),
        _profile("pedestrian"),
        _configuration(ValhallaRoutingProxy::GetDefaultConfiguration())
    {
    }

    ValhallaRoutingService::~ValhallaRoutingService() {
        // Close any databases that may still be open (should only happen if the
        // service is destroyed while a request is in flight, which is API misuse).
        std::lock_guard<std::mutex> lk(_mutex);
        for (auto* db : _openDbs) sqlite3_close(db);
        _openDbs.clear();
    }

    // -----------------------------------------------------------------------
    // Path management
    // -----------------------------------------------------------------------

    void ValhallaRoutingService::addMBTilesPath(const std::string& path) {
        std::lock_guard<std::mutex> lk(_mutex);
        _paths.push_back(path);
    }

    std::vector<std::string> ValhallaRoutingService::getMBTilesPaths() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _paths;
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
    // Database lifecycle helpers
    //
    // Databases are opened lazily when the first request arrives (_activeCount
    // goes from 0→1) and closed when the last request completes (1→0).
    // All access to _paths, _openDbs, _activeCount is guarded by _mutex.
    // -----------------------------------------------------------------------

    std::vector<sqlite3*> ValhallaRoutingService::acquireDatabases() const {
        std::lock_guard<std::mutex> lk(_mutex);

        if (_paths.empty()) {
            throw GenericException("No MBTiles databases registered");
        }

        if (_activeCount == 0) {
            // Open all registered databases.
            std::vector<sqlite3*> newDbs;
            newDbs.reserve(_paths.size());
            try {
                for (const auto& path : _paths) {
                    sqlite3* db = nullptr;
                    int rc = sqlite3_open_v2(path.c_str(), &db,
                                             SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX,
                                             nullptr);
                    if (rc != SQLITE_OK) {
                        std::string err = db ? sqlite3_errmsg(db) : "unknown error";
                        if (db) sqlite3_close(db);
                        throw std::runtime_error("Cannot open '" + path + "': " + err);
                    }
                    // Performance tuning for read-only access
                    sqlite3_exec(db, "PRAGMA journal_mode=OFF;", nullptr, nullptr, nullptr);
                    sqlite3_exec(db, "PRAGMA temp_store=MEMORY;", nullptr, nullptr, nullptr);
                    sqlite3_exec(db, "PRAGMA synchronous=OFF;",  nullptr, nullptr, nullptr);
                    sqlite3_exec(db, "PRAGMA cache_size=2000;",  nullptr, nullptr, nullptr);
                    newDbs.push_back(db);
                }
            } catch (...) {
                // Clean up any databases we opened before the failure.
                for (auto* db : newDbs) sqlite3_close(db);
                throw;
            }
            _openDbs = std::move(newDbs);
        }

        ++_activeCount;
        return _openDbs;  // copy of pointers; valid while _activeCount > 0
    }

    void ValhallaRoutingService::releaseDatabases() const {
        std::lock_guard<std::mutex> lk(_mutex);
        if (--_activeCount == 0) {
            for (auto* db : _openDbs) sqlite3_close(db);
            _openDbs.clear();
        }
    }

    // -----------------------------------------------------------------------
    // Routing API
    // -----------------------------------------------------------------------

    std::string ValhallaRoutingService::callRaw(const std::string& endpoint,
                                                 const std::string& jsonBody) const {
        // Snapshot mutable state under the lock.
        std::string profile;
        Variant config;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            profile = _profile;
            config  = _configuration;
        }

        // Inject "costing" from the service profile if the caller has not
        // already included it in the request body.
        std::string body = jsonBody;
        if (!profile.empty() &&
            body.find("\"costing\"") == std::string::npos) {
            // Insert right after the opening '{' of the JSON object.
            auto pos = body.find('{');
            if (pos != std::string::npos) {
                body.insert(pos + 1, "\"costing\":\"" + profile + "\",");
            }
        }

        // Open (or share already-open) databases for this request.
        auto dbs = acquireDatabases();

        // RAII guard: decrement active count and conditionally close DBs.
        struct ReleaseGuard {
            const ValhallaRoutingService& svc;
            ~ReleaseGuard() { svc.releaseDatabases(); }
        } guard{*this};

        Log::debugf("ValhallaRoutingService::callRaw: endpoint=%s, dbs=%d",
                    endpoint.c_str(), static_cast<int>(dbs.size()));

        return ValhallaRoutingProxy::CallRaw(dbs, config, endpoint, body);
    }

} // namespace routing
