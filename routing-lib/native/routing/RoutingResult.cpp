#include "RoutingResult.h"
#include "../exceptions/Exceptions.h"

#include <numeric>
#include <iomanip>
#include <sstream>

namespace routing {

    RoutingResult::RoutingResult(const std::shared_ptr<Projection>& projection,
                                 std::vector<MapPos> points,
                                 std::vector<RoutingInstruction> instructions,
                                 std::string rawResult) :
        _projection(projection),
        _points(std::move(points)),
        _instructions(std::move(instructions)),
        _rawResult(std::move(rawResult))
    {
        if (!projection) throw NullArgumentException("Null projection");
    }

    RoutingResult::~RoutingResult() {}

    const std::shared_ptr<Projection>& RoutingResult::getProjection() const { return _projection; }
    const std::vector<MapPos>& RoutingResult::getPoints() const { return _points; }
    const std::vector<RoutingInstruction>& RoutingResult::getInstructions() const { return _instructions; }

    double RoutingResult::getTotalDistance() const {
        return std::accumulate(_instructions.begin(), _instructions.end(), 0.0,
            [](double d, const RoutingInstruction& i) { return d + i.getDistance(); });
    }

    double RoutingResult::getTotalTime() const {
        return std::accumulate(_instructions.begin(), _instructions.end(), 0.0,
            [](double t, const RoutingInstruction& i) { return t + i.getTime(); });
    }

    const std::string& RoutingResult::getRawResult() const { return _rawResult; }

    std::string RoutingResult::toString() const {
        std::stringstream ss;
        ss << std::setiosflags(std::ios::fixed);
        ss << "RoutingResult [instructions=" << _instructions.size()
           << ", totalDistance=" << getTotalDistance()
           << ", totalTime=" << getTotalTime() << "]";
        return ss.str();
    }

} // namespace routing
