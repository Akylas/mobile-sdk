#pragma once

#include "../../core/Variant.h"

#include <string>
#include <vector>

struct sqlite3;

namespace routing {

    class HTTPClient;

    /**
     * Internal helper that drives Valhalla workers (offline) or dispatches
     * requests via HTTP (online), returning raw JSON response strings.
     */
    class ValhallaRoutingProxy {
    public:
        // ----------------------------------------------------------------
        // Offline routing — drives Valhalla workers directly
        // ----------------------------------------------------------------

        /**
         * Call any Valhalla API endpoint using offline Valhalla workers.
         * @param databases   Open MBTiles sqlite3 handles.
         * @param config      Valhalla configuration variant.
         * @param endpoint    Endpoint name, e.g. "route", "trace_attributes".
         * @param jsonBody    Full Valhalla request JSON string (including "costing").
         * @return            Raw Valhalla JSON response string.
         */
        static std::string CallRaw(
            const std::vector<sqlite3*>& databases,
            const Variant& config,
            const std::string& endpoint,
            const std::string& jsonBody);

        // ----------------------------------------------------------------
        // Online routing — dispatches via built-in C++ HTTP client
        // ----------------------------------------------------------------

        /**
         * Call any Valhalla endpoint via HTTP POST.
         * @param httpClient HTTP client instance.
         * @param baseURL    Base URL of the Valhalla service.
         * @param endpoint   Endpoint name, e.g. "route", "isochrone".
         * @param jsonBody   Full Valhalla request JSON string (including "costing").
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

    private:
        ValhallaRoutingProxy() = delete;
    };

} // namespace routing
