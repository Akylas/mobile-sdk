#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace routing {

    /**
     * Synchronous HTTP client for routing-lib network requests.
     *
     * Mirrors the internal structure of the main SDK HTTPClient (Request / Response /
     * Impl) but without Boost, BinaryData, or streaming methods that routing-lib
     * does not need.  Only available when built with ROUTING_WITH_HTTP_CLIENT=ON.
     */
    class HTTPClient {
    public:
        explicit HTTPClient(bool log = false);

        /**
         * Send a synchronous HTTP POST with a JSON body.
         * @param url      Full request URL.
         * @param jsonBody JSON request body.
         * @return         Response body string.
         * @throws std::runtime_error on network or HTTP error.
         */
        std::string post(const std::string& url, const std::string& jsonBody) const;

    private:
        struct HeaderLess {
            bool operator()(const std::string& h1, const std::string& h2) const {
                return std::lexicographical_compare(h1.begin(), h1.end(),
                                                    h2.begin(), h2.end(),
                                                    [](char a, char b) {
                    auto lc = [](char c) { return c >= 'A' && c <= 'Z' ? char(c - 'A' + 'a') : c; };
                    return lc(a) < lc(b);
                });
            }
        };

        struct Request {
            std::string url;
            std::string method;
            std::map<std::string, std::string, HeaderLess> headers;
            std::string contentType;
            std::vector<unsigned char> body;

            explicit Request(const std::string& method_, const std::string& url_)
                : url(url_), method(method_) {}
        };

        struct Response {
            int statusCode = -1;
            std::map<std::string, std::string, HeaderLess> headers;
            std::vector<unsigned char> body;
        };

        class Impl {
        public:
            typedef std::function<bool(int, const std::map<std::string, std::string>&)> HeadersFunc;
            typedef std::function<bool(const unsigned char*, std::size_t)>              DataFunc;

            virtual ~Impl();

            virtual void setTimeout(int milliseconds) = 0;
            virtual bool makeRequest(const HTTPClient::Request& request,
                                     HeadersFunc headersFn,
                                     DataFunc    dataFn) const = 0;
        };

        class AndroidImpl;
        class IOSImpl;

        int makeRequest(Request request, Response& response) const;

        bool                  _log;
        std::unique_ptr<Impl> _impl;
    };

} // namespace routing
