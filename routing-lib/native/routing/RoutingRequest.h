#pragma once

#include "../core/MapPos.h"
#include "../core/Variant.h"
#include "../core/Projection.h"

#include <memory>
#include <mutex>
#include <vector>
#include <map>
#include <string>

namespace routing {

    /**
     * Describes a route calculation request. Pass to ValhallaRoutingService::calculateRoute().
     *
     * All setCustomParameter / setPointParameter calls use dot-separated paths
     * (e.g. "costing_options.auto.toll_booth_penalty") which are resolved into
     * nested JSON without boost::split.
     */
    class RoutingRequest {
    public:
        RoutingRequest(const std::shared_ptr<Projection>& projection,
                       const std::vector<MapPos>& points);
        virtual ~RoutingRequest();

        const std::shared_ptr<Projection>& getProjection() const;
        const std::vector<MapPos>& getPoints() const;

        Variant getPointParameters(int index) const;
        Variant getPointParameter(int index, const std::string& param) const;
        void setPointParameter(int index, const std::string& param, const Variant& value);

        Variant getCustomParameters() const;
        Variant getCustomParameter(const std::string& param) const;
        void setCustomParameter(const std::string& param, const Variant& value);

        std::string toString() const;

    private:
        const std::shared_ptr<Projection> _projection;
        const std::vector<MapPos> _points;
        std::map<int, Variant> _pointParams;
        Variant _customParams;
        mutable std::mutex _mutex;
    };

} // namespace routing
