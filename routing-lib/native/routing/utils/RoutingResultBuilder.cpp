#include "RoutingResultBuilder.h"
#include "../../core/EPSG3857.h"
#include "../../core/Constants.h"

#include <cmath>
#include <limits>
#include <utility>
#include <sstream>
#include <iomanip>

namespace routing {

    RoutingResultBuilder::RoutingResultBuilder(const std::shared_ptr<Projection>& proj,
                                               const std::string& rawResult) :
        _projection(proj),
        _rawResult(rawResult),
        _points(),
        _instructions()
    {
    }

    int RoutingResultBuilder::addPoints(const std::vector<MapPos>& points) {
        int pointIndex0 = static_cast<int>(_points.size());
        for (std::size_t i = 0; i < points.size(); i++) {
            if (i != 0 || _points.empty() || _points.back() != points[i]) {
                _points.push_back(points[i]);
            }
            if (i == 0) {
                pointIndex0 = static_cast<int>(_points.size() - 1);
            }
        }
        return pointIndex0;
    }

    RoutingInstructionBuilder& RoutingResultBuilder::addInstruction(RoutingAction::RoutingAction action,
                                                                    int pointIndex) {
        _instructions.emplace_back();
        RoutingInstructionBuilder& b = _instructions.back();
        b.setAction(action);
        b.setPointIndex(pointIndex);
        b.setAzimuth(std::numeric_limits<float>::quiet_NaN());
        b.setTurnAngle(std::numeric_limits<float>::quiet_NaN());
        return b;
    }

    std::shared_ptr<RoutingResult> RoutingResultBuilder::buildRoutingResult() const {
        std::vector<RoutingInstruction> instructions;
        instructions.reserve(_instructions.size());
        for (RoutingInstructionBuilder b : _instructions) {
            if (std::isnan(b.getTurnAngle())) b.setTurnAngle(calculateTurnAngle(b.getPointIndex()));
            if (std::isnan(b.getAzimuth()))   b.setAzimuth(calculateAzimuth(b.getPointIndex()));
            if (b.getInstruction().empty())    b.setInstruction(calculateInstruction(b));
            instructions.push_back(b.buildRoutingInstruction());
        }
        return std::make_shared<RoutingResult>(_projection, _points, std::move(instructions), _rawResult);
    }

    float RoutingResultBuilder::calculateTurnAngle(int pointIndex) const {
        EPSG3857 epsg3857;
        if (pointIndex < 0 || pointIndex >= static_cast<int>(_points.size())) return 0.0f;

        MapPos p1 = epsg3857.fromInternal(_projection->toInternal(_points[pointIndex]));
        MapPos p0 = p1, p2 = p1;

        int idx0 = pointIndex;
        while (--idx0 >= 0) {
            p0 = epsg3857.fromInternal(_projection->toInternal(_points[idx0]));
            if (p0 != p1) break;
        }
        int idx2 = pointIndex;
        while (++idx2 < static_cast<int>(_points.size())) {
            p2 = epsg3857.fromInternal(_projection->toInternal(_points[idx2]));
            if (p2 != p1) break;
        }

        if (idx0 >= 0 && idx2 < static_cast<int>(_points.size())) {
            MapVec v10 = p1 - p0;
            MapVec v21 = p2 - p1;
            double dot = v10.dotProduct(v21) / (v10.length() * v21.length());
            return static_cast<float>(std::acos(std::max(-1.0, std::min(1.0, dot))) * ROUTING_RAD_TO_DEG);
        }
        return 0.0f;
    }

    float RoutingResultBuilder::calculateAzimuth(int pointIndex) const {
        EPSG3857 epsg3857;
        int step = 1;
        for (int i = pointIndex; i >= 0; i += step) {
            if (i + 1 >= static_cast<int>(_points.size())) { step = -1; continue; }
            MapPos p0 = epsg3857.fromInternal(_projection->toInternal(_points[i]));
            MapPos p1 = epsg3857.fromInternal(_projection->toInternal(_points[i + 1]));
            MapVec v = p1 - p0;
            if (v.length() > 0) {
                float angle   = static_cast<float>(std::atan2(v.getY(), v.getX()) * ROUTING_RAD_TO_DEG);
                float azimuth = 90.0f - angle;
                return azimuth < 0 ? azimuth + 360.0f : azimuth;
            }
        }
        return std::numeric_limits<float>::quiet_NaN();
    }

    std::string RoutingResultBuilder::calculateDirection(float azimuth) const {
        static const std::vector<std::pair<float, std::string>> dirs = {
            {   0.0f, "north"     }, {  45.0f, "northeast" },
            {  90.0f, "east"      }, { 135.0f, "southeast" },
            { 180.0f, "south"     }, { 225.0f, "southwest" },
            { 270.0f, "west"      }, { 315.0f, "northwest" },
        };
        float bestDiff = std::numeric_limits<float>::infinity();
        std::string best;
        for (const auto& d : dirs) {
            float diff = std::min(std::abs(azimuth - d.first), std::abs(azimuth - d.first - 360.0f));
            if (diff < bestDiff) { bestDiff = diff; best = d.second; }
        }
        return best;
    }

    std::string RoutingResultBuilder::calculateDistance(double distance) const {
        std::stringstream ss;
        if (distance < 1000) {
            ss << std::fixed << std::setprecision(distance >= 10 ? 0 : 1) << distance << "m";
        } else {
            ss << std::fixed << std::setprecision(distance >= 10000 ? 0 : 1) << (distance / 1000.0) << "km";
        }
        return ss.str();
    }

    std::string RoutingResultBuilder::calculateInstruction(const RoutingInstructionBuilder& instr) const {
        std::string direction = calculateDirection(instr.getAzimuth());
        std::string street = instr.getStreetName();
        if (!street.empty() && street.front() == '{' && street.back() == '}') street.clear();

        switch (instr.getAction()) {
        case RoutingAction::ROUTING_ACTION_HEAD_ON:
            return "Head on " + direction + (street.empty() ? "" : " on " + street);
        case RoutingAction::ROUTING_ACTION_FINISH:
            return "You have reached your destination";
        case RoutingAction::ROUTING_ACTION_NO_TURN:
        case RoutingAction::ROUTING_ACTION_GO_STRAIGHT:
            return "Go straight " + direction + (street.empty() ? "" : " on " + street);
        case RoutingAction::ROUTING_ACTION_TURN_RIGHT:
            return (instr.getTurnAngle() < 30.0f ? "Bear right" : "Turn right") + (street.empty() ? std::string() : " onto " + street);
        case RoutingAction::ROUTING_ACTION_UTURN:
            return "Make U turn" + (street.empty() ? std::string() : " onto " + street);
        case RoutingAction::ROUTING_ACTION_TURN_LEFT:
            return (instr.getTurnAngle() < 30.0f ? "Bear left" : "Turn left") + (street.empty() ? std::string() : " onto " + street);
        case RoutingAction::ROUTING_ACTION_REACH_VIA_LOCATION:
            return "You have reached your non-final destination";
        case RoutingAction::ROUTING_ACTION_ENTER_ROUNDABOUT:
            return "Enter the roundabout";
        case RoutingAction::ROUTING_ACTION_LEAVE_ROUNDABOUT:
            return "Exit the roundabout" + (street.empty() ? std::string() : " onto " + street);
        case RoutingAction::ROUTING_ACTION_STAY_ON_ROUNDABOUT:
            return "Stay on the roundabout";
        case RoutingAction::ROUTING_ACTION_START_AT_END_OF_STREET:
            return "Start at end of a street" + (street.empty() ? std::string() : " on " + street);
        case RoutingAction::ROUTING_ACTION_ENTER_AGAINST_ALLOWED_DIRECTION:
            return "Enter against the allowed direction" + (street.empty() ? std::string() : " on " + street);
        case RoutingAction::ROUTING_ACTION_LEAVE_AGAINST_ALLOWED_DIRECTION:
            return "Leave against the allowed direction" + (street.empty() ? std::string() : " onto " + street);
        case RoutingAction::ROUTING_ACTION_GO_UP:
            return "Go up for " + calculateDistance(instr.getDistance());
        case RoutingAction::ROUTING_ACTION_GO_DOWN:
            return "Go down for " + calculateDistance(instr.getDistance());
        case RoutingAction::ROUTING_ACTION_WAIT:
            return "Wait for your turn";
        case RoutingAction::ROUTING_ACTION_ENTER_FERRY:
            return street.empty() ? std::string("Take a ferry") : "Take the " + street + " ferry";
        case RoutingAction::ROUTING_ACTION_LEAVE_FERRY:
            return "Leave the ferry and head on " + direction + (street.empty() ? std::string() : " on " + street);
        }
        return {};
    }

} // namespace routing
