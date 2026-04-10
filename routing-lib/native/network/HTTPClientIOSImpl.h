#pragma once

#include "network/HTTPClient.h"

#include <atomic>

namespace routing {

    class HTTPClient::IOSImpl : public HTTPClient::Impl {
    public:
        explicit IOSImpl(bool log);
        virtual ~IOSImpl();

        virtual void setTimeout(int milliseconds);
        virtual bool makeRequest(const HTTPClient::Request& request,
                                 HeadersFunc headersFn,
                                 DataFunc    dataFn) const;

    private:
        const bool       _log;
        std::atomic<int> _timeout;
    };

} // namespace routing
