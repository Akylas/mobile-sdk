#pragma once

#include "../../core/Variant.h"

#include <string>
#include <vector>

struct sqlite3;

namespace routing {

    class HTTPClient;

    /**
     * Internal helper that serializes requests to JSON, drives Valhalla workers
     * or dispatches them via HTTP, and returns raw JSON response strings.
     */
    class ValhallaRoutingProxy {
    public:
        // ----------------------------------------------------------------
        // Offline routing (calls Valhalla workers directly)
        // ----------------------------------------------------------------

        // static std::string MatchRoute(
        //     const std::vector<sqlite3*>& databases,
        //     const std::string& profile,
        //     const Variant& config,
        //     const std::shared_ptr<RouteMatchingRequest>& request);

        // static std::string CalculateRoute(
        //     const std::vector<sqlite3*>& databases,
        //     const std::string& profile,
        //     const Variant& config,
        //     const std::shared_ptr<RoutingRequest>& request);

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
        // static std::string MatchRoute(
        //     HTTPClient& httpClient,
        //     const std::string& baseURL,
        //     const std::string& profile,
        //     const std::shared_ptr<RouteMatchingRequest>& request);

        /**
         * Calculate a route via HTTP POST to a remote Valhalla service.
         */
        // static std::string CalculateRoute(
        //     HTTPClient& httpClient,
        //     const std::string& baseURL,
        //     const std::string& profile,
        //     const std::shared_ptr<RoutingRequest>& request);

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

        // ----------------------------------------------------------------
        // Shared utilities
        // ----------------------------------------------------------------

        static void AddLocale(const std::string& key, const std::string& json);

        static Variant GetDefaultConfiguration();

        /** Build the JSON body for a routing request. */
        // static std::string SerializeRoutingRequest(
        //     const std::string& profile,
        //     const std::shared_ptr<RoutingRequest>& request);

        // /** Build the JSON body for a route-matching request. */
        // static std::string SerializeRouteMatchingRequest(
        //     const std::string& profile,
        //     const std::shared_ptr<RouteMatchingRequest>& request);

    private:
        ValhallaRoutingProxy() = delete;
    };

} // namespace routing
