#pragma once

#include "../../core/Variant.h"
#include "../RoutingInstruction.h"
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
     * Internal helper that serializes requests to JSON, drives valhalla workers,
     * and parses the JSON response back into routing result objects.
     *
     * All boost dependencies have been replaced with C++17 standard library.
     * The sqlite3 handle is passed directly to valhalla::baldr::GraphReader.
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

        static void AddLocale(const std::string& key, const std::string& json);

        static Variant GetDefaultConfiguration();

    private:
        ValhallaRoutingProxy() = delete;

        static bool TranslateManeuverType(int maneuverType, RoutingAction::RoutingAction& action);

        static std::string SerializeRouteMatchingRequest(
            const std::string& profile,
            const std::shared_ptr<RouteMatchingRequest>& request);

        static std::string SerializeRoutingRequest(
            const std::string& profile,
            const std::shared_ptr<RoutingRequest>& request);

        static std::shared_ptr<RouteMatchingResult> ParseRouteMatchingResult(
            const std::shared_ptr<Projection>& proj,
            const std::string& resultString);

        static std::shared_ptr<RoutingResult> ParseRoutingResult(
            const std::shared_ptr<Projection>& proj,
            const std::string& resultString);
    };

} // namespace routing
