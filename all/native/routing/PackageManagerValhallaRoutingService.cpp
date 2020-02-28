#if defined(_CARTO_ROUTING_SUPPORT) && defined(_CARTO_VALHALLA_ROUTING_SUPPORT) && defined(_CARTO_PACKAGEMANAGER_SUPPORT)

#include "PackageManagerValhallaRoutingService.h"
#include "components/Exceptions.h"
#include "packagemanager/PackageInfo.h"
#include "packagemanager/handlers/ValhallaRoutingPackageHandler.h"
#include "projections/Projection.h"
#include "routing/ValhallaRoutingProxy.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <boost/algorithm/string.hpp>

namespace carto {

    PackageManagerValhallaRoutingService::PackageManagerValhallaRoutingService(const std::shared_ptr<PackageManager>& packageManager) :
        RoutingService(),
        _packageManager(packageManager),
        _profile("pedestrian"),
        _configuration(ValhallaRoutingProxy::GetDefaultConfiguration()),
        _cachedPackageDatabases(),
        _mutex()
    {
        if (!packageManager) {
            throw NullArgumentException("Null packageManager");
        }

        _packageManagerListener = std::make_shared<PackageManagerListener>(*this);
        _packageManager->registerOnChangeListener(_packageManagerListener);
    }

    PackageManagerValhallaRoutingService::~PackageManagerValhallaRoutingService() {
        _packageManager->unregisterOnChangeListener(_packageManagerListener);
        _packageManagerListener.reset();
    }

    std::string PackageManagerValhallaRoutingService::getProfile() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _profile;
    }

    void PackageManagerValhallaRoutingService::setProfile(const std::string& profile) {
        std::lock_guard<std::mutex> lock(_mutex);
        _profile = profile;
    }

    Variant PackageManagerValhallaRoutingService::getConfigurationParameter(const std::string& param) const {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> keys;
        boost::split(keys, param, boost::is_any_of("."));
        picojson::value subValue = _configuration.toPicoJSON();
        for (const std::string& key : keys) {
            if (!subValue.is<picojson::object>()) {
                return Variant();
            }
            subValue = subValue.get(key);
        }
        return Variant::FromPicoJSON(subValue);
    }

    void PackageManagerValhallaRoutingService::setConfigurationParameter(const std::string& param, const Variant& value) {
        std::lock_guard<std::mutex> lock(_mutex);
        std::vector<std::string> keys;
        boost::split(keys, param, boost::is_any_of("."));
        picojson::value config = _configuration.toPicoJSON();
        picojson::value* subValue = &config;
        for (const std::string& key : keys) {
            if (!subValue->is<picojson::object>()) {
                subValue->set(picojson::object());
            }
            subValue = &subValue->get<picojson::object>()[key];
        }
        *subValue = value.toPicoJSON();
        _configuration = Variant::FromPicoJSON(config);
    }

    std::shared_ptr<RouteMatchingResult> PackageManagerValhallaRoutingService::matchRoute(const std::shared_ptr<RouteMatchingRequest>& request) const {
        if (!request) {
            throw NullArgumentException("Null request");
        }

        // Do routing via package manager, so that all packages are locked during routing
        std::shared_ptr<RouteMatchingResult> result;
        _packageManager->accessLocalPackages([this, &result, &request](const std::map<std::shared_ptr<PackageInfo>, std::shared_ptr<PackageHandler> >& packageHandlerMap) {
            // Build map of routing packages and graph files
            std::vector<std::shared_ptr<sqlite3pp::database> > packageDatabases;
            for (auto it = packageHandlerMap.begin(); it != packageHandlerMap.end(); it++) {
                if (auto valhallaRoutingHandler = std::dynamic_pointer_cast<ValhallaRoutingPackageHandler>(it->second)) {
                    if (std::shared_ptr<sqlite3pp::database> database = valhallaRoutingHandler->getDatabase()) {
                        packageDatabases.push_back(database);
                    }
                }
            }

            // Now check if we have already a cached route finder for the files. If not, create new instance.
            std::lock_guard<std::mutex> lock(_mutex);
            if (packageDatabases != _cachedPackageDatabases) {
                _cachedPackageDatabases = packageDatabases;
            }

            result = ValhallaRoutingProxy::MatchRoute(_cachedPackageDatabases, _profile, _configuration, request);
        });

        return result;
    }

    std::shared_ptr<RoutingResult> PackageManagerValhallaRoutingService::calculateRoute(const std::shared_ptr<RoutingRequest>& request) const {
        if (!request) {
            throw NullArgumentException("Null request");
        }

        // Do routing via package manager, so that all packages are locked during routing
        std::shared_ptr<RoutingResult> result;
        _packageManager->accessLocalPackages([this, &result, &request](const std::map<std::shared_ptr<PackageInfo>, std::shared_ptr<PackageHandler> >& packageHandlerMap) {
            // Build map of routing packages and graph files
            std::vector<std::shared_ptr<sqlite3pp::database> > packageDatabases;
            for (auto it = packageHandlerMap.begin(); it != packageHandlerMap.end(); it++) {
                if (auto valhallaRoutingHandler = std::dynamic_pointer_cast<ValhallaRoutingPackageHandler>(it->second)) {
                    if (std::shared_ptr<sqlite3pp::database> database = valhallaRoutingHandler->getDatabase()) {
                        packageDatabases.push_back(database);
                    }
                }
            }

            // Now check if we have already a cached route finder for the files. If not, create new instance.
            std::lock_guard<std::mutex> lock(_mutex);
            if (packageDatabases != _cachedPackageDatabases) {
                _cachedPackageDatabases = packageDatabases;
            }

            result = ValhallaRoutingProxy::CalculateRoute(_cachedPackageDatabases, _profile, _configuration, request);
        });

        return result;
    }
            
    PackageManagerValhallaRoutingService::PackageManagerListener::PackageManagerListener(PackageManagerValhallaRoutingService& service) :
        _service(service)
    {
    }
        
    void PackageManagerValhallaRoutingService::PackageManagerListener::onPackagesChanged() {
        std::lock_guard<std::mutex> lock(_service._mutex);
        _service._cachedPackageDatabases.clear();
    }

    void PackageManagerValhallaRoutingService::PackageManagerListener::onStylesChanged() {
        // Impossible
    }

}

#endif
