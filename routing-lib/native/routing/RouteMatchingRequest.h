#pragma once

#include "../core/MapPos.h"
#include "../core/Variant.h"

#include <mutex>
#include <map>
#include <vector>
#include <string>

namespace routing {

    /**
     * Describes a route-matching request (GPS trace -> road network snap).
     *
     * Points are WGS-84 coordinates: MapPos(lon, lat).
     */
    class RouteMatchingRequest {
    public:
        /**
         * @param points    WGS-84 GPS trace points as MapPos(lon, lat).
         * @param accuracy  GPS accuracy in metres (0 = use Valhalla default).
         */
        RouteMatchingRequest(const std::vector<MapPos>& points, float accuracy = 0.0f);
        virtual ~RouteMatchingRequest();

        const std::vector<MapPos>& getPoints() const;
        float getAccuracy() const;

        Variant getPointParameters(int index) const;
        Variant getPointParameter(int index, const std::string& param) const;
        void setPointParameter(int index, const std::string& param, const Variant& value);

        Variant getCustomParameters() const;
        Variant getCustomParameter(const std::string& param) const;
        void setCustomParameter(const std::string& param, const Variant& value);

        std::string toString() const;

    private:
        const std::vector<MapPos> _points;
        const float _accuracy;
        std::map<int, Variant> _pointParams;
        Variant _customParams;
        mutable std::mutex _mutex;
    };

} // namespace routing
