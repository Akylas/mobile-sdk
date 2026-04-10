#include "ValhallaRoutingService.h"
#include "utils/ValhallaRoutingProxy.h"
#include "../exceptions/Exceptions.h"
#include "../log/Log.h"
#include "../utils/StringUtils.h"

#include <picojson/picojson.h>
#include <sqlite3pp.h>
#include <stdexcept>
#include <functional>


#include <valhalla/midgard/encoded.h>
#include <valhalla/midgard/pointll.h>
#include <charconv>
#include <cstdio>

namespace routing {

    // -----------------------------------------------------------------------
    ValhallaRoutingService::ValhallaRoutingService(std::vector<std::string> paths) :
        _paths(std::move(paths)),
        _profile("pedestrian"),
        _configuration(ValhallaRoutingProxy::GetDefaultConfiguration())
    {
    }

    ValhallaRoutingService::~ValhallaRoutingService() {
        // Close any databases that may still be open. Should only happen if the
        // service is destroyed while a request is in flight (API misuse).
        std::lock_guard<std::mutex> lk(_mutex);
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
    // Configuration (dot-delimited key access, picojson)
    // -----------------------------------------------------------------------

    std::vector<std::string> ValhallaRoutingService::splitKeys(const std::string& param) {
        return routing::splitKeys(param, '.');
    }

    std::string ValhallaRoutingService::getConfigurationParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lk(_mutex);
        picojson::value v;
        std::string err = picojson::parse(v, _configuration);
        if (!err.empty() || !v.is<picojson::object>()) return {};

        auto keys = splitKeys(param);
        const picojson::value* cur = &v;
        for (const auto& key : keys) {
            if (!cur->is<picojson::object>()) return {};
            const auto& obj = cur->get<picojson::object>();
            auto it = obj.find(key);
            if (it == obj.end()) return {};
            cur = &it->second;
        }
        return cur->serialize();
    }

    void ValhallaRoutingService::setConfigurationParameter(const std::string& param,
                                                            const std::string& valueJson) {
        picojson::value newVal;
        {
            std::string err = picojson::parse(newVal, valueJson);
            if (!err.empty()) {
                throw std::runtime_error("Invalid JSON value for '" + param + "': " + err);
            }
        }

        std::lock_guard<std::mutex> lk(_mutex);
        picojson::value v;
        std::string err = picojson::parse(v, _configuration);
        if (!err.empty() || !v.is<picojson::object>()) return;

        auto keys = splitKeys(param);
        std::function<void(picojson::value&, int)> setNested = [&](picojson::value& node, int idx) {
            if (!node.is<picojson::object>()) {
                node = picojson::value(picojson::object());
            }
            auto& obj = node.get<picojson::object>();
            if (idx == static_cast<int>(keys.size()) - 1) {
                obj[keys[static_cast<std::size_t>(idx)]] = newVal;
            } else {
                setNested(obj[keys[static_cast<std::size_t>(idx)]], idx + 1);
            }
        };
        setNested(v, 0);
        _configuration = v.serialize();
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
    // Valhalla's GraphReader requires sqlite3pp::database objects.
    // All access to _paths, _openDbs, _activeCount is guarded by _mutex.
    // -----------------------------------------------------------------------

    std::vector<std::shared_ptr<sqlite3pp::database>> ValhallaRoutingService::acquireDatabases() const {
        std::lock_guard<std::mutex> lk(_mutex);

        if (_paths.empty()) {
            throw GenericException("No MBTiles databases registered");
        }

        if (_activeCount == 0) {
            // Open all registered databases.
            std::vector<std::shared_ptr<sqlite3pp::database>> newDbs;
            newDbs.reserve(_paths.size());
            try {
                for (const auto& path : _paths) {
                    auto db = std::make_shared<sqlite3pp::database>();
                    int rc = db->connect_v2(path.c_str(),
                                            SQLITE_OPEN_READONLY | SQLITE_OPEN_FULLMUTEX);
                    if (rc != SQLITE_OK) {
                        throw std::runtime_error("Cannot open '" + path + "': sqlite error " +
                                                 std::to_string(rc));
                    }
                    // Performance tuning for read-only access
                    db->execute("PRAGMA journal_mode=OFF");
                    db->execute("PRAGMA temp_store=MEMORY");
                    db->execute("PRAGMA synchronous=OFF");
                    db->execute("PRAGMA cache_size=2000");
                    newDbs.push_back(std::move(db));
                }
            } catch (...) {
                // newDbs' shared_ptrs will clean up on destruction
                throw;
            }
            _openDbs = std::move(newDbs);
        }

        ++_activeCount;
        return _openDbs;  // copy of shared_ptrs; valid while _activeCount > 0
    }

    void ValhallaRoutingService::releaseDatabases() const {
        std::lock_guard<std::mutex> lk(_mutex);
        if (--_activeCount == 0) {
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
        std::string config;
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

    std::string ValhallaRoutingService::parseShape(const std::string &shapeStr) {
        // Decode the compact valhalla shape into a vector of PointLL (lon, lat)
        auto shape = valhalla::midgard::decode<std::vector<valhalla::midgard::PointLL> >(shapeStr);

        // Fast path for empty shape
        if (shape.empty()) return "[]";

        // Build JSON array: [[lon,lat],[lon,lat],...]
        // Use a small stack buffer and std::to_chars for fast number -> chars conversion.
        std::string out;
        out.reserve(shape.size() * 24); // heuristic reserve to avoid many reallocs
        out.push_back('[');

        char buf[64]; // enough for double textual representation
        for (std::size_t i = 0; i < shape.size(); ++i) {
            if (i) out.push_back(',');
            out.push_back('[');

            // valhalla::midgard::PointLL is typedef'd as std::pair<float,float>
            // where first = lon, second = lat
            double lon = static_cast<double>(shape[i].first);
            double lat = static_cast<double>(shape[i].second);

            // Convert lon
            auto res1 = std::to_chars(buf, buf + sizeof(buf), lon);
            if (res1.ec == std::errc()) {
                out.append(buf, res1.ptr);
            } else {
                // fallback: snprintf with 6 fractional digits
                int n = std::snprintf(buf, sizeof(buf), "%.6f", lon);
                if (n > 0) out.append(buf, static_cast<std::size_t>(n));
            }

            out.push_back(',');

            // Convert lat
            auto res2 = std::to_chars(buf, buf + sizeof(buf), lat);
            if (res2.ec == std::errc()) {
                out.append(buf, res2.ptr);
            } else {
                int n = std::snprintf(buf, sizeof(buf), "%.6f", lat);
                if (n > 0) out.append(buf, static_cast<std::size_t>(n));
            }

            out.push_back(']');
        }

        out.push_back(']');
        return out;
    }

} // namespace routing
