#include "RouteMatchingResult.h"
#include "../exceptions/Exceptions.h"

#include <sstream>

namespace routing {

    RouteMatchingResult::RouteMatchingResult(const std::shared_ptr<Projection>& projection,
                                             std::vector<RouteMatchingPoint> matchingPoints,
                                             std::vector<RouteMatchingEdge> matchingEdges,
                                             std::string rawResult) :
        _projection(projection),
        _matchingPoints(std::move(matchingPoints)),
        _matchingEdges(std::move(matchingEdges)),
        _rawResult(std::move(rawResult))
    {
        if (!projection) throw NullArgumentException("Null projection");
    }

    RouteMatchingResult::~RouteMatchingResult() {}

    const std::shared_ptr<Projection>& RouteMatchingResult::getProjection() const { return _projection; }

    std::vector<MapPos> RouteMatchingResult::getPoints() const {
        std::vector<MapPos> poses;
        poses.reserve(_matchingPoints.size());
        for (const auto& pt : _matchingPoints) poses.push_back(pt.getPos());
        return poses;
    }

    const std::vector<RouteMatchingEdge>& RouteMatchingResult::getMatchingEdges() const { return _matchingEdges; }
    const std::vector<RouteMatchingPoint>& RouteMatchingResult::getMatchingPoints() const { return _matchingPoints; }
    const std::string& RouteMatchingResult::getRawResult() const { return _rawResult; }

    std::string RouteMatchingResult::toString() const {
        std::stringstream ss;
        ss << "RouteMatchingResult [matchingPoints=[";
        for (auto it = _matchingPoints.begin(); it != _matchingPoints.end(); ++it)
            ss << (it == _matchingPoints.begin() ? "" : ", ") << it->toString();
        ss << "], matchingEdges=[";
        for (auto it = _matchingEdges.begin(); it != _matchingEdges.end(); ++it)
            ss << (it == _matchingEdges.begin() ? "" : ", ") << it->toString();
        ss << "]]";
        return ss.str();
    }

} // namespace routing
