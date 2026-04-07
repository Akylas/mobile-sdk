#pragma once

#include <string>

namespace routing {

    /**
     * Result of a route-matching operation.
     * The full Valhalla JSON response is exposed via getRawResult().
     * Parsing of matched_points, edges etc. is the responsibility of the
     * application layer.
     */
    class RouteMatchingResult {
    public:
        explicit RouteMatchingResult(std::string rawResult);
        virtual ~RouteMatchingResult();

        const std::string& getRawResult() const;
        std::string toString() const;

    private:
        std::string _rawResult;
    };

} // namespace routing
