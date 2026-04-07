#pragma once

#include "../core/Variant.h"
#include "../routing/RoutingRequest.h"
#include "../routing/RoutingResult.h"
#include "../routing/RouteMatchingRequest.h"
#include "../routing/RouteMatchingResult.h"

#include <functional>
#include <memory>
#include <mutex>
#include <string>

namespace routing {

    /**
     * Online Valhalla routing service.
     *
     * Makes HTTP requests to a remote Valhalla server. All HTTP transport
     * is handled by the user-supplied HttpHandler callback, keeping this
     * class free of platform HTTP dependencies.
     *
     * The handler is called synchronously on whichever thread invokes
     * calculateRoute / matchRoute / callRaw, so callers should ensure those
     * methods run on a background thread.
     *
     * Routing results are returned as raw Valhalla JSON strings.
     */
    class ValhallaOnlineRoutingService {
    public:
        /**
         * Synchronous HTTP POST handler.
         * @param url       Full URL (base + "/" + endpoint).
         * @param postBody  JSON request body.
         * @return          Response body string. Throw std::exception on error.
         */
        using HttpHandler =
            std::function<std::string(const std::string& url,
                                      const std::string& postBody)>;

        /**
         * @param baseURL    Base URL of the Valhalla service,
         *                   e.g. "https://valhalla1.openstreetmap.de".
         * @param handler    HTTP POST handler.
         */
        ValhallaOnlineRoutingService(const std::string& baseURL,
                                     HttpHandler handler);
        virtual ~ValhallaOnlineRoutingService();

        // ----------------------------------------------------------------
        // Service URL
        // ----------------------------------------------------------------

        std::string getBaseURL() const;
        void setBaseURL(const std::string& url);

        // ----------------------------------------------------------------
        // Profile
        // ----------------------------------------------------------------

        std::string getProfile() const;
        void setProfile(const std::string& profile);

        // ----------------------------------------------------------------
        // Routing API — return raw Valhalla JSON strings
        // ----------------------------------------------------------------

        /**
         * Calculate a route. Builds the JSON request from the RoutingRequest,
         * POSTs to {baseURL}/route, returns raw JSON response.
         */
        std::shared_ptr<RoutingResult> calculateRoute(
            const std::shared_ptr<RoutingRequest>& request) const;

        /**
         * Match a GPS trace. POSTs to {baseURL}/trace_attributes,
         * returns raw JSON response.
         */
        std::shared_ptr<RouteMatchingResult> matchRoute(
            const std::shared_ptr<RouteMatchingRequest>& request) const;

        /**
         * Call any Valhalla API endpoint directly.
         * @param endpoint  e.g. "route", "trace_attributes", "isochrone", …
         * @param jsonBody  Pre-built Valhalla JSON request.
         * @return          Raw JSON response string.
         */
        std::string callRaw(const std::string& endpoint,
                            const std::string& jsonBody) const;

    private:
        std::string buildURL(const std::string& endpoint) const;

        mutable std::mutex _mutex;
        std::string   _baseURL;
        std::string   _profile;
        HttpHandler   _handler;
    };

} // namespace routing
