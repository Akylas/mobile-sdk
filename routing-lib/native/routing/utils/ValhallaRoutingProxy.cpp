#include "ValhallaRoutingProxy.h"
#include "RoutingResultBuilder.h"
#include "../../../assets/ValhallaDefaultConfig.h"
#include "../../exceptions/Exceptions.h"
#include "../../log/Log.h"

#include <picojson/picojson.h>

// Valhalla headers — compiled in when HAVE_VALHALLA is defined.
// When building without valhalla (e.g. header-only unit tests), include a
// fallback polyline decoder so ParseRoutingResult still compiles.
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
#  include <valhalla/baldr/json.h>
#  include <valhalla/baldr/pathlocation.h>
#  include <valhalla/baldr/directededge.h>
#  include <valhalla/baldr/datetime.h>
#  include <valhalla/baldr/rapidjson_utils.h>
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
#  include <valhalla/baldr/rapidjson_utils.h>
#else
// -----------------------------------------------------------------------
// Fallback polyline decoder (used when valhalla is not present at all)
// -----------------------------------------------------------------------
namespace valhalla { namespace midgard {

    typedef std::pair<float, float> PointLL;

    template <typename Point>
    class Shape5Decoder {
    public:
        Shape5Decoder(const char* begin, const size_t size)
            : begin(begin), end(begin + size) {}

        Point pop() {
            lat = next(lat);
            lon = next(lon);
            return Point(typename Point::first_type(double(lon) * 1e-6),
                         typename Point::second_type(double(lat) * 1e-6));
        }
        bool empty() const { return begin == end; }

    private:
        const char* begin;
        const char* end;
        int32_t lat = 0, lon = 0;

        int32_t next(const int32_t previous) {
            int byte, shift = 0, result = 0;
            do {
                if (empty()) throw std::runtime_error("Bad encoded polyline");
                byte = int32_t(*begin++) - 63;
                result |= (byte & 0x1f) << shift;
                shift += 5;
            } while (byte >= 0x20);
            return previous + (result & 1 ? ~(result >> 1) : (result >> 1));
        }
    };

    template <class container_t,
              class ShapeDecoder = Shape5Decoder<typename container_t::value_type>>
    container_t decode(const std::string& encoded) {
        ShapeDecoder shape(encoded.c_str(), encoded.size());
        container_t c;
        c.reserve(encoded.size() / 4);
        while (!shape.empty()) c.emplace_back(shape.pop());
        return c;
    }

} } // namespace valhalla::midgard
#endif // HAVE_VALHALLA

// C++17 helper: split a string by delimiter (replaces boost::split)
static std::vector<std::string> splitByDot(const std::string& s) {
    std::vector<std::string> result;
    std::string cur;
    for (char c : s) {
        if (c == '.') { result.push_back(cur); cur.clear(); }
        else cur += c;
    }
    result.push_back(cur);
    return result;
}

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
        ss << config.toPicoJSON().serialize();
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
    return ParseRouteMatchingResult(request->getProjection(), resultString);
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
        ss << config.toPicoJSON().serialize();
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
    return ParseRoutingResult(request->getProjection(), resultString);
#else
    throw GenericException("Valhalla routing support not compiled in");
#endif
}

// -------------------------------------------------------------------------
// Maneuver type translation
// -------------------------------------------------------------------------
bool ValhallaRoutingProxy::TranslateManeuverType(int maneuverType, RoutingAction::RoutingAction& action) {
    enum {
        kNone = 0, kStart = 1, kStartRight = 2, kStartLeft = 3,
        kDestination = 4, kDestinationRight = 5, kDestinationLeft = 6,
        kBecomes = 7, kContinue = 8, kSlightRight = 9, kRight = 10,
        kSharpRight = 11, kUturnRight = 12, kUturnLeft = 13,
        kSharpLeft = 14, kLeft = 15, kSlightLeft = 16,
        kRampStraight = 17, kRampRight = 18, kRampLeft = 19,
        kExitRight = 20, kExitLeft = 21, kStayStraight = 22,
        kStayRight = 23, kStayLeft = 24, kMerge = 25,
        kRoundaboutEnter = 26, kRoundaboutExit = 27,
        kFerryEnter = 28, kFerryExit = 29,
        kMergeRight = 37, kMergeLeft = 38
    };

    static const std::unordered_map<int, RoutingAction::RoutingAction> table = {
        { kNone,           RoutingAction::ROUTING_ACTION_NO_TURN },
        { kContinue,       RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kBecomes,        RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kRampStraight,   RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kStayStraight,   RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kMerge,          RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kMergeLeft,      RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kMergeRight,     RoutingAction::ROUTING_ACTION_GO_STRAIGHT },
        { kFerryEnter,     RoutingAction::ROUTING_ACTION_ENTER_FERRY },
        { kFerryExit,      RoutingAction::ROUTING_ACTION_LEAVE_FERRY },
        { kSlightRight,    RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kRight,          RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kRampRight,      RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kExitRight,      RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kStayRight,      RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kSharpRight,     RoutingAction::ROUTING_ACTION_TURN_RIGHT },
        { kUturnLeft,      RoutingAction::ROUTING_ACTION_UTURN },
        { kUturnRight,     RoutingAction::ROUTING_ACTION_UTURN },
        { kSharpLeft,      RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kLeft,           RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kRampLeft,       RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kExitLeft,       RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kStayLeft,       RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kSlightLeft,     RoutingAction::ROUTING_ACTION_TURN_LEFT },
        { kRoundaboutEnter, RoutingAction::ROUTING_ACTION_ENTER_ROUNDABOUT },
        { kRoundaboutExit,  RoutingAction::ROUTING_ACTION_LEAVE_ROUNDABOUT },
        { kStart,          RoutingAction::ROUTING_ACTION_HEAD_ON },
        { kStartRight,     RoutingAction::ROUTING_ACTION_HEAD_ON },
        { kStartLeft,      RoutingAction::ROUTING_ACTION_HEAD_ON },
        { kDestination,    RoutingAction::ROUTING_ACTION_FINISH },
        { kDestinationRight, RoutingAction::ROUTING_ACTION_FINISH },
        { kDestinationLeft, RoutingAction::ROUTING_ACTION_FINISH },
    };

    auto it = table.find(maneuverType);
    if (it != table.end()) { action = it->second; return true; }
    Log::infof("ValhallaRoutingProxy: ignoring unknown maneuver type %d", maneuverType);
    return false;
}

// -------------------------------------------------------------------------
// Request serialization
// -------------------------------------------------------------------------
std::string ValhallaRoutingProxy::SerializeRouteMatchingRequest(
        const std::string& profile,
        const std::shared_ptr<RouteMatchingRequest>& request) {

    std::shared_ptr<Projection> proj = request->getProjection();
    picojson::array locations;
    const auto& points = request->getPoints();
    for (std::size_t i = 0; i < points.size(); i++) {
        picojson::object loc;
        picojson::value pp = request->getPointParameters(static_cast<int>(i)).toPicoJSON();
        if (pp.is<picojson::object>()) loc = pp.get<picojson::object>();
        MapPos wgs84 = proj->toWgs84(points[i]);
        loc["lon"] = picojson::value(wgs84.getX());
        loc["lat"] = picojson::value(wgs84.getY());
        locations.emplace_back(loc);
    }

    picojson::object json;
    json["shape"]       = picojson::value(locations);
    json["shape_match"] = picojson::value(std::string("map_snap"));
    json["costing"]     = picojson::value(profile);
    json["units"]       = picojson::value(std::string("kilometers"));
    if (request->getAccuracy() > 0) {
        json["gps_accuracy"] = picojson::value(static_cast<double>(request->getAccuracy()));
    }
    picojson::value custom = request->getCustomParameters().toPicoJSON();
    if (custom.is<picojson::object>()) {
        for (const auto& kv : custom.get<picojson::object>())
            json[kv.first] = kv.second;
    }
    return picojson::value(json).serialize();
}

std::string ValhallaRoutingProxy::SerializeRoutingRequest(
        const std::string& profile,
        const std::shared_ptr<RoutingRequest>& request) {

    std::shared_ptr<Projection> proj = request->getProjection();
    picojson::array locations;
    const auto& points = request->getPoints();
    for (std::size_t i = 0; i < points.size(); i++) {
        picojson::object loc;
        picojson::value pp = request->getPointParameters(static_cast<int>(i)).toPicoJSON();
        if (pp.is<picojson::object>()) loc = pp.get<picojson::object>();
        MapPos wgs84 = proj->toWgs84(points[i]);
        loc["lon"] = picojson::value(wgs84.getX());
        loc["lat"] = picojson::value(wgs84.getY());
        locations.emplace_back(loc);
    }

    picojson::object json;
    json["locations"] = picojson::value(locations);
    json["costing"]   = picojson::value(profile);
    json["units"]     = picojson::value(std::string("kilometers"));

    picojson::value custom = request->getCustomParameters().toPicoJSON();
    if (custom.is<picojson::object>()) {
        for (const auto& kv : custom.get<picojson::object>())
            json[kv.first] = kv.second;
    }
    return picojson::value(json).serialize();
}

// -------------------------------------------------------------------------
// Result parsing
// -------------------------------------------------------------------------
std::shared_ptr<RouteMatchingResult> ValhallaRoutingProxy::ParseRouteMatchingResult(
        const std::shared_ptr<Projection>& proj,
        const std::string& resultString) {

    picojson::value result;
    std::string err = picojson::parse(result, resultString);
    if (!err.empty()) throw GenericException("Failed to parse matching result", err);

    std::vector<RouteMatchingPoint> matchingPoints;
    std::vector<RouteMatchingEdge>  matchingEdges;
    try {
        if (result.get("matched_points").is<picojson::array>()) {
            for (const picojson::value& pt : result.get("matched_points").get<picojson::array>()) {
                RouteMatchingPointType::RouteMatchingPointType type =
                    RouteMatchingPointType::ROUTE_MATCHING_POINT_UNMATCHED;
                const std::string& typeStr = pt.get("type").get<std::string>();
                if (typeStr == "matched")      type = RouteMatchingPointType::ROUTE_MATCHING_POINT_MATCHED;
                else if (typeStr == "interpolated") type = RouteMatchingPointType::ROUTE_MATCHING_POINT_INTERPOLATED;

                double lat       = pt.get("lat").get<double>();
                double lon       = pt.get("lon").get<double>();
                int edgeIndex    = static_cast<int>(pt.get("edge_index").get<std::int64_t>());
                matchingPoints.emplace_back(proj->fromLatLong(lat, lon), type, edgeIndex);
            }
        }
        if (result.get("edges").is<picojson::array>()) {
            for (const picojson::value& edge : result.get("edges").get<picojson::array>()) {
                std::map<std::string, Variant> attrs;
                if (edge.is<picojson::object>()) {
                    for (const auto& kv : edge.get<picojson::object>())
                        attrs[kv.first] = Variant::FromPicoJSON(kv.second);
                }
                matchingEdges.emplace_back(attrs);
            }
        }
    }
    catch (const std::exception& ex) {
        throw GenericException("Exception while parsing route matching result", ex.what());
    }
    return std::make_shared<RouteMatchingResult>(proj, std::move(matchingPoints),
                                                  std::move(matchingEdges), resultString);
}

std::shared_ptr<RoutingResult> ValhallaRoutingProxy::ParseRoutingResult(
        const std::shared_ptr<Projection>& proj,
        const std::string& resultString) {

    picojson::value result;
    std::string err = picojson::parse(result, resultString);
    if (!err.empty()) throw GenericException("Failed to parse routing result", err);
    if (!result.get("trip").is<picojson::object>()) {
        throw GenericException("No trip info in the routing result");
    }

    RoutingResultBuilder builder(proj, resultString);
    try {
        std::size_t shapeOffset = 0;
        for (const picojson::value& leg : result.get("trip").get("legs").get<picojson::array>()) {
            std::vector<valhalla::midgard::PointLL> shape =
                valhalla::midgard::decode<std::vector<valhalla::midgard::PointLL>>(
                    leg.get("shape").get<std::string>());

            std::vector<MapPos> points;
            points.reserve(shape.size());
            for (const auto& pt : shape)
                points.push_back(proj->fromLatLong(pt.second, pt.first));
            builder.addPoints(points);

            const picojson::array& maneuvers = leg.get("maneuvers").get<picojson::array>();
            for (std::size_t i = 0; i < maneuvers.size(); i++) {
                const picojson::value& m = maneuvers[i];

                RoutingAction::RoutingAction action = RoutingAction::ROUTING_ACTION_NO_TURN;
                TranslateManeuverType(static_cast<int>(m.get("type").get<std::int64_t>()), action);
                if (action == RoutingAction::ROUTING_ACTION_FINISH && i + 1 < maneuvers.size())
                    action = RoutingAction::ROUTING_ACTION_REACH_VIA_LOCATION;

                std::string streetName;
                if (m.get("street_names").is<picojson::array>()) {
                    for (const picojson::value& n : m.get("street_names").get<picojson::array>())
                        streetName += (streetName.empty() ? "" : "/") + n.get<std::string>();
                }

                int pointIndex = static_cast<int>(shapeOffset +
                    m.get("begin_shape_index").get<std::int64_t>());

                RoutingInstructionBuilder& ib = builder.addInstruction(action, pointIndex);
                ib.setStreetName(streetName);
                ib.setTime(m.get("time").get<double>());
                ib.setDistance(m.get("length").get<double>() * 1000.0);
                ib.setInstruction(m.get("instruction").get<std::string>());
            }
            if (!maneuvers.empty())
                shapeOffset += maneuvers.back().get("begin_shape_index").get<std::int64_t>();
        }
    }
    catch (const std::exception& ex) {
        throw GenericException("Exception while translating routing result", ex.what());
    }
    return builder.buildRoutingResult();
}

} // namespace routing
