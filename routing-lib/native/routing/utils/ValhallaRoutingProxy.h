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

    /**
     * Internal helper that serializes requests to JSON, drives Valhalla workers,
     * and returns raw JSON response strings.
     *
     * All methods that touch Valhalla workers are guarded by #ifdef HAVE_VALHALLA.
     * The sqlite3 handles are passed directly to valhalla::baldr::GraphReader.
     */
    class ValhallaRoutingProxy {
    public:
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
         * Call any Valhalla API endpoint directly.
         * @param databases   Open MBTiles sqlite3 handles.
         * @param config      Valhalla configuration variant.
         * @param endpoint    Endpoint name, e.g. "route", "trace_attributes",
         *                    "trace_route", "isochrone", "matrix", "locate",
         *                    "height", "expansion", "centroid", "status".
         * @param jsonBody    Full Valhalla request JSON string.
         * @return            Raw Valhalla JSON response string.
         */
        static std::string CallRaw(
            const std::vector<sqlite3*>& databases,
            const Variant& config,
            const std::string& endpoint,
            const std::string& jsonBody);

        static void AddLocale(const std::string& key, const std::string& json);

        static Variant GetDefaultConfiguration();

        /** Build the JSON body for a routing request (also used by the online service). */
        static std::string SerializeRoutingRequest(
            const std::string& profile,
            const std::shared_ptr<RoutingRequest>& request);

        /** Build the JSON body for a route-matching request (also used by the online service). */
        static std::string SerializeRouteMatchingRequest(
            const std::string& profile,
            const std::shared_ptr<RouteMatchingRequest>& request);

    private:
        ValhallaRoutingProxy() = delete;
    };

} // namespace routing
