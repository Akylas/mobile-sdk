#pragma once

#include "../core/MapPos.h"
#include "../core/Variant.h"

#include <mutex>
#include <vector>
#include <map>
#include <string>

namespace routing {

    /**
     * Describes a route calculation request. Pass to ValhallaRoutingService::calculateRoute().
     *
     * Points are WGS-84 coordinates: MapPos(lon, lat).
     *
     * All setCustomParameter / setPointParameter calls use dot-separated paths
     * (e.g. "costing_options.auto.toll_booth_penalty") resolved into nested JSON.
     */
    class RoutingRequest {
    public:
        /**
         * @param points  WGS-84 waypoints as MapPos(lon, lat).
         */
        explicit RoutingRequest(const std::vector<MapPos>& points);
        virtual ~RoutingRequest();

        const std::vector<MapPos>& getPoints() const;

        Variant getPointParameters(int index) const;
        Variant getPointParameter(int index, const std::string& param) const;
        void setPointParameter(int index, const std::string& param, const Variant& value);

        Variant getCustomParameters() const;
        Variant getCustomParameter(const std::string& param) const;
        void setCustomParameter(const std::string& param, const Variant& value);

        std::string toString() const;

    private:
        const std::vector<MapPos> _points;
        std::map<int, Variant> _pointParams;
        Variant _customParams;
        mutable std::mutex _mutex;
    };

} // namespace routing
