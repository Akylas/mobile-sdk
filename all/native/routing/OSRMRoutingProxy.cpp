#ifdef _CARTO_ROUTING_SUPPORT

#include "OSRMRoutingProxy.h"
#include "core/BinaryData.h"
#include "components/Exceptions.h"
#include "projections/Projection.h"
#include "projections/EPSG3857.h"
#include "routing/RoutingRequest.h"
#include "routing/RoutingResult.h"
#include "routing/RouteMatchingRequest.h"
#include "routing/RouteMatchingResult.h"
#include "network/HTTPClient.h"
#include "utils/NetworkUtils.h"
#include "utils/Const.h"
#include "utils/Log.h"

#include <boost/lexical_cast.hpp>

#include <rapidjson/rapidjson.h>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include <osrm/Graph.h>
#include <osrm/Query.h>
#include <osrm/Result.h>
#include <osrm/Instruction.h>
#include <osrm/RouteFinder.h>

namespace carto {

    std::shared_ptr<RoutingResult> OSRMRoutingProxy::CalculateRoute(const std::shared_ptr<osrm::RouteFinder>& routeFinder, const std::shared_ptr<RoutingRequest>& request) {
        std::shared_ptr<Projection> proj = request->getProjection();
        EPSG3857 epsg3857;

        std::size_t totalPoints = 0;
        std::size_t totalInstructions = 0;
        std::vector<osrm::Result> results;
        for (std::size_t i = 1; i < request->getPoints().size(); i++) {
            MapPos p0 = proj->toWgs84(request->getPoints()[i - 1]);
            MapPos p1 = proj->toWgs84(request->getPoints()[i]);
            osrm::Query query(osrm::WGSPos(p0.getY(), p0.getX()), osrm::WGSPos(p1.getY(), p1.getX()));
            osrm::Result result = routeFinder->find(query);
            if (result.getStatus() == osrm::Result::Status::FAILED) {
                throw GenericException("Routing failed");
            }
            totalPoints += result.getGeometry().size();
            totalInstructions += result.getInstructions().size();
            results.push_back(std::move(result));
        }

        std::vector<MapPos> points;
        points.reserve(totalPoints);
        std::vector<MapPos> epsg3857Points;
        epsg3857Points.reserve(totalPoints);
        std::vector<RoutingInstruction> instructions;
        instructions.reserve(totalInstructions);
        for (std::size_t i = 0; i < results.size(); i++) {
            const osrm::Result& result = results[i];
            if (result.getInstructions().empty()) {
                continue;
            }

            std::size_t pointIndex = points.size();
            for (const osrm::WGSPos& pos : result.getGeometry()) {
                points.push_back(proj->fromWgs84(MapPos(pos(1), pos(0))));
                epsg3857Points.push_back(epsg3857.fromWgs84(MapPos(pos(1), pos(0))));
            }

            for (const osrm::Instruction& instr : result.getInstructions()) {
                double distance = instr.getDistance();
                double time = instr.getTime();

                RoutingAction::RoutingAction action = RoutingAction::ROUTING_ACTION_NO_TURN;
                TranslateInstructionCode(static_cast<int>(instr.getType()), action);
                if (action == RoutingAction::ROUTING_ACTION_NO_TURN || action == RoutingAction::ROUTING_ACTION_STAY_ON_ROUNDABOUT) {
                    if (!instructions.empty()) {
                        instructions.back().setTime(instructions.back().getTime() + time);
                        instructions.back().setDistance(instructions.back().getDistance() + distance);
                    }
                    continue;
                }
                if (action == RoutingAction::ROUTING_ACTION_FINISH && i + 1 < results.size()) {
                    action = RoutingAction::ROUTING_ACTION_REACH_VIA_LOCATION;
                }

                int posIndex = static_cast<int>(pointIndex + instr.getGeometryIndex());
                std::string streetName = instr.getAddress();
                float turnAngle = CalculateTurnAngle(epsg3857Points, posIndex);
                float azimuth = CalculateAzimuth(epsg3857Points, posIndex);
                instructions.emplace_back(action, posIndex, streetName, turnAngle, azimuth, distance, time);
            }
        }

        return std::make_shared<RoutingResult>(proj, points, instructions);
    }

    std::shared_ptr<RoutingResult> OSRMRoutingProxy::CalculateRoute(HTTPClient& httpClient, const std::string& url, const std::shared_ptr<RoutingRequest>& request) {
        std::shared_ptr<Projection> proj = request->getProjection();
        EPSG3857 epsg3857;
        
        std::map<std::string, std::string> requestHeaders = NetworkUtils::CreateAppRefererHeader();
        requestHeaders["Connection"] = "close";
        std::map<std::string, std::string> responseHeaders;
        std::shared_ptr<BinaryData> responseData;
        if (httpClient.get(url, requestHeaders, responseHeaders, responseData) != 0) {
            std::string result;
            if (responseData) {
                result = std::string(reinterpret_cast<const char*>(responseData->data()), responseData->size());
            }
            throw GenericException("Failed to read routing data", result);
        }
        
        const char* responseDataPtr = reinterpret_cast<const char*>(responseData->data());
        std::string json(responseDataPtr, responseDataPtr + responseData->size());
        rapidjson::Document responseDoc;
        if (responseDoc.Parse<rapidjson::kParseDefaultFlags>(json.c_str()).HasParseError()) {
            std::string err = rapidjson::GetParseError_En(responseDoc.GetParseError());
            throw ParseException(err, json, static_cast<int>(responseDoc.GetErrorOffset()));
        }

        int statusCode = responseDoc["status"].GetInt();
        if (statusCode != 0 && statusCode != 200) {
            throw GenericException("Routing failed", responseDoc["status_message"].GetString());
        }
        
        std::vector<MapPos> wgs84Points = DecodeGeometry(responseDoc["route_geometry"].GetString());
        std::vector<MapPos> points;
        points.reserve(wgs84Points.size());
        std::vector<MapPos> epsg3857Points;
        epsg3857Points.reserve(wgs84Points.size());
        for (const MapPos& pos : wgs84Points) {
            points.push_back(proj->fromWgs84(pos));
            epsg3857Points.push_back(epsg3857.fromWgs84(pos));
        }
        
        std::vector<RoutingInstruction> instructions;
        instructions.reserve(responseDoc["route_instructions"].Size() + 2);
        for (rapidjson::Value::ValueIterator jit = responseDoc["route_instructions"].Begin(); jit != responseDoc["route_instructions"].End(); jit++) {
            rapidjson::Value& routeInstruction = *jit;
            double distance = routeInstruction[2].GetDouble();
            double time = routeInstruction[4].GetDouble();
            
            RoutingAction::RoutingAction action = RoutingAction::ROUTING_ACTION_NO_TURN;
            if (!TranslateInstructionCode(boost::lexical_cast<int>(routeInstruction[0].GetString()), action)) {
                if (!instructions.empty()) {
                    instructions.back().setTime(instructions.back().getTime() + time);
                    instructions.back().setDistance(instructions.back().getDistance() + distance);
                }
                continue;
            }
            
            std::string streetName = routeInstruction[1].GetString();
            int posIndex = routeInstruction[3].GetInt();
            float turnAngle = CalculateTurnAngle(epsg3857Points, posIndex);
            float azimuth = static_cast<float>(routeInstruction[7].GetDouble());
            instructions.emplace_back(action, posIndex, streetName, turnAngle, azimuth, distance, time);
        }
        
        return std::make_shared<RoutingResult>(proj, points, instructions);
    }
    
    float OSRMRoutingProxy::CalculateTurnAngle(const std::vector<MapPos>& epsg3857Points, int pointIndex) {
        int pointIndex0 = pointIndex;
        while (--pointIndex0 >= 0) {
            if (epsg3857Points.at(pointIndex0) != epsg3857Points.at(pointIndex)) {
                break;
            }
        }
        int pointIndex1 = pointIndex;
        while (++pointIndex1 < static_cast<int>(epsg3857Points.size())) {
            if (epsg3857Points.at(pointIndex1) != epsg3857Points.at(pointIndex)) {
                break;
            }
        }

        float turnAngle = 0;
        if (pointIndex0 >= 0 && pointIndex1 < static_cast<int>(epsg3857Points.size())) {
            const MapPos& p0 = epsg3857Points.at(pointIndex0);
            const MapPos& p1 = epsg3857Points.at(pointIndex);
            const MapPos& p2 = epsg3857Points.at(pointIndex1);
            MapVec v1 = p1 - p0;
            MapVec v2 = p2 - p1;
            double dot = v1.dotProduct(v2) / v1.length() / v2.length();
            turnAngle = static_cast<float>(std::acos(std::max(-1.0, std::min(1.0, dot))) * Const::RAD_TO_DEG);
        }
        return turnAngle;
    }
    
    float OSRMRoutingProxy::CalculateAzimuth(const std::vector<MapPos>& epsg3857Points, int pointIndex) {
        int step = 1;
        for (int i = pointIndex; i >= 0; i += step) {
            if (i + 1 >= static_cast<int>(epsg3857Points.size())) {
                step = -1;
                continue;
            }
            const MapPos& p0 = epsg3857Points.at(i);
            const MapPos& p1 = epsg3857Points.at(i + 1);
            MapVec v = p1 - p0;
            if (v.length() > 0) {
                float angle = static_cast<float>(std::atan2(v.getY(), v.getX()) * Const::RAD_TO_DEG);
                float azimuth = 90 - angle;
                return (azimuth < 0 ? azimuth + 360 : azimuth);
            }
        }
        return std::numeric_limits<float>::quiet_NaN();
    }
    
    bool OSRMRoutingProxy::TranslateInstructionCode(int instructionCode, RoutingAction::RoutingAction& action) {
        switch (static_cast<osrm::Instruction::Type>(instructionCode)) {
        case osrm::Instruction::Type::NO_TURN:
            action = RoutingAction::ROUTING_ACTION_NO_TURN;
            break;
        case osrm::Instruction::Type::GO_STRAIGHT:
            action = RoutingAction::ROUTING_ACTION_GO_STRAIGHT;
            break;
        case osrm::Instruction::Type::TURN_SLIGHT_RIGHT:
        case osrm::Instruction::Type::TURN_RIGHT:
        case osrm::Instruction::Type::TURN_SHARP_RIGHT:
            action = RoutingAction::ROUTING_ACTION_TURN_RIGHT;
            break;
        case osrm::Instruction::Type::UTURN:
            action = RoutingAction::ROUTING_ACTION_UTURN;
            break;
        case osrm::Instruction::Type::TURN_SLIGHT_LEFT:
        case osrm::Instruction::Type::TURN_LEFT:
        case osrm::Instruction::Type::TURN_SHARP_LEFT:
            action = RoutingAction::ROUTING_ACTION_TURN_LEFT;
            break;
        case osrm::Instruction::Type::REACH_VIA_LOCATION:
            action = RoutingAction::ROUTING_ACTION_REACH_VIA_LOCATION;
            break;
        case osrm::Instruction::Type::HEAD_ON:
            action = RoutingAction::ROUTING_ACTION_HEAD_ON;
            break;
        case osrm::Instruction::Type::ENTER_ROUNDABOUT:
            action = RoutingAction::ROUTING_ACTION_ENTER_ROUNDABOUT;
            break;
        case osrm::Instruction::Type::LEAVE_ROUNDABOUT:
            action = RoutingAction::ROUTING_ACTION_LEAVE_ROUNDABOUT;
            break;
        case osrm::Instruction::Type::STAY_ON_ROUNDABOUT:
            action = RoutingAction::ROUTING_ACTION_STAY_ON_ROUNDABOUT;
            break;
        case osrm::Instruction::Type::ENTER_AGAINST_ALLOWED_DIRECTION:
            action = RoutingAction::ROUTING_ACTION_ENTER_AGAINST_ALLOWED_DIRECTION;
            break;
        case osrm::Instruction::Type::LEAVE_AGAINST_ALLOWED_DIRECTION:
            action = RoutingAction::ROUTING_ACTION_LEAVE_AGAINST_ALLOWED_DIRECTION;
            break;
        case osrm::Instruction::Type::REACHED_YOUR_DESTINATION:
            action = RoutingAction::ROUTING_ACTION_FINISH;
            break;
        default:
            Log::Infof("OSRMRoutingProxy::TranslateInstructionCode: ignoring instruction %d", instructionCode);
            return false;
        }
        return true;
    }

    std::vector<MapPos> OSRMRoutingProxy::DecodeGeometry(const std::string& encodedGeometry) {
        std::vector<MapPos> points;
        points.reserve(encodedGeometry.size());
        int lat = 0;
        int lon = 0;
        for (std::size_t index = 0; index < encodedGeometry.size(); ) {
            unsigned int b;
            unsigned int shift = 0;
            unsigned int result = 0;
            do {
                b = encodedGeometry[index++] - 63;
                result |= (b & 0x1f) << shift;
                shift += 5;
            } while (b >= 0x20);
            int dlat = ((result & 1) ? ~(result >> 1) : (result >> 1));
            lat += dlat;
            
            shift = 0;
            result = 0;
            do {
                b = encodedGeometry[index++] - 63;
                result |= (b & 0x1f) << shift;
                shift += 5;
            } while (b >= 0x20);
            int dlon = ((result & 1) ? ~(result >> 1) : (result >> 1));
            lon += dlon;
            
            points.emplace_back(lon * COORDINATE_SCALE, lat * COORDINATE_SCALE);
        }
        return points;
    }
    
    OSRMRoutingProxy::OSRMRoutingProxy() {
    }
    
    const double OSRMRoutingProxy::COORDINATE_SCALE = 1.0e-6;
    
}

#endif
