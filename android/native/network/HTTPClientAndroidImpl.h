/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _CARTO_HTTPCLIENTANDROIDIMPL_H_
#define _CARTO_HTTPCLIENTANDROIDIMPL_H_

#include "network/HTTPClient.h"

namespace carto {

    class HTTPClient::AndroidImpl : public HTTPClient::Impl {
    public:
        explicit AndroidImpl(bool log);

        virtual void setTimeout(int milliseconds);
        virtual bool makeRequest(const HTTPClient::Request& request, HeadersFunc headersFn, DataFunc dataFn) const;

    private:
        struct URLClass;
        struct HttpURLConnectionClass;
        struct InputStreamClass;
        struct OutputStreamClass;
        
        static std::unique_ptr<URLClass> _URLClass;
        static std::unique_ptr<HttpURLConnectionClass> _HttpURLConnectionClass;
        static std::unique_ptr<InputStreamClass> _InputStreamClass;
        static std::unique_ptr<OutputStreamClass> _OutputStreamClass;
        static std::mutex _Mutex;

        bool _log;
        int _timeout;
    };

}

#endif
