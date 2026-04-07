#include "RouteMatchingPoint.h"

#include <sstream>

namespace routing {

    RouteMatchingPoint::RouteMatchingPoint() :
        _pos(), _type(RouteMatchingPointType::ROUTE_MATCHING_POINT_UNMATCHED), _edgeIndex(-1) {}

    RouteMatchingPoint::RouteMatchingPoint(const MapPos& pos,
                                           RouteMatchingPointType::RouteMatchingPointType type,
                                           int edgeIndex) :
        _pos(pos), _type(type), _edgeIndex(edgeIndex) {}

    RouteMatchingPoint::~RouteMatchingPoint() {}

    const MapPos& RouteMatchingPoint::getPos() const { return _pos; }
    RouteMatchingPointType::RouteMatchingPointType RouteMatchingPoint::getType() const { return _type; }
    int RouteMatchingPoint::getEdgeIndex() const { return _edgeIndex; }

    std::string RouteMatchingPoint::toString() const {
        static const char* names[] = { "unmatched", "interpolated", "matched" };
        std::stringstream ss;
        ss << "RouteMatchingPoint [type=" << names[static_cast<int>(_type)]
           << ", pos=" << _pos.toString() << "]";
        return ss.str();
    }

} // namespace routing
