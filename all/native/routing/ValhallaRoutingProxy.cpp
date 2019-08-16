#ifdef _CARTO_ROUTING_SUPPORT

#include "ValhallaRoutingProxy.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "projections/Projection.h"
#include "projections/EPSG3857.h"
#include "routing/RouteMatchingRequest.h"
#include "routing/RouteMatchingResult.h"
#include "network/HTTPClient.h"
#include "utils/NetworkUtils.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <ctime>
#include <vector>
#include <functional>
#include <string>
#include <stdexcept>
#include <vector>
#include <cstdint>
#include <sstream>
#include <strstream>
#include <utility>

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/local_time/local_time.hpp>

#include <picojson/picojson.h>

#ifdef _CARTO_VALHALLA_ROUTING_SUPPORT
#include <valhalla/config.h>
#include <valhalla/meili/measurement.h>
#include <valhalla/meili/match_result.h>
#include <valhalla/meili/map_matcher_factory.h>
#include <valhalla/meili/candidate_search.h>
#include <valhalla/meili/map_matcher.h>
#include <valhalla/meili/universal_cost.h>
#include <valhalla/meili/map_matcher_factory.h>
#include <valhalla/midgard/logging.h>
#include <valhalla/midgard/constants.h>
#include <valhalla/midgard/encoded.h>
#include <valhalla/midgard/pointll.h>
#include <valhalla/baldr/json.h>
#include <valhalla/baldr/geojson.h>
#include <valhalla/baldr/graphtilembtstorage.h>
#include <valhalla/baldr/pathlocation.h>
#include <valhalla/baldr/directededge.h>
#include <valhalla/baldr/datetime.h>
#include <valhalla/loki/search.h>
#include <valhalla/sif/autocost.h>
#include <valhalla/sif/costfactory.h>
#include <valhalla/sif/bicyclecost.h>
#include <valhalla/sif/pedestriancost.h>
#include <valhalla/meili/measurement.h>
#include <valhalla/meili/map_matching.h>
#include <valhalla/meili/map_matcher.h>
#include <valhalla/meili/map_matcher_factory.h>
#include <valhalla/meili/match_result.h>
#include <valhalla/thor/pathalgorithm.h>
#include <valhalla/thor/bidirectional_astar.h>
#include <valhalla/thor/astar.h>
#include <valhalla/thor/multimodal.h>
#include <valhalla/thor/trippathbuilder.h>
#include <valhalla/thor/isochrone.h>
#include <valhalla/odin/util.h>
#include <valhalla/odin/directionsbuilder.h>
#include <valhalla/proto/trippath.pb.h>
#include <valhalla/proto/tripdirections.pb.h>

namespace valhalla { namespace sif {

    cost_ptr_t CreateWheelchairCost(const boost::property_tree::ptree& config) {
        boost::property_tree::ptree wheelchairConfig(config);
        wheelchairConfig.put("type", "wheelchair");
        return CreatePedestrianCost(wheelchairConfig);
    }

} }

namespace valhalla { namespace meili {

    inline float local_tile_size(const valhalla::baldr::GraphReader& graphreader) {
        const auto& tile_hierarchy = graphreader.GetTileHierarchy();
        const auto& tiles = tile_hierarchy.levels().rbegin()->second.tiles;
        return tiles.TileSize();
    }

    class map_matcher_factory_t {
    public:
        map_matcher_factory_t(const std::vector<std::shared_ptr<sqlite3pp::database>>& databases, const std::string& costing);
        ~map_matcher_factory_t();

        baldr::GraphReader& graphreader()
        { return graphreader_; }

        CandidateQuery& candidatequery()
        { return candidatequery_; }

        MapMatcher* Create(const std::string& name)
        { return Create(name, boost::property_tree::ptree()); }

        MapMatcher* Create(const std::string& name,
                           const boost::property_tree::ptree& preferences);

        MapMatcher* Create(const boost::property_tree::ptree&);

        boost::property_tree::ptree
        MergeConfig(const std::string&, const boost::property_tree::ptree&);

        boost::property_tree::ptree&
        MergeConfig(const std::string&, boost::property_tree::ptree&);

        static constexpr size_t kModeCostingCount = 8;

    private:
        typedef sif::cost_ptr_t (*factory_function_t)(const boost::property_tree::ptree&);

        static boost::property_tree::ptree make_reader_config();
        static boost::property_tree::ptree make_meili_config(const std::string& costing);

        boost::property_tree::ptree config_;

        baldr::GraphReader graphreader_;

        valhalla::sif::cost_ptr_t mode_costing_[kModeCostingCount];

        sif::CostFactory<sif::DynamicCost> cost_factory_;

        CandidateGridQuery candidatequery_;

        float max_grid_cache_size_;

        sif::cost_ptr_t get_costing(const boost::property_tree::ptree& request,
                                                const std::string& costing);
    };

    map_matcher_factory_t::map_matcher_factory_t(const std::vector<std::shared_ptr<sqlite3pp::database>>& databases, const std::string& costing)
        : config_(make_meili_config(costing)),
          graphreader_(std::make_shared<baldr::GraphTileMBTStorage>(databases), make_reader_config()),
          candidatequery_(graphreader_,
                          local_tile_size(graphreader_)/500,
                          local_tile_size(graphreader_)/500),
          max_grid_cache_size_(100240)
    { 
        cost_factory_.Register("auto", sif::CreateAutoCost);
        cost_factory_.Register("bicycle", sif::CreateBicycleCost);
        cost_factory_.Register("pedestrian", sif::CreatePedestrianCost);
        cost_factory_.Register("wheelchair", sif::CreateWheelchairCost);
        cost_factory_.Register("multimodal", CreateUniversalCost);
    }

    map_matcher_factory_t::~map_matcher_factory_t() {
    }

    boost::property_tree::ptree map_matcher_factory_t::make_reader_config() {
        boost::property_tree::ptree config;
        config.put("max_cache_size", 16 * 1024 * 1024);
        config.put("tile_dir", "");
        return config;
    }

    boost::property_tree::ptree map_matcher_factory_t::make_meili_config(const std::string& costing) {
        boost::property_tree::ptree defaultProfile;
        defaultProfile.put("sigma_z", 4.07f);
        defaultProfile.put("gps_accuracy", 5.0f);
        defaultProfile.put("beta", 3);
        defaultProfile.put("max_route_distance_factor", 3);
        defaultProfile.put("breakage_distance", 2000);
        defaultProfile.put("interpolation_distance", 10);
        defaultProfile.put("search_radius", 50);
        defaultProfile.put("max_search_radius", 100);
        defaultProfile.put("geometry", false);
        defaultProfile.put("route", true);
        defaultProfile.put("turn_penalty_factor", 0);

        boost::property_tree::ptree autoProfile;
        autoProfile.put("turn_penalty_factor", 200);
        autoProfile.put("search_radius", 50);

        boost::property_tree::ptree pedestrianProfile;
        pedestrianProfile.put("turn_penalty_factor", 100);
        pedestrianProfile.put("search_radius", 25);

        boost::property_tree::ptree wheelchairProfile(pedestrianProfile);
        wheelchairProfile.put("type", "wheelchair");

        boost::property_tree::ptree bicycleProfile;
        bicycleProfile.put("turn_penalty_factor", 140);

        boost::property_tree::ptree multimodalProfile;
        multimodalProfile.put("turn_penalty_factor", 70);

        boost::property_tree::ptree config;
        config.put("mode", costing);
        config.put("verbose", false);
        config.add_child("default", defaultProfile);
        config.add_child("auto", autoProfile);
        config.add_child("pedestrian", pedestrianProfile);
        config.add_child("wheelchair", wheelchairProfile);
        config.add_child("bicycle", bicycleProfile);
        config.add_child("multimodal", multimodalProfile);

        return config;
    }

    MapMatcher* map_matcher_factory_t::Create(const boost::property_tree::ptree& preferences) {
        const auto& name = preferences.get<std::string>("mode", config_.get<std::string>("mode"));
        return Create(name, preferences);
    }

    MapMatcher* map_matcher_factory_t::Create(const std::string& costing, const boost::property_tree::ptree& preferences) {
        const auto& config = MergeConfig(costing, preferences);

        valhalla::sif::cost_ptr_t cost = get_costing(config, costing);
        valhalla::sif::TravelMode mode = cost->travel_mode();

        mode_costing_[static_cast<uint32_t>(mode)] = cost;

        // TODO investigate exception safety
        return new MapMatcher(config, graphreader_, candidatequery_, mode_costing_, mode);
    }

    boost::property_tree::ptree map_matcher_factory_t::MergeConfig(const std::string& name,
                                   const boost::property_tree::ptree& preferences)
    {
        // Copy the default child config
        auto config = config_.get_child("default");

        // The mode-specific config overwrites defaults
        const auto mode_config = config_.get_child_optional(name);
        if (mode_config) {
            for (const auto& child : *mode_config) {
               config.put_child(child.first, child.second);
            }
        }

        // Preferences overwrites defaults
        for (const auto& child : preferences) {
            config.put_child(child.first, child.second);
        }

        // Give it back
        return config;
    }

    boost::property_tree::ptree& map_matcher_factory_t::MergeConfig(const std::string& name,
                                   boost::property_tree::ptree& preferences)
    {
        const auto mode_config = config_.get_child_optional(name);
        if (mode_config) {
            for (const auto& child : *mode_config) {
                auto pchild = preferences.get_child_optional(child.first);
                if (!pchild) {
                  preferences.put_child(child.first, child.second);
                }
            }
        }

        for (const auto& child : config_.get_child("default")) {
            auto pchild = preferences.get_child_optional(child.first);
            if (!pchild) {
                preferences.put_child(child.first, child.second);
            }
        }

        return preferences;
    }

    sif::cost_ptr_t map_matcher_factory_t::get_costing(const boost::property_tree::ptree& request,
                                              const std::string& costing)
    {
        std::string method_options = "costing_options." + costing;
        auto costing_options = request.get_child(method_options, {});
        return cost_factory_.Create(costing, costing_options);
    }

} }

namespace valhalla { namespace thor {

    class thor_worker_t {
    public:
        thor_worker_t(const std::vector<std::shared_ptr<sqlite3pp::database>>& databases, const std::string& costing);
        virtual ~thor_worker_t();

        std::list<valhalla::odin::TripPath> path_depart_at(const std::vector<valhalla::midgard::PointLL>& points, const boost::optional<int>& date_time_type);

    protected:
        static boost::property_tree::ptree make_reader_config();

        void update_origin(baldr::PathLocation& origin, bool prior_is_node, const baldr::GraphId& through_edge);
        void get_path(PathAlgorithm* path_algorithm, baldr::PathLocation& origin, baldr::PathLocation& destination, std::vector<thor::PathInfo>& path_edges);
        thor::PathAlgorithm* get_path_algorithm(const std::string& routetype, const baldr::PathLocation& origin, const baldr::PathLocation& destination);

        valhalla::baldr::GraphReader reader;
        std::string costing;
        valhalla::sif::TravelMode mode;
        boost::optional<std::string> jsonp;
        std::vector<baldr::Location> locations;
        std::vector<baldr::Location> sources;
        std::vector<baldr::Location> targets;
        std::vector<baldr::PathLocation> correlated;
        std::vector<baldr::PathLocation> correlated_s;
        std::vector<baldr::PathLocation> correlated_t;
        sif::CostFactory<sif::DynamicCost> factory;
        sif::CostFactory<sif::DynamicCost>::cost_ptr_t cost;
        valhalla::sif::cost_ptr_t mode_costing[16];

        // Path algorithms
        AStarPathAlgorithm astar;
        BidirectionalAStar bidir_astar;
        MultiModalPathAlgorithm multi_modal_astar;
        Isochrone isochrone_gen;
        float long_request;
        boost::optional<int> date_time_type;
    };

    thor_worker_t::thor_worker_t(const std::vector<std::shared_ptr<sqlite3pp::database> >& databases, const std::string& costing) : reader(std::make_shared<baldr::GraphTileMBTStorage>(databases), make_reader_config()), costing(costing) {
        factory.Register("auto", sif::CreateAutoCost);
        factory.Register("auto_shorter", sif::CreateAutoShorterCost);
        factory.Register("bus", sif::CreateBusCost);
        factory.Register("bicycle", sif::CreateBicycleCost);
        factory.Register("pedestrian", sif::CreatePedestrianCost);
        factory.Register("wheelchair", sif::CreateWheelchairCost);
        factory.Register("truck", sif::CreateTruckCost);
        factory.Register("transit", sif::CreateTransitCost);

        boost::property_tree::ptree config_costing;
        if (costing == "multimodal" || costing == "transit") {
            mode_costing[0] = factory.Create("auto", config_costing);
            mode_costing[1] = factory.Create("pedestrian", config_costing);
            mode_costing[2] = factory.Create("bicycle", config_costing);
            mode_costing[3] = factory.Create("transit", config_costing);
            cost = factory.Create("pedestrian", config_costing);
            mode = valhalla::sif::TravelMode::kPedestrian;
        } else {
            cost = factory.Create(costing, config_costing);
            mode = cost->travel_mode();
            mode_costing[static_cast<uint32_t>(mode)] = cost;
        }
    }
    
    thor_worker_t::~thor_worker_t() {
    }

    boost::property_tree::ptree thor_worker_t::make_reader_config() {
        boost::property_tree::ptree config;
        config.put("max_cache_size", 16 * 1024 * 1024);
        config.put("tile_dir", "");
        return config;
    }

    thor::PathAlgorithm* thor_worker_t::get_path_algorithm(const std::string& routetype,
        const baldr::PathLocation& origin, const baldr::PathLocation& destination) {
        if (routetype == "multimodal") {
            return &multi_modal_astar;
        }
        else if (routetype == "bus") {
            // TODO - can we use bidirectional A*?
            return &astar;
        }
        else if (routetype == "pedestrian" || routetype == "wheelchair") {
            return &bidir_astar;
        }
        else {
            // Use A* if any origin and destination edges are the same - otherwise
            // use bidirectional A*. Bidirectional A* does not handle trivial cases
            // with oneways.
            for (auto& edge1 : origin.edges) {
                for (auto& edge2 : destination.edges) {
                    if (edge1.id == edge2.id) {
                        return &astar;
                    }
                }
            }
            return &bidir_astar;
        }
    }

    void thor_worker_t::get_path(PathAlgorithm* path_algorithm,
        baldr::PathLocation& origin, baldr::PathLocation& destination,
        std::vector<thor::PathInfo>& path_edges) {
        midgard::logging::Log("#_passes::1", " [ANALYTICS] ");
        // Find the path.
        path_edges = path_algorithm->GetBestPath(origin, destination, reader,
            mode_costing, mode);
        // If path is not found try again with relaxed limits (if allowed)
        if (path_edges.size() == 0) {
            valhalla::sif::cost_ptr_t cost = mode_costing[static_cast<uint32_t>(mode)];
            if (cost->AllowMultiPass()) {
                // 2nd pass. Less aggressive hierarchy transitioning
                path_algorithm->Clear();
                bool using_astar = (path_algorithm == &astar);
                float relax_factor = using_astar ? 16.0f : 8.0f;
                float expansion_within_factor = using_astar ? 4.0f : 2.0f;
                cost->RelaxHierarchyLimits(relax_factor, expansion_within_factor);
                midgard::logging::Log("#_passes::2", " [ANALYTICS] ");
                path_edges = path_algorithm->GetBestPath(origin, destination,
                    reader, mode_costing, mode);
            }
        }
    }

    void thor_worker_t::update_origin(baldr::PathLocation& origin, bool prior_is_node,
        const baldr::GraphId& through_edge) {
        if (prior_is_node) {
            // TODO - remove the opposing through edge from list of edges unless
            // all outbound edges are entering noth_thru regions.
            // For now allow all edges
        }
        else {
            // Check if the edge is entering a not_thru region - if so do not
            // exclude the opposing edge
            const baldr::DirectedEdge* de = reader.GetGraphTile(through_edge)->directededge(through_edge);
            if (de->not_thru()) {
                return;
            }

            // Check if the through edge is dist = 1 (through point is at a node)
            bool ends_at_node = false;
            for (const auto& e : origin.edges) {
                if (e.id == through_edge) {
                    if (e.end_node()) {
                        ends_at_node = true;
                        break;
                    }
                }
            }

            // Special case if location is at the end of a through edge
            if (ends_at_node) {
                // Erase the through edge and its opposing edge (if in the list)
                // from the origin edges
                auto opp_edge = reader.GetOpposingEdgeId(through_edge);
                std::remove_if(origin.edges.begin(), origin.edges.end(),
                    [&through_edge, &opp_edge](const baldr::PathLocation::PathEdge& edge) {
                    return edge.id == through_edge || edge.id == opp_edge; });
            }
            else {
                // Set the origin edge to the through_edge.
                for (auto e : origin.edges) {
                    if (e.id == through_edge) {
                        origin.edges.clear();
                        origin.edges.push_back(e);
                        break;
                    }
                }
            }
        }
    }

    std::list<valhalla::odin::TripPath> thor_worker_t::path_depart_at(const std::vector<valhalla::midgard::PointLL>& points, const boost::optional<int>& date_time_type) {
        // Build correlated path locations
        std::vector<baldr::PathLocation> correlated;
        correlated.reserve(points.size());
        for (const auto& point : points) {
            auto projections = valhalla::loki::Search(std::vector<valhalla::baldr::Location> { point }, reader, cost->GetEdgeFilter(), cost->GetNodeFilter());
            if (projections.find(point) == projections.end()) {
                throw std::runtime_error("Failed to locate route endpoint in routing graph");
            }
            correlated.push_back(projections.at(point));
        }

        if (!correlated.empty() && date_time_type == 0) {
#ifndef _WIN32
            tzset();
            time_t now = time(0);
            struct tm* ltnow = localtime(&now);
            boost::local_time::time_zone_ptr timezone(new boost::local_time::posix_time_zone(tzname[ltnow->tm_isdst]));
            correlated.front().date_time_ = valhalla::baldr::DateTime::iso_date_time(timezone);
#else
            throw std::runtime_error("Multimodal routing not implemented on Windows platforms");
#endif
        }
        
        bool prior_is_node = false;
        std::list<baldr::PathLocation> through_loc;
        baldr::GraphId through_edge;
        std::vector<thor::PathInfo> path_edges;
        std::string origin_date_time, dest_date_time;
        
        std::list<valhalla::odin::TripPath> trippaths;
        baldr::PathLocation& last_break_origin = correlated[0];
        for (auto path_location = ++correlated.cbegin(); path_location != correlated.cend(); ++path_location) {
            auto origin = *std::prev(path_location);
            auto destination = *path_location;

            if (date_time_type && (*date_time_type == 0 || *date_time_type == 1) &&
                !dest_date_time.empty() && origin.stoptype_ == baldr::Location::StopType::BREAK)
                origin.date_time_ = dest_date_time;

            // Through edge is valid if last destination was "through"
            if (through_edge.Is_Valid()) {
                update_origin(origin, prior_is_node, through_edge);
            }
            else {
                last_break_origin = origin;
            }

            // Get the algorithm type for this location pair
            thor::PathAlgorithm* path_algorithm = get_path_algorithm(costing,
                origin, destination);

            // Get best path
            if (path_edges.size() == 0) {
                get_path(path_algorithm, origin, destination, path_edges);
                if (path_edges.size() == 0) {
                    throw std::runtime_error("No path could be found for input");
                }

                if (date_time_type && *date_time_type == 0 && origin_date_time.empty() &&
                    origin.stoptype_ == baldr::Location::StopType::BREAK)
                    last_break_origin.date_time_ = origin.date_time_;
            }
            else {
                // Get the path in a temporary vector
                std::vector<thor::PathInfo> temp_path;
                get_path(path_algorithm, origin, destination, temp_path);
                if (temp_path.size() == 0) {
                    throw std::runtime_error("No path could be found for input");
                }

                if (date_time_type && *date_time_type == 0 && origin_date_time.empty() &&
                    origin.stoptype_ == baldr::Location::StopType::BREAK)
                    last_break_origin.date_time_ = origin.date_time_;

                // Append the temp_path edges to path_edges, adding the elapsed
                // time from the end of the current path. If continuing along the
                // same edge, remove the prior so we do not get a duplicate edge.
                uint32_t t = path_edges.back().elapsed_time;
                if (temp_path.front().edgeid == path_edges.back().edgeid) {
                    path_edges.pop_back();
                }
                for (auto edge : temp_path) {
                    edge.elapsed_time += t;
                    path_edges.emplace_back(edge);
                }
            }

            // Build trip path for this leg and add to the result if this
            // location is a BREAK or if this is the last location
            if (destination.stoptype_ == baldr::Location::StopType::BREAK ||
                path_location == --correlated.cend()) {

                // Create controller for default route attributes
                TripPathController controller;

                // Form output information based on path edges
                auto trip_path = thor::TripPathBuilder::Build(controller, reader, mode_costing, 
                    path_edges,
                    last_break_origin, destination, through_loc, nullptr);

                if (date_time_type) {
                    origin_date_time = *last_break_origin.date_time_;
                    dest_date_time = *destination.date_time_;
                }

                // The protobuf path
                trippaths.emplace_back(std::move(trip_path));

                // Clear path edges and set through edge to invalid
                path_edges.clear();
                through_edge = baldr::GraphId();
            }
            else {
                // This is a through location. Save last edge as the through_edge
                prior_is_node = false;
                for (const auto& e : origin.edges) {
                    if (e.id == path_edges.back().edgeid) {
                        prior_is_node = e.begin_node() || e.end_node();
                        break;
                    }
                }
                through_edge = path_edges.back().edgeid;

                // Add to list of through locations for this leg
                through_loc.emplace_back(destination);
            }

            // If we have another one coming we need to clear
            if (--correlated.cend() != path_location)
                path_algorithm->Clear();
        }

        return trippaths;
    }

} }

#else

namespace valhalla { namespace midgard {

    typedef std::pair<float, float> PointLL;

    template<typename Point>
    class Shape5Decoder {
    public:
        Shape5Decoder(const char* begin, const size_t size) : begin(begin), end(begin + size) {
        }
        Point pop() noexcept(false) {
            lat = next(lat);
            lon = next(lon);
            return Point(typename Point::first_type(double(lon) * 1e-6),
                         typename Point::second_type(double(lat) * 1e-6));
        }
        bool empty() const {
            return begin == end;
        }

    private:
        const char* begin;
        const char* end;
        int32_t lat = 0;
        int32_t lon = 0;

        int32_t next(const int32_t previous) noexcept(false) {
            //grab each 5 bits and mask it in where it belongs using the shift
            int byte, shift = 0, result = 0;
              do {
                if(empty()) throw std::runtime_error("Bad encoded polyline");
                //take the least significant 5 bits shifted into place
                byte = int32_t(*begin++) - 63;
                result |= (byte & 0x1f) << shift;
                shift += 5;
                //if the most significant bit is set there is more to this number
            } while (byte >= 0x20);
            //handle the bit flipping and add to previous since its an offset
            return previous + (result & 1 ? ~(result >> 1) : (result >> 1));
        }
    };

    // specialized implemetation for std::vector with reserve
    template<class container_t,
             class ShapeDecoder = Shape5Decoder<typename container_t::value_type>>
    container_t decode(const std::string& encoded) {
        ShapeDecoder shape(encoded.c_str(), encoded.size());
        container_t c;
        c.reserve(encoded.size() / 4);
        while (!shape.empty()) {
            c.emplace_back(shape.pop());
        }
        return c;
    }

    template<class container_t>
    std::string encode(const container_t& points) {
        //a place to keep the output
        std::string output;
        //unless the shape is very course you should probably only need about 3 bytes
        //per coord, which is 6 bytes with 2 coords, so we overshoot to 8 just in case
        output.reserve(points.size() * 8);

        //handy lambda to turn an integer into an encoded string
        auto serialize = [&output](int number) {
            //move the bits left 1 position and flip all the bits if it was a negative number
            number = number < 0 ? ~(number << 1) : (number << 1);
            //write 5 bit chunks of the number
            while (number >= 0x20) {
                int nextValue = (0x20 | (number & 0x1f)) + 63;
                output.push_back(static_cast<char>(nextValue));
                number >>= 5;
            }
            //write the last chunk
            number += 63;
            output.push_back(static_cast<char>(number));
        };

        //this is an offset encoding so we remember the last point we saw
        int last_lon = 0, last_lat = 0;
        //for each point
        for (const auto& p : points) {
            //shift the decimal point 5 places to the right and truncate
            int lon = static_cast<int>(floor(static_cast<double>(p.first) * 1e6));
            int lat = static_cast<int>(floor(static_cast<double>(p.second) * 1e6));
            //encode each coordinate, lat first for some reason
            serialize(lat - last_lat);
            serialize(lon - last_lon);
            //remember the last one we encountered
            last_lon = lon;
            last_lat = lat;
        }
        return output;
    }

} }
#endif

namespace carto {

    std::shared_ptr<RouteMatchingResult> ValhallaRoutingProxy::MatchRoute(const std::string& baseURL, const std::string& profile, const std::shared_ptr<RouteMatchingRequest>& request) {
        EPSG3857 epsg3857;
        std::shared_ptr<Projection> proj = request->getProjection();

        std::vector<valhalla::midgard::PointLL> points;
        for (const MapPos& pos : request->getPoints()) {
            MapPos posWgs84 = proj->toWgs84(pos);
            points.emplace_back(static_cast<float>(posWgs84.getX()), static_cast<float>(posWgs84.getY()));
        }

        picojson::object json;
        json["encoded_polyline"] = picojson::value(valhalla::midgard::encode(points));
        json["shape_match"] = picojson::value("map_snap");
        json["costing"] = picojson::value(profile);
        if (profile == "wheelchair") {
            picojson::object pedestrianOptions;
            pedestrianOptions["type"] = picojson::value("wheelchair");
            picojson::object costingOptions;
            costingOptions["pedestrian"] = picojson::value(pedestrianOptions);
            json["costing"] = picojson::value("pedestrian");
            json["costing_options"] = picojson::value(costingOptions);
        }

        std::map<std::string, std::string> params;
        params["json"] = picojson::value(json).serialize();
        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);
        Log::Debugf("ValhallaRoutingProxy::MatchRoute: Loading %s", url.c_str());

        std::shared_ptr<BinaryData> responseData;
        if (!NetworkUtils::GetHTTP(url, responseData, Log::IsShowDebug())) {
            throw NetworkException("Failed to fetch response"); // NOTE: we may have error messages, thus do not return from here
        }

        std::string responseString;
        if (responseData) {
            responseString = std::string(reinterpret_cast<const char*>(responseData->data()), responseData->size());
        } else {
            throw GenericException("Empty response");
        }

        picojson::value response;
        std::string err = picojson::parse(response, responseString);
        if (!err.empty()) {
            throw GenericException("Failed to parse response", err);
        }

        if (!response.get("trip").is<picojson::object>()) {
            throw GenericException("No trip info in the response");
        }

        std::vector<MapPos> poses;

        try {
            for (const picojson::value& legInfo : response.get("trip").get("legs").get<picojson::array>()) {
                std::vector<valhalla::midgard::PointLL> shape = valhalla::midgard::decode<std::vector<valhalla::midgard::PointLL> >(legInfo.get("shape").get<std::string>());
                poses.reserve(poses.size() + shape.size());

                const picojson::array& maneuvers = legInfo.get("maneuvers").get<picojson::array>();
                for (const picojson::value& maneuver : maneuvers) {
                    for (std::size_t j = static_cast<std::size_t>(maneuver.get("begin_shape_index").get<std::int64_t>()); j <= static_cast<std::size_t>(maneuver.get("end_shape_index").get<std::int64_t>()); j++) {
                        const valhalla::midgard::PointLL& point = shape.at(j);
                        poses.push_back(proj->fromLatLong(point.second, point.first));
                    }
                }
            }
        }
        catch (const std::exception& ex) {
            throw GenericException("Exception while translating route", ex.what());
        }

        return std::make_shared<RouteMatchingResult>(proj, poses);
    }

    std::shared_ptr<RoutingResult> ValhallaRoutingProxy::CalculateRoute(const std::string& baseURL, const std::string& profile, const std::shared_ptr<RoutingRequest>& request) {
        EPSG3857 epsg3857;
        std::shared_ptr<Projection> proj = request->getProjection();

        picojson::array locations;
        for (const MapPos& pos : request->getPoints()) {
            MapPos posWgs84 = proj->toWgs84(pos);
            picojson::object location;
            location["lon"] = picojson::value(posWgs84.getX());
            location["lat"] = picojson::value(posWgs84.getY());
            locations.emplace_back(location);
        }

        picojson::object json;
        json["locations"] = picojson::value(locations);
        json["costing"] = picojson::value(profile);
        if (profile == "wheelchair") {
            picojson::object pedestrianOptions;
            pedestrianOptions["type"] = picojson::value("wheelchair");
            picojson::object costingOptions;
            costingOptions["pedestrian"] = picojson::value(pedestrianOptions);
            json["costing"] = picojson::value("pedestrian");
            json["costing_options"] = picojson::value(costingOptions);
        }

        std::map<std::string, std::string> params;
        params["json"] = picojson::value(json).serialize();
        std::string url = NetworkUtils::BuildURLFromParameters(baseURL, params);
        Log::Debugf("ValhallaRoutingProxy::CalculateRoute: Loading %s", url.c_str());

        std::shared_ptr<BinaryData> responseData;
        if (!NetworkUtils::GetHTTP(url, responseData, Log::IsShowDebug())) {
            throw NetworkException("Failed to fetch response"); // NOTE: we may have error messages, thus do not return from here
        }

        std::string responseString;
        if (responseData) {
            responseString = std::string(reinterpret_cast<const char*>(responseData->data()), responseData->size());
        } else {
            throw GenericException("Empty response");
        }

        picojson::value response;
        std::string err = picojson::parse(response, responseString);
        if (!err.empty()) {
            throw GenericException("Failed to parse response", err);
        }

        if (!response.get("trip").is<picojson::object>()) {
            throw GenericException("No trip info in the response");
        }

        std::vector<MapPos> points;
        std::vector<MapPos> epsg3857Points;
        std::vector<RoutingInstruction> instructions;

        try {
            for (const picojson::value& legInfo : response.get("trip").get("legs").get<picojson::array>()) {
                std::vector<valhalla::midgard::PointLL> shape = valhalla::midgard::decode<std::vector<valhalla::midgard::PointLL> >(legInfo.get("shape").get<std::string>());
                points.reserve(points.size() + shape.size());
                epsg3857Points.reserve(epsg3857Points.size() + shape.size());

                const picojson::array& maneuvers = legInfo.get("maneuvers").get<picojson::array>();
                for (const picojson::value& maneuver : maneuvers) {
                    RoutingAction::RoutingAction action = RoutingAction::ROUTING_ACTION_NO_TURN;
                    TranslateManeuverType(static_cast<int>(maneuver.get("type").get<std::int64_t>()), action);

                    std::size_t pointIndex = points.size();
                    for (std::size_t j = static_cast<std::size_t>(maneuver.get("begin_shape_index").get<std::int64_t>()); j <= static_cast<std::size_t>(maneuver.get("end_shape_index").get<std::int64_t>()); j++) {
                        const valhalla::midgard::PointLL& point = shape.at(j);
                        epsg3857Points.push_back(epsg3857.fromLatLong(point.second, point.first));
                        points.push_back(proj->fromLatLong(point.second, point.first));
                    }

                    float turnAngle = CalculateTurnAngle(epsg3857Points, pointIndex);
                    float azimuth = CalculateAzimuth(epsg3857Points, pointIndex);

                    std::string streetName;
                    if (maneuver.get("street_names").is<picojson::array>()) {
                        const picojson::array& streetNames = maneuver.get("street_names").get<picojson::array>();
                        streetName = !streetNames.empty() ? streetNames[0].get<std::string>() : std::string("");
                    }

                    instructions.emplace_back(
                        action,
                        pointIndex,
                        streetName,
                        turnAngle,
                        azimuth,
                        maneuver.get("length").get<double>() * 1000.0,
                        maneuver.get("time").get<double>()
                    );
                }
            }
        }
        catch (const std::exception& ex) {
            throw GenericException("Exception while translating route", ex.what());
        }

        return std::make_shared<RoutingResult>(proj, points, instructions);
    }

#ifdef _CARTO_VALHALLA_ROUTING_SUPPORT
    std::shared_ptr<RouteMatchingResult> ValhallaRoutingProxy::MatchRoute(const std::vector<std::shared_ptr<sqlite3pp::database> >& databases, const std::string& profile, const std::shared_ptr<RouteMatchingRequest>& request) {
        EPSG3857 epsg3857;
        std::shared_ptr<Projection> proj = request->getProjection();

        const float searchRadius = 50.0f;
        std::vector<valhalla::meili::Measurement> measurements;
        try {
            for (const MapPos& pos : request->getPoints()) {
                MapPos posWgs84 = proj->toWgs84(pos);
                valhalla::midgard::PointLL lnglat(static_cast<float>(posWgs84.getX()), static_cast<float>(posWgs84.getY()));
                measurements.emplace_back(lnglat, request->getAccuracy(), searchRadius);
            }

            valhalla::meili::map_matcher_factory_t factory(databases, profile);
            std::shared_ptr<valhalla::meili::MapMatcher> matcher(factory.Create(profile));
            if (!matcher) {
                throw std::runtime_error("Failed to create matcher instance");
            }

            std::vector<valhalla::meili::MatchResult> matchResults = matcher->OfflineMatch(measurements);

            std::vector<MapPos> poses;
            for (const valhalla::meili::MatchResult& matchResult : matchResults) {
                MapPos pos = proj->fromLatLong(matchResult.lnglat().lat(), matchResult.lnglat().lng());
                poses.push_back(pos);
            }
            return std::make_shared<RouteMatchingResult>(proj, poses);
        }
        catch (const std::exception& ex) {
            throw GenericException("Exception while matching route", ex.what());
        }
    }

    std::shared_ptr<RoutingResult> ValhallaRoutingProxy::CalculateRoute(const std::vector<std::shared_ptr<sqlite3pp::database> >& databases, const std::string& profile, const std::shared_ptr<RoutingRequest>& request) {
        EPSG3857 epsg3857;
        std::shared_ptr<Projection> proj = request->getProjection();
        
        std::list<valhalla::odin::TripPath> tripPaths;
        try {
            std::vector<valhalla::midgard::PointLL> points;
            for (const MapPos& pos : request->getPoints()) {
                MapPos posWgs84 = proj->toWgs84(pos);
                points.emplace_back(static_cast<float>(posWgs84.getX()), static_cast<float>(posWgs84.getY()));
            }
            
            valhalla::thor::thor_worker_t worker(databases, profile);
            tripPaths = worker.path_depart_at(points, profile == "multimodal" ? boost::optional<int>(0) : boost::optional<int>());
        }
        catch (const std::exception& ex) {
            throw GenericException("Exception while calculating route", ex.what());
        }

        std::vector<MapPos> points;
        std::vector<MapPos> epsg3857Points;
        std::vector<RoutingInstruction> instructions;

        for (valhalla::odin::TripPath& tripPath : tripPaths) {
            valhalla::odin::DirectionsOptions directionsOptions;
            valhalla::odin::DirectionsBuilder directions;
            valhalla::odin::TripDirections tripDirections;
            try {
                tripDirections = directions.Build(directionsOptions, tripPath);

                std::vector<valhalla::midgard::PointLL> shape = valhalla::midgard::decode<std::vector<PointLL> >(tripDirections.shape());
                points.reserve(points.size() + shape.size());
                epsg3857Points.reserve(epsg3857Points.size() + shape.size());

                for (int i = 0; i < tripDirections.maneuver_size(); i++) {
                    const valhalla::odin::TripDirections_Maneuver& maneuver = tripDirections.maneuver(i);

                    RoutingAction::RoutingAction action = RoutingAction::ROUTING_ACTION_NO_TURN;
                    TranslateManeuverType(static_cast<int>(maneuver.type()), action);

                    std::size_t pointIndex = points.size();
                    for (int j = maneuver.begin_shape_index(); j <= maneuver.end_shape_index(); j++) {
                        const valhalla::midgard::PointLL& point = shape.at(j);
                        epsg3857Points.push_back(epsg3857.fromLatLong(point.lat(), point.lng()));
                        points.push_back(proj->fromLatLong(point.lat(), point.lng()));
                    }

                    float turnAngle = CalculateTurnAngle(epsg3857Points, pointIndex);

                    instructions.emplace_back(
                        action,
                        pointIndex,
                        maneuver.street_name_size() ? maneuver.street_name(0) : std::string(""),
                        turnAngle,
                        maneuver.begin_heading(),
                        maneuver.length() * 1000.0,
                        maneuver.time()
                    );
                }
            }
            catch (const std::exception& ex) {
                throw GenericException("Exception while translating route", ex.what());
            }
        }

        return std::make_shared<RoutingResult>(proj, points, instructions);
    }
#endif
    
    float ValhallaRoutingProxy::CalculateTurnAngle(const std::vector<MapPos>& epsg3857Points, int pointIndex) {
        int pointIndex0 = pointIndex;
        while (--pointIndex0 >= 0) {
            if (epsg3857Points.at(pointIndex0) != epsg3857Points.at(pointIndex)) {
                break;
            }
        }
        int pointIndex1 = pointIndex;
        while (++pointIndex1 < static_cast<int>(epsg3857Points.size())) {
            if (epsg3857Points.at(pointIndex1) != epsg3857Points.at(pointIndex)) {
                break;
            }
        }

        float turnAngle = 0;
        if (pointIndex0 >= 0 && pointIndex1 < static_cast<int>(epsg3857Points.size())) {
            const MapPos& p0 = epsg3857Points.at(pointIndex0);
            const MapPos& p1 = epsg3857Points.at(pointIndex);
            const MapPos& p2 = epsg3857Points.at(pointIndex1);
            MapVec v1 = p1 - p0;
            MapVec v2 = p2 - p1;
            double dot = v1.dotProduct(v2) / v1.length() / v2.length();
            turnAngle = static_cast<float>(std::acos(std::max(-1.0, std::min(1.0, dot))) * Const::RAD_TO_DEG);
        }
        return turnAngle;
    }

    float ValhallaRoutingProxy::CalculateAzimuth(const std::vector<MapPos>& epsg3857Points, int pointIndex) {
        int step = 1;
        for (int i = pointIndex; i >= 0; i += step) {
            if (i + 1 >= static_cast<int>(epsg3857Points.size())) {
                step = -1;
                continue;
            }
            const MapPos& p0 = epsg3857Points.at(i);
            const MapPos& p1 = epsg3857Points.at(i + 1);
            MapVec v = p1 - p0;
            if (v.length() > 0) {
                float angle = static_cast<float>(std::atan2(v.getY(), v.getX()) * Const::RAD_TO_DEG);
                float azimuth = 90 - angle;
                return (azimuth < 0 ? azimuth + 360 : azimuth);
            }
        }
        return std::numeric_limits<float>::quiet_NaN();
    }

    bool ValhallaRoutingProxy::TranslateManeuverType(int maneuverType, RoutingAction::RoutingAction& action) {
        enum {
            TripDirections_Maneuver_Type_kNone = 0,
            TripDirections_Maneuver_Type_kStart = 1,
            TripDirections_Maneuver_Type_kStartRight = 2,
            TripDirections_Maneuver_Type_kStartLeft = 3,
            TripDirections_Maneuver_Type_kDestination = 4,
            TripDirections_Maneuver_Type_kDestinationRight = 5,
            TripDirections_Maneuver_Type_kDestinationLeft = 6,
            TripDirections_Maneuver_Type_kBecomes = 7,
            TripDirections_Maneuver_Type_kContinue = 8,
            TripDirections_Maneuver_Type_kSlightRight = 9,
            TripDirections_Maneuver_Type_kRight = 10,
            TripDirections_Maneuver_Type_kSharpRight = 11,
            TripDirections_Maneuver_Type_kUturnRight = 12,
            TripDirections_Maneuver_Type_kUturnLeft = 13,
            TripDirections_Maneuver_Type_kSharpLeft = 14,
            TripDirections_Maneuver_Type_kLeft = 15,
            TripDirections_Maneuver_Type_kSlightLeft = 16,
            TripDirections_Maneuver_Type_kRampStraight = 17,
            TripDirections_Maneuver_Type_kRampRight = 18,
            TripDirections_Maneuver_Type_kRampLeft = 19,
            TripDirections_Maneuver_Type_kExitRight = 20,
            TripDirections_Maneuver_Type_kExitLeft = 21,
            TripDirections_Maneuver_Type_kStayStraight = 22,
            TripDirections_Maneuver_Type_kStayRight = 23,
            TripDirections_Maneuver_Type_kStayLeft = 24,
            TripDirections_Maneuver_Type_kMerge = 25,
            TripDirections_Maneuver_Type_kRoundaboutEnter = 26,
            TripDirections_Maneuver_Type_kRoundaboutExit = 27,
            TripDirections_Maneuver_Type_kFerryEnter = 28,
            TripDirections_Maneuver_Type_kFerryExit = 29,
            TripDirections_Maneuver_Type_kTransit = 30,
            TripDirections_Maneuver_Type_kTransitTransfer = 31,
            TripDirections_Maneuver_Type_kTransitRemainOn = 32,
            TripDirections_Maneuver_Type_kTransitConnectionStart = 33,
            TripDirections_Maneuver_Type_kTransitConnectionTransfer = 34,
            TripDirections_Maneuver_Type_kTransitConnectionDestination = 35,
            TripDirections_Maneuver_Type_kPostTransitConnectionDestination = 36
        };

        static const std::unordered_map<int, RoutingAction::RoutingAction> maneuverTypes = {
            { TripDirections_Maneuver_Type_kNone,             carto::RoutingAction::ROUTING_ACTION_NO_TURN },//NoTurn = 0,
            { TripDirections_Maneuver_Type_kContinue,         carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kBecomes,          carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kRampStraight,     carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kStayStraight,     carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kMerge,            carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kFerryEnter,       carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kFerryExit,        carto::RoutingAction::ROUTING_ACTION_GO_STRAIGHT },//GoStraight,
            { TripDirections_Maneuver_Type_kSlightRight,      carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnSlightRight,
            { TripDirections_Maneuver_Type_kRight,            carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnRight,
            { TripDirections_Maneuver_Type_kRampRight,        carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnRight,
            { TripDirections_Maneuver_Type_kExitRight,        carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnRight,
            { TripDirections_Maneuver_Type_kStayRight,        carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnRight,
            { TripDirections_Maneuver_Type_kSharpRight,       carto::RoutingAction::ROUTING_ACTION_TURN_RIGHT },//TurnSharpRight,
            { TripDirections_Maneuver_Type_kUturnLeft,        carto::RoutingAction::ROUTING_ACTION_UTURN },//UTurn,
            { TripDirections_Maneuver_Type_kUturnRight,       carto::RoutingAction::ROUTING_ACTION_UTURN },//UTurn,
            { TripDirections_Maneuver_Type_kSharpLeft,        carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnSharpLeft,
            { TripDirections_Maneuver_Type_kLeft,             carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnLeft,
            { TripDirections_Maneuver_Type_kRampLeft,         carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnLeft,
            { TripDirections_Maneuver_Type_kExitLeft,         carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnLeft,
            { TripDirections_Maneuver_Type_kStayLeft,         carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnLeft,
            { TripDirections_Maneuver_Type_kSlightLeft,       carto::RoutingAction::ROUTING_ACTION_TURN_LEFT },//TurnSlightLeft,
            { TripDirections_Maneuver_Type_kRoundaboutEnter,  carto::RoutingAction::ROUTING_ACTION_ENTER_ROUNDABOUT },//EnterRoundAbout,
            { TripDirections_Maneuver_Type_kRoundaboutExit,   carto::RoutingAction::ROUTING_ACTION_LEAVE_ROUNDABOUT },//LeaveRoundAbout,
            { TripDirections_Maneuver_Type_kStart,            carto::RoutingAction::ROUTING_ACTION_START_AT_END_OF_STREET },//StartAtEndOfStreet,
            { TripDirections_Maneuver_Type_kStartRight,       carto::RoutingAction::ROUTING_ACTION_START_AT_END_OF_STREET },//StartAtEndOfStreet,
            { TripDirections_Maneuver_Type_kStartLeft,        carto::RoutingAction::ROUTING_ACTION_START_AT_END_OF_STREET },//StartAtEndOfStreet,
            { TripDirections_Maneuver_Type_kDestination,      carto::RoutingAction::ROUTING_ACTION_FINISH },//ReachedYourDestination,
            { TripDirections_Maneuver_Type_kDestinationRight, carto::RoutingAction::ROUTING_ACTION_FINISH },//ReachedYourDestination,
            { TripDirections_Maneuver_Type_kDestinationLeft,  carto::RoutingAction::ROUTING_ACTION_FINISH },//ReachedYourDestination,
            // NOTE: transit maneuvers are missing from this table
        };

        auto it = maneuverTypes.find(maneuverType);
        if (it != maneuverTypes.end()) {
            action = it->second;
            return true;
        }

        Log::Infof("ValhallaRoutingProxy::TranslateManeuverType: ignoring maneuver %d", maneuverType);
        return false;
    }
    
    ValhallaRoutingProxy::ValhallaRoutingProxy() {
    }

}

#endif
