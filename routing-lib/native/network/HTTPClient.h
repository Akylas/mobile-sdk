#pragma once

#include <string>

namespace routing {

    /**
     * Minimal synchronous HTTP client for routing-lib network requests.
     *
     * Only available when the library is built with ROUTING_WITH_HTTP_CLIENT=ON.
     * Sends a JSON POST request and returns the response body.
     * Throws std::runtime_error on network or HTTP errors.
     */
    class HTTPClient {
    public:
        HTTPClient();

        /**
         * Sends a synchronous HTTP POST with a JSON body.
         * @param url      Full request URL.
         * @param jsonBody JSON request body.
         * @return         Response body string.
         * @throws std::runtime_error on network or HTTP error.
         */
        std::string post(const std::string& url, const std::string& jsonBody) const;
    };

} // namespace routing
