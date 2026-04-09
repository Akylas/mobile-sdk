#pragma once

#include "../../core/Variant.h"
#include "../RoutingRequest.h"
#include "../RoutingResult.h"
#include "../RouteMatchingRequest.h"
#include "../RouteMatchingResult.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct sqlite3;

namespace routing {

#ifdef ROUTING_WITH_HTTP_CLIENT
    class HTTPClient;
#endif

    /**
     * Internal helper that serializes requests to JSON, drives Valhalla workers
     * or dispatches them via HTTP, and returns raw JSON response strings.
     *
     * Offline (Valhalla) methods are guarded by #ifdef HAVE_VALHALLA.
     * Online (HTTP) methods are guarded by #ifdef ROUTING_WITH_HTTP_CLIENT.
     */
    class ValhallaRoutingProxy {
    public:
        // ----------------------------------------------------------------
        // Offline routing (calls Valhalla workers directly)
        // ----------------------------------------------------------------

        static std::shared_ptr<RouteMatchingResult> MatchRoute(
            const std::vector<sqlite3*>& databases,
            const std::string& profile,
            const Variant& config,
            const std::shared_ptr<RouteMatchingRequest>& request);

        static std::shared_ptr<RoutingResult> CalculateRoute(
            const std::vector<sqlite3*>& databases,
            const std::string& profile,
            const Variant& config,
            const std::shared_ptr<RoutingRequest>& request);

        /**
         * Call any Valhalla API endpoint directly (offline, Valhalla workers).
         * @param databases   Open MBTiles sqlite3 handles.
         * @param config      Valhalla configuration variant.
         * @param endpoint    Endpoint name, e.g. "route", "trace_attributes", etc.
         * @param jsonBody    Full Valhalla request JSON string.
         * @return            Raw Valhalla JSON response string.
         */
        static std::string CallRaw(
            const std::vector<sqlite3*>& databases,
            const Variant& config,
            const std::string& endpoint,
            const std::string& jsonBody);

#ifdef ROUTING_WITH_HTTP_CLIENT
        // ----------------------------------------------------------------
        // Online routing (dispatches via built-in C++ HTTP client)
        // ----------------------------------------------------------------

        /**
         * Match a route via HTTP POST to a remote Valhalla service.
         * @param httpClient  HTTP client instance.
         * @param baseURL     Base URL of the Valhalla service.
         * @param profile     Costing profile ("auto", "pedestrian", …).
         * @param request     Route matching request.
         */
        static std::shared_ptr<RouteMatchingResult> MatchRoute(
            HTTPClient& httpClient,
            const std::string& baseURL,
            const std::string& profile,
            const std::shared_ptr<RouteMatchingRequest>& request);

        /**
         * Calculate a route via HTTP POST to a remote Valhalla service.
         */
        static std::shared_ptr<RoutingResult> CalculateRoute(
            HTTPClient& httpClient,
            const std::string& baseURL,
            const std::string& profile,
            const std::shared_ptr<RoutingRequest>& request);

        /**
         * Call any Valhalla endpoint via HTTP POST.
         * @param httpClient HTTP client instance.
         * @param baseURL    Base URL of the Valhalla service.
         * @param endpoint   Endpoint name, e.g. "route", "isochrone", etc.
         * @param jsonBody   Full Valhalla request JSON string.
         * @return           Raw JSON response string.
         */
        static std::string CallRaw(
            HTTPClient& httpClient,
            const std::string& baseURL,
            const std::string& endpoint,
            const std::string& jsonBody);
#endif // ROUTING_WITH_HTTP_CLIENT

        // ----------------------------------------------------------------
        // Shared utilities
        // ----------------------------------------------------------------

        static void AddLocale(const std::string& key, const std::string& json);

        static Variant GetDefaultConfiguration();

        /** Build the JSON body for a routing request. */
        static std::string SerializeRoutingRequest(
            const std::string& profile,
            const std::shared_ptr<RoutingRequest>& request);

        /** Build the JSON body for a route-matching request. */
        static std::string SerializeRouteMatchingRequest(
            const std::string& profile,
            const std::shared_ptr<RouteMatchingRequest>& request);

    private:
        ValhallaRoutingProxy() = delete;
    };

} // namespace routing
