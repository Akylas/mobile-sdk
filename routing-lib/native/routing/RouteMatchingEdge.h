#pragma once

#include "../core/Variant.h"

#include <map>
#include <string>

namespace routing {

    /**
     * An edge in the road network returned by route matching.
     */
    class RouteMatchingEdge {
    public:
        RouteMatchingEdge();
        explicit RouteMatchingEdge(const std::map<std::string, Variant>& attributes);
        virtual ~RouteMatchingEdge();

        bool containsAttribute(const std::string& name) const;
        Variant getAttribute(const std::string& name) const;

        std::string toString() const;

    private:
        std::map<std::string, Variant> _attributes;
    };

} // namespace routing
