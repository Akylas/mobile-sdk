#pragma once

#include "../core/MapPos.h"
#include "../core/Variant.h"
#include "../core/Projection.h"

#include <memory>
#include <mutex>
#include <map>
#include <vector>
#include <string>

namespace routing {

    /**
     * Describes a route-matching request (GPS trace -> road network snap).
     */
    class RouteMatchingRequest {
    public:
        RouteMatchingRequest(const std::shared_ptr<Projection>& projection,
                             const std::vector<MapPos>& points,
                             float accuracy);
        virtual ~RouteMatchingRequest();

        const std::shared_ptr<Projection>& getProjection() const;
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
        const std::shared_ptr<Projection> _projection;
        const std::vector<MapPos> _points;
        const float _accuracy;
        std::map<int, Variant> _pointParams;
        Variant _customParams;
        mutable std::mutex _mutex;
    };

} // namespace routing
