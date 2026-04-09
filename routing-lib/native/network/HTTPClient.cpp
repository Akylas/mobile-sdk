#include "HTTPClient.h"
#include "exceptions/Exceptions.h"
#include "log/Log.h"

#include <regex>
#include <limits>
#include <string>

#if defined(__APPLE__)
#define ROUTING_HTTP_SOCKET_IMPL IOSImpl
#include "network/HTTPClientIOSImpl.h"
#elif defined(__ANDROID__)
#define ROUTING_HTTP_SOCKET_IMPL AndroidImpl
#include "network/HTTPClientAndroidImpl.h"
#endif

namespace routing {

    HTTPClient::HTTPClient(bool log) :
        _log(log),
        _impl(std::make_unique<ROUTING_HTTP_SOCKET_IMPL>(log))
    {
    }

    std::string HTTPClient::post(const std::string& url, const std::string& jsonBody) const {
        Request request("POST", url);
        request.contentType = "application/json";
        request.headers["Content-Type"] = "application/json";
        request.headers["Accept"] = "application/json";
        request.body.assign(jsonBody.begin(), jsonBody.end());

        Response response;
        int code = makeRequest(std::move(request), response);
        if (code != 0) {
            throw NetworkException("HTTP request failed", url);
        }
        return std::string(response.body.begin(), response.body.end());
    }

    int HTTPClient::makeRequest(Request request, Response& response) const {
        std::uint64_t contentOffset = 0;
        std::uint64_t contentLength = std::numeric_limits<std::uint64_t>::max();

        auto headersFn = [&](int statusCode, const std::map<std::string, std::string>& headers) {
            response.statusCode = statusCode;
            response.headers.insert(headers.begin(), headers.end());

            // Read Content-Length
            auto it = response.headers.find("Content-Length");
            if (it != response.headers.end()) {
                try { contentLength = std::stoull(it->second); } catch (...) {}
            }

            return true;
        };

        auto dataFn = [&](const unsigned char* data, std::size_t size) {
            response.body.insert(response.body.end(), data, data + size);
            return true;
        };

        if (!_impl->makeRequest(request, headersFn, dataFn)) {
            return -1;
        }

        if (response.statusCode >= 300 && response.statusCode < 400) {
            auto it = response.headers.find("Location");
            if (it != response.headers.end()) {
                std::string location = it->second;
                if (_log) {
                    Log::infof("HTTPClient::makeRequest: Redirection from URL: %s to URL: %s",
                               request.url.c_str(), location.c_str());
                }
                Request redirectedRequest(request);
                redirectedRequest.url = location;
                response = Response();
                return makeRequest(std::move(redirectedRequest), response);
            }
        }

        if (response.statusCode < 200 || response.statusCode >= 300) {
            if (_log) {
                Log::errorf("HTTPClient::makeRequest: Bad status code: %d, URL: %s",
                            response.statusCode, request.url.c_str());
            }
            return response.statusCode;
        }

        return 0;
    }

    HTTPClient::Impl::~Impl() {
    }

} // namespace routing
