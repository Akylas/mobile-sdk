#pragma once

#include "network/HTTPClient.h"

#include <atomic>

namespace routing {

    class HTTPClient::AndroidImpl : public HTTPClient::Impl {
    public:
        explicit AndroidImpl(bool log);

        virtual void setTimeout(int milliseconds);
        virtual bool makeRequest(const HTTPClient::Request& request,
                                 HeadersFunc headersFn,
                                 DataFunc    dataFn) const;

    private:
        struct URLClass;
        struct HttpURLConnectionClass;
        struct InputStreamClass;
        struct OutputStreamClass;

        static std::unique_ptr<URLClass>&                GetURLClass();
        static std::unique_ptr<HttpURLConnectionClass>&  GetHttpURLConnectionClass();
        static std::unique_ptr<InputStreamClass>&        GetInputStreamClass();
        static std::unique_ptr<OutputStreamClass>&       GetOutputStreamClass();

        const bool       _log;
        std::atomic<int> _timeout;
    };

} // namespace routing
