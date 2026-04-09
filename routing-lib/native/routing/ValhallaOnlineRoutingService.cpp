#include "ValhallaOnlineRoutingService.h"
#include "utils/ValhallaRoutingProxy.h"
#include "../exceptions/Exceptions.h"
#include "../log/Log.h"

#ifdef ROUTING_WITH_HTTP_CLIENT
#  include "../network/HTTPClient.h"
#endif

namespace routing {

#ifdef ROUTING_WITH_HTTP_CLIENT
    ValhallaOnlineRoutingService::ValhallaOnlineRoutingService(
            const std::string& baseURL) :
        _baseURL(baseURL),
        _profile("pedestrian"),
        _handler()
    {
    }
#endif

    ValhallaOnlineRoutingService::ValhallaOnlineRoutingService(
            const std::string& baseURL,
            HttpHandler handler) :
        _baseURL(baseURL),
        _profile("pedestrian"),
        _handler(std::move(handler))
    {
    }

    ValhallaOnlineRoutingService::~ValhallaOnlineRoutingService() = default;

    std::string ValhallaOnlineRoutingService::getBaseURL() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _baseURL;
    }

    void ValhallaOnlineRoutingService::setBaseURL(const std::string& url) {
        std::lock_guard<std::mutex> lk(_mutex);
        _baseURL = url;
    }

    std::string ValhallaOnlineRoutingService::getProfile() const {
        std::lock_guard<std::mutex> lk(_mutex);
        return _profile;
    }

    void ValhallaOnlineRoutingService::setProfile(const std::string& profile) {
        std::lock_guard<std::mutex> lk(_mutex);
        _profile = profile;
    }

    std::string ValhallaOnlineRoutingService::buildURL(const std::string& endpoint) const {
        // _baseURL may or may not have a trailing slash
        std::string url = _baseURL;
        if (!url.empty() && url.back() == '/') url.pop_back();
        return url + "/" + endpoint;
    }

    std::shared_ptr<RoutingResult> ValhallaOnlineRoutingService::calculateRoute(
            const std::shared_ptr<RoutingRequest>& request) const {
        std::string profile;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            profile = _profile;
        }
        std::string requestJSON =
            ValhallaRoutingProxy::SerializeRoutingRequest(profile, request);
        std::string result = callRaw("route", requestJSON);
        return std::make_shared<RoutingResult>(std::move(result));
    }

    std::shared_ptr<RouteMatchingResult> ValhallaOnlineRoutingService::matchRoute(
            const std::shared_ptr<RouteMatchingRequest>& request) const {
        std::string profile;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            profile = _profile;
        }
        std::string requestJSON =
            ValhallaRoutingProxy::SerializeRouteMatchingRequest(profile, request);
        std::string result = callRaw("trace_attributes", requestJSON);
        return std::make_shared<RouteMatchingResult>(std::move(result));
    }

    std::string ValhallaOnlineRoutingService::callRaw(const std::string& endpoint,
                                                       const std::string& jsonBody) const {
        HttpHandler handler;
        std::string url;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            handler = _handler;
            url     = buildURL(endpoint);
        }

        Log::debugf("ValhallaOnlineRoutingService::callRaw: url=%s", url.c_str());

#ifdef ROUTING_WITH_HTTP_CLIENT
        if (!handler) {
            // Use the built-in C++ HTTP client — no external handler needed.
            try {
                HTTPClient httpClient;
                return httpClient.post(url, jsonBody);
            }
            catch (const std::exception& ex) {
                throw GenericException("HTTP request failed for endpoint '" + endpoint + "'",
                                       ex.what());
            }
        }
#endif

        if (!handler) {
            throw GenericException("No HTTP handler set for endpoint '" + endpoint + "'");
        }

        try {
            return handler(url, jsonBody);
        }
        catch (const std::exception& ex) {
            throw GenericException("HTTP request failed for endpoint '" + endpoint + "'",
                                   ex.what());
        }
    }

} // namespace routing
