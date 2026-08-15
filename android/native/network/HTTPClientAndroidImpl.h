/*
 * Copyright (c) 2016 CartoDB. All rights reserved.
 * Copying and using this code is allowed only according
 * to license terms, as given in https://cartodb.com/terms/
 */

#ifndef _MASSIF_HTTPCLIENTANDROIDIMPL_H_
#define _MASSIF_HTTPCLIENTANDROIDIMPL_H_

#include "network/HTTPClient.h"
#include "components/Exceptions.h"

#include <atomic>
#include <string>

#include <jni.h>

namespace massif {

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
        struct ThrowableClass;

        static std::unique_ptr<URLClass>& GetURLClass();
        static std::unique_ptr<HttpURLConnectionClass>& GetHttpURLConnectionClass();
        static std::unique_ptr<InputStreamClass>& GetInputStreamClass();
        static std::unique_ptr<OutputStreamClass>& GetOutputStreamClass();
        static std::unique_ptr<ThrowableClass>& GetThrowableClass();

        /**
         * Clears the pending Java exception and returns its description, including its cause
         * chain ("java.net.ConnectException: Failed to connect to ..., caused by ..."). Returns
         * an empty string if no exception is pending.
         */
        static std::string DescribePendingException(JNIEnv* jenv);

        /**
         * Builds a NetworkException for a failed JNI call, clearing the pending Java exception and
         * folding its description into the exception message.
         */
        static NetworkException BuildNetworkException(JNIEnv* jenv, const std::string& msg, const std::string& url);

        static const int MAX_EXCEPTION_CAUSE_DEPTH = 4;

        const bool _log;
        std::atomic<int> _timeout;
    };

}

#endif
