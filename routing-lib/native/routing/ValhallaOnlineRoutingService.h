#pragma once

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
     * Makes HTTP requests to a remote Valhalla server.
     *
     * Two build modes are supported:
     *
     * 1) Built with ROUTING_WITH_HTTP_CLIENT=ON (recommended):
     *    HTTP transport is handled internally by the built-in C++ HTTP client.
     *    Use the single-argument constructor ValhallaOnlineRoutingService(baseURL).
     *
     * 2) Built without ROUTING_WITH_HTTP_CLIENT:
     *    HTTP transport is provided by a caller-supplied HttpHandler callback.
     *    Use the two-argument constructor ValhallaOnlineRoutingService(baseURL, handler).
     *    The handler is called synchronously on whichever thread invokes the routing
     *    methods, so callers should ensure those methods run on a background thread.
     *
     * Routing results are returned as raw Valhalla JSON strings.
     */
    class ValhallaOnlineRoutingService {
    public:
        /**
         * Synchronous HTTP POST handler type (only used when built WITHOUT
         * ROUTING_WITH_HTTP_CLIENT).
         * @param url       Full URL (base + "/" + endpoint).
         * @param postBody  JSON request body.
         * @return          Response body string. Throw std::exception on error.
         */
        using HttpHandler =
            std::function<std::string(const std::string& url,
                                      const std::string& postBody)>;

        /**
         * Constructor for use when built with ROUTING_WITH_HTTP_CLIENT=ON.
         * No external HTTP handler is required; all HTTP I/O is handled
         * internally by the built-in C++ HTTP client.
         *
         * @param baseURL  Base URL of the Valhalla service,
         *                 e.g. "https://valhalla1.openstreetmap.de".
         */
        explicit ValhallaOnlineRoutingService(const std::string& baseURL);

        /**
         * Constructor that accepts an explicit HTTP POST handler.
         * Works regardless of whether ROUTING_WITH_HTTP_CLIENT is defined.
         *
         * @param baseURL  Base URL of the Valhalla service.
         * @param handler  HTTP POST handler callback.
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
        // std::shared_ptr<RoutingResult> calculateRoute(
        //     const std::shared_ptr<RoutingRequest>& request) const;

        /**
         * Match a GPS trace. POSTs to {baseURL}/trace_attributes,
         * returns raw JSON response.
         */
        // std::shared_ptr<RouteMatchingResult> matchRoute(
        //     const std::shared_ptr<RouteMatchingRequest>& request) const;

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
