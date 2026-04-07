#pragma once

#include <string>

namespace routing {

    /**
     * Result of a route calculation.
     * The full Valhalla JSON response is exposed via getRawResult().
     * Parsing of individual fields (legs, maneuvers, geometry …) is the
     * responsibility of the application layer.
     */
    class RoutingResult {
    public:
        explicit RoutingResult(std::string rawResult);
        virtual ~RoutingResult();

        const std::string& getRawResult() const;
        std::string toString() const;

    private:
        std::string _rawResult;
    };

} // namespace routing
