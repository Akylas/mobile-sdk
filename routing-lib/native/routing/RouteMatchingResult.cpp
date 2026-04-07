#include "RouteMatchingResult.h"

namespace routing {

    RouteMatchingResult::RouteMatchingResult(std::string rawResult) :
        _rawResult(std::move(rawResult))
    {
    }

    RouteMatchingResult::~RouteMatchingResult() {}

    const std::string& RouteMatchingResult::getRawResult() const { return _rawResult; }

    std::string RouteMatchingResult::toString() const {
        return "RouteMatchingResult [rawResult.size=" + std::to_string(_rawResult.size()) + "]";
    }

} // namespace routing
