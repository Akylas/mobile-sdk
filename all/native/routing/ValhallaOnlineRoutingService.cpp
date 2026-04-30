#ifdef _CARTO_ROUTING_SUPPORT

#include "ValhallaOnlineRoutingService.h"
#include "components/Exceptions.h"
#include "routing/utils/ValhallaRoutingProxy.h"
#include "network/HTTPClient.h"
#include "utils/GeneralUtils.h"
#include "utils/NetworkUtils.h"
#include "utils/Log.h"

#include <map>

namespace carto {

    ValhallaOnlineRoutingService::ValhallaOnlineRoutingService(const std::string& apiKey) :
        _apiKey(apiKey),
        _profile("pedestrian"),
        _serviceURL(),
        _timeout(-1),
        _headers(),
        _mutex()
    {
    }
    ValhallaOnlineRoutingService::ValhallaOnlineRoutingService() :
        _profile("pedestrian"),
        _serviceURL(),
        _timeout(-1),
        _headers(),
        _mutex()
    {
    }

    ValhallaOnlineRoutingService::~ValhallaOnlineRoutingService() {
    }

    std::string ValhallaOnlineRoutingService::getCustomServiceURL() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _serviceURL;
    }

    void ValhallaOnlineRoutingService::setCustomServiceURL(const std::string& serviceURL) {
        std::lock_guard<std::mutex> lock(_mutex);
        _serviceURL = serviceURL;
    }

    std::string ValhallaOnlineRoutingService::getProfile() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _profile;
    }

    void ValhallaOnlineRoutingService::setProfile(const std::string& profile) {
        std::lock_guard<std::mutex> lock(_mutex);
        _profile = profile;
    }

    int ValhallaOnlineRoutingService::getTimeout() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _timeout;
    }

    void ValhallaOnlineRoutingService::setTimeout(int timeout) {
        std::lock_guard<std::mutex> lock(_mutex);
        _timeout = timeout;
    }

    std::map<std::string, std::string> ValhallaOnlineRoutingService::getHTTPHeaders() const {
        std::lock_guard<std::mutex> lock(_mutex);
        return _headers;
    }
    
    void ValhallaOnlineRoutingService::setHTTPHeaders(const std::map<std::string, std::string>& headers) {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _headers = headers;
        }
    }

    std::shared_ptr<RouteMatchingResult> ValhallaOnlineRoutingService::matchRoute(const std::shared_ptr<RouteMatchingRequest>& request) const {
        if (!request) {
            throw NullArgumentException("Null request");
        }

        std::string baseURL;

        std::map<std::string, std::string> params;
        std::map<std::string, std::string> headers;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            headers = _headers;
            std::map<std::string, std::string> tagMap;
            tagMap["service"] = "trace_attributes";
            if (!_apiKey.empty()) {
                tagMap["api_key"] = NetworkUtils::URLEncode(_apiKey);
            }
            baseURL = GeneralUtils::ReplaceTags(_serviceURL.empty() ? MAPBOX_SERVICE_URL : _serviceURL, tagMap);
        }

        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);

        HTTPClient httpClient(Log::IsShowDebug());
        httpClient.setTimeout(_timeout < 0 ? -1 : _timeout * 1000);
        return ValhallaRoutingProxy::MatchRoute(httpClient, url, getProfile(), request, headers);
    }

    std::shared_ptr<RoutingResult> ValhallaOnlineRoutingService::calculateRoute(const std::shared_ptr<RoutingRequest>& request) const {
        if (!request) {
            throw NullArgumentException("Null request");
        }

        std::string baseURL;

        std::map<std::string, std::string> params;
        std::map<std::string, std::string> headers;
        {
            std::lock_guard<std::mutex> lock(_mutex);

            headers = _headers;
            std::map<std::string, std::string> tagMap;
            tagMap["service"] = "route";
            if (!_apiKey.empty()) {
                tagMap["api_key"] = NetworkUtils::URLEncode(_apiKey);
            }
            baseURL = GeneralUtils::ReplaceTags(_serviceURL.empty() ? MAPBOX_SERVICE_URL : _serviceURL, tagMap);
        }

        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);

        HTTPClient httpClient(Log::IsShowDebug());
        httpClient.setTimeout(_timeout < 0 ? -1 : _timeout * 1000);
        return ValhallaRoutingProxy::CalculateRoute(httpClient, url, getProfile(), request, headers);
    }

    const std::string ValhallaOnlineRoutingService::MAPBOX_SERVICE_URL = "https://api.mapbox.com/valhalla/v1/{service}?access_token={api_key}";

}

#endif
