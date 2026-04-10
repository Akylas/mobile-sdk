#include "ValhallaRoutingProxy.h"
#include "../../../assets/ValhallaDefaultConfig.h"
#include "../../exceptions/Exceptions.h"
#include "../../log/Log.h"

#include "../../network/HTTPClient.h"

#include <sqlite3pp.h>

#include <valhalla/meili/map_matcher.h>
#include <valhalla/meili/map_matcher_factory.h>
#include <valhalla/thor/worker.h>
#include <valhalla/meili/measurement.h>
#include <valhalla/meili/match_result.h>
#include <valhalla/meili/candidate_search.h>
#include <valhalla/midgard/logging.h>
#include <valhalla/midgard/constants.h>
#include <valhalla/midgard/encoded.h>
#include <valhalla/midgard/pointll.h>
#include <valhalla/baldr/pathlocation.h>
#include <valhalla/baldr/directededge.h>
#include <valhalla/baldr/datetime.h>
#include <valhalla/loki/search.h>
#include <valhalla/loki/worker.h>
#include <valhalla/sif/autocost.h>
#include <valhalla/sif/costfactory.h>
#include <valhalla/sif/bicyclecost.h>
#include <valhalla/sif/pedestriancost.h>
#include <valhalla/odin/worker.h>
#include <valhalla/tyr/serializers.h>
#include <valhalla/odin/util.h>
#include <valhalla/odin/directionsbuilder.h>

#include <boost/property_tree/json_parser.hpp>

#include <sstream>
#include <string>
#include <unordered_map>

namespace routing {

// -------------------------------------------------------------------------
// Module-level locale registry (shared across all proxy calls)
// -------------------------------------------------------------------------
static std::unordered_map<std::string, std::string> g_valhallaLocales;

void ValhallaRoutingProxy::AddLocale(const std::string& key, const std::string& json) {
    if (g_valhallaLocales.find(key) == g_valhallaLocales.end()) {
        g_valhallaLocales.emplace(key, json);
    }
}

std::string ValhallaRoutingProxy::GetDefaultConfiguration() {
    return valhalla_default_config;
}

// -------------------------------------------------------------------------
// CallRaw — dispatch any Valhalla endpoint directly
// -------------------------------------------------------------------------
std::string ValhallaRoutingProxy::CallRaw(
        const std::vector<std::shared_ptr<sqlite3pp::database>>& databases,
        const std::string& config,
        const std::string& endpoint,
        const std::string& jsonBody) {

    // Map endpoint string → valhalla action enum
    valhalla::Options::Action action;
    if      (endpoint == "route"             ) action = valhalla::Options::route;
    else if (endpoint == "locate"            ) action = valhalla::Options::locate;
    else if (endpoint == "matrix" ||
             endpoint == "sources_to_targets") action = valhalla::Options::sources_to_targets;
    else if (endpoint == "optimized_route"   ) action = valhalla::Options::optimized_route;
    else if (endpoint == "isochrone"         ) action = valhalla::Options::isochrone;
    else if (endpoint == "trace_route"       ) action = valhalla::Options::trace_route;
    else if (endpoint == "trace_attributes"  ) action = valhalla::Options::trace_attributes;
    else if (endpoint == "height"            ) action = valhalla::Options::height;
    else if (endpoint == "transit_available" ) action = valhalla::Options::transit_available;
    else if (endpoint == "expansion"         ) action = valhalla::Options::expansion;
    else if (endpoint == "centroid"          ) action = valhalla::Options::centroid;
    else if (endpoint == "status"            ) action = valhalla::Options::status;
    else throw GenericException("Unknown Valhalla endpoint", endpoint);

    std::string result;
    try {
        std::stringstream ss(config);
        boost::property_tree::ptree configTree;
        boost::property_tree::json_parser::read_json(ss, configTree);
        auto reader = std::make_shared<valhalla::baldr::GraphReader>(databases);

        valhalla::Api api;
        valhalla::ParseApi(jsonBody, action, api, g_valhallaLocales);

        valhalla::loki::loki_worker_t  lokiWorker(configTree, reader);
        valhalla::thor::thor_worker_t  thorWorker(configTree, reader);
        valhalla::odin::odin_worker_t  odinWorker(configTree);

        switch (action) {
        case valhalla::Options::route:
            lokiWorker.route(api);
            thorWorker.route(api);
            odinWorker.narrate(api);
            result = valhalla::tyr::serializeDirections(api);
            break;

        case valhalla::Options::trace_attributes:
            lokiWorker.trace(api);
            result = thorWorker.trace_attributes(api);
            break;

        case valhalla::Options::trace_route:
            lokiWorker.trace(api);
            thorWorker.trace_route(api);
            odinWorker.narrate(api);
            result = valhalla::tyr::serializeDirections(api);
            break;

        case valhalla::Options::sources_to_targets:
            lokiWorker.matrix(api);
            result = thorWorker.matrix(api);
            break;

        case valhalla::Options::optimized_route:
            thorWorker.optimized_route(api);
            result = valhalla::tyr::serializePbf(api);
            break;

        case valhalla::Options::isochrone:
            lokiWorker.isochrones(api);
            result = thorWorker.isochrones(api);
            break;

        case valhalla::Options::locate:
            result = lokiWorker.locate(api);
            break;

        case valhalla::Options::height:
            result = lokiWorker.height(api);
            break;

        case valhalla::Options::expansion:
            result = thorWorker.expansion(api);
            break;

        case valhalla::Options::centroid:
            thorWorker.centroid(api);
            result = valhalla::tyr::serializePbf(api);
            break;

        case valhalla::Options::status:
            lokiWorker.status(api);
            result = valhalla::tyr::serializeStatus(api);
            break;

        default:
            throw GenericException("Unhandled Valhalla action");
        }

        lokiWorker.cleanup();
        thorWorker.cleanup();
        odinWorker.cleanup();
    }
    catch (const std::exception& ex) {
        throw GenericException("Exception in callRaw(" + endpoint + ")", ex.what());
    }
    return result;
}

std::string ValhallaRoutingProxy::CallRaw(
        HTTPClient& httpClient,
        const std::string& baseURL,
        const std::string& endpoint,
        const std::string& jsonBody) {
    std::string url = baseURL;
    if (!url.empty() && url.back() == '/') url.pop_back();
    url += "/" + endpoint;
    Log::debugf("ValhallaRoutingProxy::CallRaw (HTTP): url=%s", url.c_str());
    return httpClient.post(url, jsonBody);
}

} // namespace routing
