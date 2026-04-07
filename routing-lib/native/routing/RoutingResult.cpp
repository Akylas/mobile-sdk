#include "RoutingResult.h"

namespace routing {

    RoutingResult::RoutingResult(std::string rawResult) :
        _rawResult(std::move(rawResult))
    {
    }

    RoutingResult::~RoutingResult() {}

    const std::string& RoutingResult::getRawResult() const { return _rawResult; }

    std::string RoutingResult::toString() const {
        return "RoutingResult [rawResult.size=" + std::to_string(_rawResult.size()) + "]";
    }

} // namespace routing
