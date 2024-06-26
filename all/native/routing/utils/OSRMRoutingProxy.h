/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_OSRMROUTINGPROXY_H_
#define _CARTO_OSRMROUTINGPROXY_H_

//#ifdef _CARTO_ROUTING_SUPPORT

#include "core/MapPos.h"
#include "routing/RoutingInstruction.h"

#include <memory>
#include <string>
#include <vector>

namespace carto {
    namespace osrm {
        class RouteFinder;
    }
    
    class HTTPClient;
    class RoutingRequest;
    class RoutingResult;
    class RouteMatchingRequest;
    class RouteMatchingResult;

    class OSRMRoutingProxy {
    public:
        static std::shared_ptr<RoutingResult> CalculateRoute(const std::shared_ptr<osrm::RouteFinder>& routeFinder, const std::shared_ptr<RoutingRequest>& request);
        
        static std::shared_ptr<RoutingResult> CalculateRoute(HTTPClient& httpClient, const std::string& url, const std::shared_ptr<RoutingRequest>& request);

    private:
        OSRMRoutingProxy();
        
        static bool TranslateInstructionCode(int instructionCode, RoutingAction::RoutingAction& action);
        
        static std::vector<MapPos> DecodeGeometry(const std::string& encodedGeometry);
        
        static const double COORDINATE_SCALE;
    };
    
}

#endif

//#endif
