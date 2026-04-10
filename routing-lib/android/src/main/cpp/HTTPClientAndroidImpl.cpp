#include "network/HTTPClientAndroidImpl.h"
#include "exceptions/Exceptions.h"
#include "log/Log.h"

#include <jni.h>
#include <memory>

// JavaVM pointer stored by JNI_OnLoad in ValhallaRoutingJNI.cpp
extern JavaVM* g_routing_jvm;

namespace routing {

    struct HTTPClient::AndroidImpl::URLClass {
        jclass    clazz;
        jmethodID constructor;
        jmethodID openConnection;

        explicit URLClass(JNIEnv* jenv) {
            clazz = static_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("java/net/URL")));
            constructor    = jenv->GetMethodID(clazz, "<init>",         "(Ljava/lang/String;)V");
            openConnection = jenv->GetMethodID(clazz, "openConnection", "()Ljava/net/URLConnection;");
        }
    };

    struct HTTPClient::AndroidImpl::HttpURLConnectionClass {
        jclass    clazz;
        jmethodID setRequestMethod;
        jmethodID setDoInput;
        jmethodID setDoOutput;
        jmethodID setUseCaches;
        jmethodID setAllowUserInteraction;
        jmethodID setInstanceFollowRedirects;
        jmethodID setRequestProperty;
        jmethodID setConnectTimeout;
        jmethodID setReadTimeout;
        jmethodID connect;
        jmethodID disconnect;
        jmethodID getResponseCode;
        jmethodID getHeaderFieldKey;
        jmethodID getHeaderField;
        jmethodID getInputStream;
        jmethodID getOutputStream;
        jmethodID getErrorStream;

        explicit HttpURLConnectionClass(JNIEnv* jenv) {
            clazz = static_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("java/net/HttpURLConnection")));
            setRequestMethod         = jenv->GetMethodID(clazz, "setRequestMethod",         "(Ljava/lang/String;)V");
            setDoInput               = jenv->GetMethodID(clazz, "setDoInput",               "(Z)V");
            setDoOutput              = jenv->GetMethodID(clazz, "setDoOutput",              "(Z)V");
            setUseCaches             = jenv->GetMethodID(clazz, "setUseCaches",             "(Z)V");
            setAllowUserInteraction  = jenv->GetMethodID(clazz, "setAllowUserInteraction",  "(Z)V");
            setInstanceFollowRedirects = jenv->GetMethodID(clazz, "setInstanceFollowRedirects", "(Z)V");
            setRequestProperty       = jenv->GetMethodID(clazz, "setRequestProperty",       "(Ljava/lang/String;Ljava/lang/String;)V");
            setConnectTimeout        = jenv->GetMethodID(clazz, "setConnectTimeout",        "(I)V");
            setReadTimeout           = jenv->GetMethodID(clazz, "setReadTimeout",           "(I)V");
            connect                  = jenv->GetMethodID(clazz, "connect",                  "()V");
            disconnect               = jenv->GetMethodID(clazz, "disconnect",               "()V");
            getResponseCode          = jenv->GetMethodID(clazz, "getResponseCode",          "()I");
            getHeaderFieldKey        = jenv->GetMethodID(clazz, "getHeaderFieldKey",        "(I)Ljava/lang/String;");
            getHeaderField           = jenv->GetMethodID(clazz, "getHeaderField",           "(I)Ljava/lang/String;");
            getInputStream           = jenv->GetMethodID(clazz, "getInputStream",           "()Ljava/io/InputStream;");
            getOutputStream          = jenv->GetMethodID(clazz, "getOutputStream",          "()Ljava/io/OutputStream;");
            getErrorStream           = jenv->GetMethodID(clazz, "getErrorStream",           "()Ljava/io/InputStream;");
        }
    };

    struct HTTPClient::AndroidImpl::InputStreamClass {
        jclass    clazz;
        jmethodID read;
        jmethodID close;

        explicit InputStreamClass(JNIEnv* jenv) {
            clazz = static_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("java/io/InputStream")));
            read  = jenv->GetMethodID(clazz, "read",  "([B)I");
            close = jenv->GetMethodID(clazz, "close", "()V");
        }
    };

    struct HTTPClient::AndroidImpl::OutputStreamClass {
        jclass    clazz;
        jmethodID write;
        jmethodID close;

        explicit OutputStreamClass(JNIEnv* jenv) {
            clazz = static_cast<jclass>(jenv->NewGlobalRef(jenv->FindClass("java/io/OutputStream")));
            write = jenv->GetMethodID(clazz, "write", "([B)V");
            close = jenv->GetMethodID(clazz, "close", "()V");
        }
    };

    static JNIEnv* GetCurrentThreadJNIEnv() {
        if (!g_routing_jvm) {
            throw NetworkException("HTTPClient: JavaVM not initialized");
        }
        JNIEnv* env = nullptr;
        jint status = g_routing_jvm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (status == JNI_OK) {
            return env;
        }
        if (status == JNI_EDETACHED) {
            JavaVMAttachArgs args;
            args.version = JNI_VERSION_1_6;
            args.name    = const_cast<char*>("routing-httpclient");
            args.group   = nullptr;
            if (g_routing_jvm->AttachCurrentThread(&env, &args) != JNI_OK) {
                throw NetworkException("HTTPClient: Failed to attach thread to JVM");
            }
            return env;
        }
        throw NetworkException("HTTPClient: Failed to obtain JNIEnv");
    }

    HTTPClient::AndroidImpl::AndroidImpl(bool log) :
        _log(log),
        _timeout(-1)
    {
    }

    void HTTPClient::AndroidImpl::setTimeout(int milliseconds) {
        _timeout = milliseconds;
    }

    bool HTTPClient::AndroidImpl::makeRequest(const HTTPClient::Request& request,
                                              HeadersFunc headersFn,
                                              DataFunc    dataFn) const {
        JNIEnv* jenv = GetCurrentThreadJNIEnv();

        if (jenv->PushLocalFrame(32) < 0) {
            Log::error("HTTPClient::AndroidImpl::makeRequest: PushLocalFrame failed");
            return false;
        }

        bool success = true;
        try {
            // Create URL
            jobject url = jenv->NewObject(GetURLClass()->clazz, GetURLClass()->constructor,
                                          jenv->NewStringUTF(request.url.c_str()));
            if (jenv->ExceptionCheck()) {
                jenv->ExceptionClear();
                throw NetworkException("Invalid URL", request.url);
            }

            // Open HTTP connection
            jobject conn = jenv->CallObjectMethod(url, GetURLClass()->openConnection);
            if (jenv->ExceptionCheck()) {
                jenv->ExceptionClear();
                throw NetworkException("Unable to open connection", request.url);
            }

            // Configure connection
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setRequestMethod,
                                 jenv->NewStringUTF(request.method.c_str()));
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setDoInput,  (jboolean)true);
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setDoOutput, (jboolean)!request.contentType.empty());
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setUseCaches, (jboolean)false);
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setAllowUserInteraction, (jboolean)false);
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setInstanceFollowRedirects, (jboolean)true);
            int timeout = _timeout.load();
            if (timeout > 0) {
                jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setConnectTimeout, timeout);
                jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setReadTimeout,    timeout);
            }

            // Set request headers
            for (auto it = request.headers.begin(); it != request.headers.end(); ++it) {
                jstring key = jenv->NewStringUTF(it->first.c_str());
                jstring val = jenv->NewStringUTF(it->second.c_str());
                jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->setRequestProperty, key, val);
                jenv->DeleteLocalRef(key);
                jenv->DeleteLocalRef(val);
            }

            // Write request body if Content-Type is set
            if (!request.contentType.empty()) {
                jobject outputStream = jenv->CallObjectMethod(conn, GetHttpURLConnectionClass()->getOutputStream);
                if (jenv->ExceptionCheck()) {
                    jenv->ExceptionClear();
                    throw NetworkException("Unable to get output stream", request.url);
                }
                jbyteArray jbuf = jenv->NewByteArray(static_cast<jsize>(request.body.size()));
                jenv->SetByteArrayRegion(jbuf, 0, static_cast<jsize>(request.body.size()),
                                         reinterpret_cast<const jbyte*>(request.body.data()));
                jenv->CallVoidMethod(outputStream, GetOutputStreamClass()->write, jbuf);
                if (jenv->ExceptionCheck()) {
                    jenv->ExceptionClear();
                    throw NetworkException("Unable to write data", request.url);
                }
                jenv->CallVoidMethod(outputStream, GetOutputStreamClass()->close);
                if (jenv->ExceptionCheck()) {
                    jenv->ExceptionClear();
                    throw NetworkException("Unable to write data", request.url);
                }
            }

            // Connect
            jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->connect);
            if (jenv->ExceptionCheck()) {
                jenv->ExceptionClear();
                throw NetworkException("Unable to connect", request.url);
            }

            // Read response code and headers
            jint responseCode = jenv->CallIntMethod(conn, GetHttpURLConnectionClass()->getResponseCode);
            if (jenv->ExceptionCheck()) {
                jenv->ExceptionClear();
                throw NetworkException("Unable to read response code", request.url);
            }
            std::map<std::string, std::string> headers;
            for (int i = 0; ; ++i) {
                jstring key = static_cast<jstring>(
                    jenv->CallObjectMethod(conn, GetHttpURLConnectionClass()->getHeaderFieldKey, (jint)i));
                if (!key) break;
                jstring val = static_cast<jstring>(
                    jenv->CallObjectMethod(conn, GetHttpURLConnectionClass()->getHeaderField, (jint)i));
                const char* keyStr = jenv->GetStringUTFChars(key, NULL);
                const char* valStr = jenv->GetStringUTFChars(val, NULL);
                headers[keyStr] = valStr;
                jenv->ReleaseStringUTFChars(val, valStr);
                jenv->ReleaseStringUTFChars(key, keyStr);
                jenv->DeleteLocalRef(val);
                jenv->DeleteLocalRef(key);
            }

            bool cancel = false;
            if (!headersFn(responseCode, headers)) {
                cancel = true;
            }

            // Get input stream
            jobject inputStream = jenv->CallObjectMethod(conn, GetHttpURLConnectionClass()->getInputStream);
            if (jenv->ExceptionCheck()) {
                jenv->ExceptionClear();
                inputStream = jenv->CallObjectMethod(conn, GetHttpURLConnectionClass()->getErrorStream);
                if (jenv->ExceptionCheck()) {
                    jenv->ExceptionClear();
                    throw NetworkException("Unable to get input stream", request.url);
                }
            }

            try {
                jbyte    buf[4096];
                jbyteArray jbuf = jenv->NewByteArray(sizeof(buf));
                while (!cancel) {
                    jint n = jenv->CallIntMethod(inputStream, GetInputStreamClass()->read, jbuf);
                    if (jenv->ExceptionCheck()) {
                        jenv->ExceptionClear();
                        throw NetworkException("Unable to read data", request.url);
                    }
                    if (n < 0) break;
                    jenv->GetByteArrayRegion(jbuf, 0, n, buf);
                    if (!dataFn(reinterpret_cast<const unsigned char*>(buf), n)) {
                        cancel = true;
                    }
                }
            } catch (...) {
                jenv->CallVoidMethod(inputStream, GetInputStreamClass()->close);
                if (jenv->ExceptionCheck()) jenv->ExceptionClear();
                jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->disconnect);
                jenv->PopLocalFrame(nullptr);
                throw;
            }

            jenv->CallVoidMethod(inputStream, GetInputStreamClass()->close);
            if (jenv->ExceptionCheck()) jenv->ExceptionClear();

            if (cancel) {
                jenv->CallVoidMethod(conn, GetHttpURLConnectionClass()->disconnect);
                jenv->PopLocalFrame(nullptr);
                return false;
            }
        } catch (...) {
            jenv->PopLocalFrame(nullptr);
            throw;
        }

        jenv->PopLocalFrame(nullptr);
        return success;
    }

    std::unique_ptr<HTTPClient::AndroidImpl::URLClass>& HTTPClient::AndroidImpl::GetURLClass() {
        static std::unique_ptr<URLClass> cls(new URLClass(GetCurrentThreadJNIEnv()));
        return cls;
    }

    std::unique_ptr<HTTPClient::AndroidImpl::HttpURLConnectionClass>& HTTPClient::AndroidImpl::GetHttpURLConnectionClass() {
        static std::unique_ptr<HttpURLConnectionClass> cls(new HttpURLConnectionClass(GetCurrentThreadJNIEnv()));
        return cls;
    }

    std::unique_ptr<HTTPClient::AndroidImpl::InputStreamClass>& HTTPClient::AndroidImpl::GetInputStreamClass() {
        static std::unique_ptr<InputStreamClass> cls(new InputStreamClass(GetCurrentThreadJNIEnv()));
        return cls;
    }

    std::unique_ptr<HTTPClient::AndroidImpl::OutputStreamClass>& HTTPClient::AndroidImpl::GetOutputStreamClass() {
        static std::unique_ptr<OutputStreamClass> cls(new OutputStreamClass(GetCurrentThreadJNIEnv()));
        return cls;
    }

} // namespace routing
