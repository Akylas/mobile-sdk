#include "ValhallaRoutingProxy.h"
#include "../../../assets/ValhallaDefaultConfig.h"
#include "../../exceptions/Exceptions.h"
#include "../../log/Log.h"
#include "../../utils/StringUtils.h"

#ifdef HAVE_VALHALLA
#  include <valhalla/meili/map_matcher.h>
#  include <valhalla/meili/map_matcher_factory.h>
#  include <valhalla/thor/worker.h>
#  include <valhalla/meili/measurement.h>
#  include <valhalla/meili/match_result.h>
#  include <valhalla/meili/candidate_search.h>
#  include <valhalla/midgard/logging.h>
#  include <valhalla/midgard/constants.h>
#  include <valhalla/midgard/encoded.h>
#  include <valhalla/midgard/pointll.h>
#  include <valhalla/baldr/rapidjson_utils.h>
#  include <valhalla/baldr/pathlocation.h>
#  include <valhalla/baldr/directededge.h>
#  include <valhalla/baldr/datetime.h>
#  include <valhalla/loki/search.h>
#  include <valhalla/loki/worker.h>
#  include <valhalla/sif/autocost.h>
#  include <valhalla/sif/costfactory.h>
#  include <valhalla/sif/bicyclecost.h>
#  include <valhalla/sif/pedestriancost.h>
#  include <valhalla/odin/worker.h>
#  include <valhalla/tyr/serializers.h>
#  include <valhalla/odin/util.h>
#  include <valhalla/odin/directionsbuilder.h>
#  include <boost/property_tree/ptree.hpp>

// rapidjson is always available through valhalla
#  include <rapidjson/document.h>
#  include <rapidjson/stringbuffer.h>
#  include <rapidjson/writer.h>
#endif // HAVE_VALHALLA

#include <sstream>
#include <string>
#include <unordered_map>

// -----------------------------------------------------------------------
// Dot-delimited key splitter — see utils/StringUtils.h
// -----------------------------------------------------------------------

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

Variant ValhallaRoutingProxy::GetDefaultConfiguration() {
    return Variant::FromString(valhalla_default_config);
}

// -------------------------------------------------------------------------
// MatchRoute
// -------------------------------------------------------------------------
std::shared_ptr<RouteMatchingResult> ValhallaRoutingProxy::MatchRoute(
        const std::vector<sqlite3*>& databases,
        const std::string& profile,
        const Variant& config,
        const std::shared_ptr<RouteMatchingRequest>& request) {

#ifdef HAVE_VALHALLA
    std::string resultString;
    try {
        std::stringstream ss;
        ss << config.toJSON();
        boost::property_tree::ptree configTree;
        rapidjson::read_json(ss, configTree);
        auto reader = std::make_shared<valhalla::baldr::GraphReader>(databases);

        valhalla::Api api;
        valhalla::ParseApi(SerializeRouteMatchingRequest(profile, request),
                           valhalla::Options::trace_attributes, api, g_valhallaLocales);

        valhalla::loki::loki_worker_t lokiWorker(configTree, reader);
        lokiWorker.trace(api);
        valhalla::thor::thor_worker_t thorWorker(configTree, reader);
        resultString = thorWorker.trace_attributes(api);
        lokiWorker.cleanup();
    }
    catch (const std::exception& ex) {
        throw GenericException("Exception while matching route", ex.what());
    }
    return std::make_shared<RouteMatchingResult>(std::move(resultString));
#else
    throw GenericException("Valhalla routing support not compiled in");
#endif
}

// -------------------------------------------------------------------------
// CalculateRoute
// -------------------------------------------------------------------------
std::shared_ptr<RoutingResult> ValhallaRoutingProxy::CalculateRoute(
        const std::vector<sqlite3*>& databases,
        const std::string& profile,
        const Variant& config,
        const std::shared_ptr<RoutingRequest>& request) {

#ifdef HAVE_VALHALLA
    std::string resultString;
    try {
        std::stringstream ss;
        ss << config.toJSON();
        boost::property_tree::ptree configTree;
        rapidjson::read_json(ss, configTree);
        auto reader = std::make_shared<valhalla::baldr::GraphReader>(databases);

        valhalla::Api api;
        valhalla::ParseApi(SerializeRoutingRequest(profile, request),
                           valhalla::Options::route, api, g_valhallaLocales);

        valhalla::loki::loki_worker_t lokiWorker(configTree, reader);
        lokiWorker.route(api);
        valhalla::thor::thor_worker_t thorWorker(configTree, reader);
        thorWorker.route(api);
        valhalla::odin::odin_worker_t odinWorker(configTree);
        odinWorker.narrate(api);
        resultString = valhalla::tyr::serializeDirections(api);
        lokiWorker.cleanup();
        thorWorker.cleanup();
        odinWorker.cleanup();
    }
    catch (const std::exception& ex) {
        throw GenericException("Exception while calculating route", ex.what());
    }
    return std::make_shared<RoutingResult>(std::move(resultString));
#else
    throw GenericException("Valhalla routing support not compiled in");
#endif
}

// -------------------------------------------------------------------------
// CallRaw — dispatch any Valhalla endpoint directly
// -------------------------------------------------------------------------
std::string ValhallaRoutingProxy::CallRaw(
        const std::vector<sqlite3*>& databases,
        const Variant& config,
        const std::string& endpoint,
        const std::string& jsonBody) {

#ifdef HAVE_VALHALLA
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
        std::stringstream ss;
        ss << config.toJSON();
        boost::property_tree::ptree configTree;
        rapidjson::read_json(ss, configTree);
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
            lokiWorker.route(api);
            result = thorWorker.optimized_route(api);
            break;

        case valhalla::Options::isochrone:
            lokiWorker.isochrone(api);
            result = thorWorker.isochrone(api);
            break;

        case valhalla::Options::locate:
            lokiWorker.locate(api);
            result = valhalla::tyr::serializeLocate(api);
            break;

        case valhalla::Options::height:
            lokiWorker.height(api);
            result = valhalla::tyr::serializeHeight(api);
            break;

        case valhalla::Options::expansion:
            lokiWorker.expansion(api);
            result = thorWorker.expansion(api);
            break;

        case valhalla::Options::centroid:
            lokiWorker.centroid(api);
            result = thorWorker.centroid(api);
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
#else
    throw GenericException("Valhalla routing support not compiled in");
#endif
}

// -------------------------------------------------------------------------
// Request serialization — uses rapidjson (from valhalla) when available,
// otherwise falls back to Variant's own toJSON() helpers.
// -------------------------------------------------------------------------

#ifdef HAVE_VALHALLA

// Build a rapidjson object for a single location from its Variant parameter bag.
static void addLocationToArray(
        rapidjson::Value& arr,
        rapidjson::Document::AllocatorType& alloc,
        double lon, double lat,
        const Variant& params) {

    rapidjson::Value loc(rapidjson::kObjectType);

    // Copy any extra parameters from the Variant object
    if (params.getType() == VariantType::VARIANT_TYPE_OBJECT) {
        for (const auto& key : params.getObjectKeys()) {
            const Variant& v = params.getObjectElement(key);
            rapidjson::Value k(key.c_str(), alloc);
            switch (v.getType()) {
            case VariantType::VARIANT_TYPE_STRING: {
                rapidjson::Value s(v.getString().c_str(), alloc);
                loc.AddMember(k, s, alloc);
                break;
            }
            case VariantType::VARIANT_TYPE_BOOL:
                loc.AddMember(k, v.getBool(), alloc);
                break;
            case VariantType::VARIANT_TYPE_INTEGER:
                loc.AddMember(k, v.getLong(), alloc);
                break;
            case VariantType::VARIANT_TYPE_DOUBLE:
                loc.AddMember(k, v.getDouble(), alloc);
                break;
            default:
                break;
            }
        }
    }

    loc.AddMember("lon", lon, alloc);
    loc.AddMember("lat", lat, alloc);
    arr.PushBack(loc, alloc);
}

// Merge custom parameters (Variant object) into a rapidjson object.
static void mergeCustomParams(
        rapidjson::Value& target,
        rapidjson::Document::AllocatorType& alloc,
        const Variant& params) {
    if (params.getType() != VariantType::VARIANT_TYPE_OBJECT) return;
    // Re-parse the JSON representation of params into a rapidjson document,
    // then copy top-level members into target.
    std::string json = params.toJSON();
    rapidjson::Document doc;
    doc.Parse(json.c_str(), json.size());
    if (doc.HasParseError() || !doc.IsObject()) return;
    for (auto& m : doc.GetObject()) {
        rapidjson::Value k(m.name, alloc);
        rapidjson::Value v(m.value, alloc);
        target.AddMember(k, v, alloc);
    }
}

static std::string documentToString(const rapidjson::Document& doc) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> writer(buf);
    doc.Accept(writer);
    return buf.GetString();
}

#endif // HAVE_VALHALLA

std::string ValhallaRoutingProxy::SerializeRoutingRequest(
        const std::string& profile,
        const std::shared_ptr<RoutingRequest>& request) {

    // Points are always WGS-84: MapPos(lon, lat)
    const auto& points = request->getPoints();

#ifdef HAVE_VALHALLA
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    rapidjson::Value locations(rapidjson::kArrayType);
    for (std::size_t i = 0; i < points.size(); i++) {
        addLocationToArray(locations, alloc, points[i].getX(), points[i].getY(),
                           request->getPointParameters(static_cast<int>(i)));
    }
    doc.AddMember("locations", locations, alloc);

    rapidjson::Value costingVal(profile.c_str(), alloc);
    doc.AddMember("costing", costingVal, alloc);
    doc.AddMember("units", rapidjson::StringRef("kilometers"), alloc);

    mergeCustomParams(doc, alloc, request->getCustomParameters());
    return documentToString(doc);
#else
    // Fallback: build JSON manually
    std::string json = "{\"locations\":[";
    for (std::size_t i = 0; i < points.size(); i++) {
        if (i > 0) json += ',';
        json += "{\"lon\":" + std::to_string(points[i].getX()) +
                ",\"lat\":" + std::to_string(points[i].getY()) + "}";
    }
    json += "],\"costing\":\"" + profile + "\",\"units\":\"kilometers\"}";
    return json;
#endif
}

std::string ValhallaRoutingProxy::SerializeRouteMatchingRequest(
        const std::string& profile,
        const std::shared_ptr<RouteMatchingRequest>& request) {

    // Points are always WGS-84: MapPos(lon, lat)
    const auto& points = request->getPoints();

#ifdef HAVE_VALHALLA
    rapidjson::Document doc;
    doc.SetObject();
    auto& alloc = doc.GetAllocator();

    rapidjson::Value shape(rapidjson::kArrayType);
    for (std::size_t i = 0; i < points.size(); i++) {
        addLocationToArray(shape, alloc, points[i].getX(), points[i].getY(),
                           request->getPointParameters(static_cast<int>(i)));
    }
    doc.AddMember("shape", shape, alloc);
    doc.AddMember("shape_match", rapidjson::StringRef("map_snap"), alloc);
    rapidjson::Value costingVal(profile.c_str(), alloc);
    doc.AddMember("costing", costingVal, alloc);
    doc.AddMember("units", rapidjson::StringRef("kilometers"), alloc);

    if (request->getAccuracy() > 0) {
        doc.AddMember("gps_accuracy",
                      static_cast<double>(request->getAccuracy()), alloc);
    }

    mergeCustomParams(doc, alloc, request->getCustomParameters());
    return documentToString(doc);
#else
    // Fallback: build JSON manually
    std::string json = "{\"shape\":[";
    for (std::size_t i = 0; i < points.size(); i++) {
        if (i > 0) json += ',';
        json += "{\"lon\":" + std::to_string(points[i].getX()) +
                ",\"lat\":" + std::to_string(points[i].getY()) + "}";
    }
    json += "],\"shape_match\":\"map_snap\",\"costing\":\"" + profile + "\",\"units\":\"kilometers\"}";
    return json;
#endif
}

} // namespace routing
