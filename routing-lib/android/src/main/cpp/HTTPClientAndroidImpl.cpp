//
// HTTPClientAndroidImpl.cpp
// Android JNI implementation of routing::HTTPClient using java.net.HttpURLConnection.
//

#include "../../native/network/HTTPClient.h"

#include <jni.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

// JavaVM pointer stored by JNI_OnLoad in ValhallaRoutingJNI.cpp
extern JavaVM* g_routing_jvm;

// Returns a JNIEnv for the current thread, attaching it to the JVM if necessary.
static JNIEnv* getRoutingJNIEnv() {
    if (!g_routing_jvm) {
        throw std::runtime_error("HTTPClient: JavaVM not initialized (JNI_OnLoad not called?)");
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
            throw std::runtime_error("HTTPClient: Failed to attach current thread to JVM");
        }
        return env;
    }
    throw std::runtime_error("HTTPClient: Failed to obtain JNIEnv");
}

namespace routing {

HTTPClient::HTTPClient() {}

std::string HTTPClient::post(const std::string& url, const std::string& jsonBody) const {
    JNIEnv* jenv = getRoutingJNIEnv();

    // Push a local frame to automatically release all local refs on exit.
    if (jenv->PushLocalFrame(48) < 0) {
        throw std::runtime_error("HTTPClient: Failed to push local JNI frame");
    }
    struct FrameGuard {
        JNIEnv* jenv;
        ~FrameGuard() { jenv->PopLocalFrame(nullptr); }
    } guard{jenv};

    auto clearAndThrow = [&](const std::string& msg) {
        if (jenv->ExceptionCheck()) jenv->ExceptionClear();
        throw std::runtime_error(msg);
    };

    // ---- java.net.URL ----
    jclass clsURL = jenv->FindClass("java/net/URL");
    jmethodID midURLInit       = jenv->GetMethodID(clsURL, "<init>", "(Ljava/lang/String;)V");
    jmethodID midOpenConn      = jenv->GetMethodID(clsURL, "openConnection", "()Ljava/net/URLConnection;");

    jstring jurl = jenv->NewStringUTF(url.c_str());
    jobject urlObj = jenv->NewObject(clsURL, midURLInit, jurl);
    if (jenv->ExceptionCheck() || !urlObj) clearAndThrow("HTTPClient: Invalid URL: " + url);

    jobject connObj = jenv->CallObjectMethod(urlObj, midOpenConn);
    if (jenv->ExceptionCheck() || !connObj) clearAndThrow("HTTPClient: Cannot open connection to: " + url);

    // ---- java.net.HttpURLConnection ----
    jclass clsConn = jenv->FindClass("java/net/HttpURLConnection");
    jmethodID midSetMethod   = jenv->GetMethodID(clsConn, "setRequestMethod", "(Ljava/lang/String;)V");
    jmethodID midSetDoInput  = jenv->GetMethodID(clsConn, "setDoInput",  "(Z)V");
    jmethodID midSetDoOutput = jenv->GetMethodID(clsConn, "setDoOutput", "(Z)V");
    jmethodID midSetCaches   = jenv->GetMethodID(clsConn, "setUseCaches", "(Z)V");
    jmethodID midSetProp     = jenv->GetMethodID(clsConn, "setRequestProperty", "(Ljava/lang/String;Ljava/lang/String;)V");
    jmethodID midGetCode     = jenv->GetMethodID(clsConn, "getResponseCode", "()I");
    jmethodID midGetInput    = jenv->GetMethodID(clsConn, "getInputStream", "()Ljava/io/InputStream;");
    jmethodID midGetError    = jenv->GetMethodID(clsConn, "getErrorStream", "()Ljava/io/InputStream;");
    jmethodID midGetOutput   = jenv->GetMethodID(clsConn, "getOutputStream", "()Ljava/io/OutputStream;");
    jmethodID midDisconnect  = jenv->GetMethodID(clsConn, "disconnect", "()V");

    jenv->CallVoidMethod(connObj, midSetMethod, jenv->NewStringUTF("POST"));
    jenv->CallVoidMethod(connObj, midSetDoInput,  (jboolean)JNI_TRUE);
    jenv->CallVoidMethod(connObj, midSetDoOutput, (jboolean)JNI_TRUE);
    jenv->CallVoidMethod(connObj, midSetCaches,   (jboolean)JNI_FALSE);
    jenv->CallVoidMethod(connObj, midSetProp,
        jenv->NewStringUTF("Content-Type"),
        jenv->NewStringUTF("application/json; charset=utf-8"));
    jenv->CallVoidMethod(connObj, midSetProp,
        jenv->NewStringUTF("Accept"),
        jenv->NewStringUTF("application/json"));

    // ---- Write request body ----
    jobject outputStream = jenv->CallObjectMethod(connObj, midGetOutput);
    if (jenv->ExceptionCheck() || !outputStream) {
        jenv->ExceptionClear();
        jenv->CallVoidMethod(connObj, midDisconnect);
        throw std::runtime_error("HTTPClient: Cannot get output stream for: " + url);
    }
    jclass clsOS = jenv->FindClass("java/io/OutputStream");
    jmethodID midOSWrite = jenv->GetMethodID(clsOS, "write", "([B)V");
    jmethodID midOSClose = jenv->GetMethodID(clsOS, "close", "()V");

    jbyteArray jbody = jenv->NewByteArray(static_cast<jsize>(jsonBody.size()));
    jenv->SetByteArrayRegion(jbody, 0, static_cast<jsize>(jsonBody.size()),
                             reinterpret_cast<const jbyte*>(jsonBody.data()));
    jenv->CallVoidMethod(outputStream, midOSWrite, jbody);
    if (jenv->ExceptionCheck()) {
        jenv->ExceptionClear();
        jenv->CallVoidMethod(connObj, midDisconnect);
        throw std::runtime_error("HTTPClient: Cannot write request body to: " + url);
    }
    jenv->CallVoidMethod(outputStream, midOSClose);
    if (jenv->ExceptionCheck()) jenv->ExceptionClear();

    // ---- Read response code ----
    jint responseCode = jenv->CallIntMethod(connObj, midGetCode);
    if (jenv->ExceptionCheck()) {
        jenv->ExceptionClear();
        jenv->CallVoidMethod(connObj, midDisconnect);
        throw std::runtime_error("HTTPClient: Cannot get response code from: " + url);
    }

    // ---- Read response body ----
    jobject inputStream = nullptr;
    if (responseCode >= 200 && responseCode < 300) {
        inputStream = jenv->CallObjectMethod(connObj, midGetInput);
        if (jenv->ExceptionCheck()) {
            jenv->ExceptionClear();
            inputStream = nullptr;
        }
    }
    if (!inputStream) {
        inputStream = jenv->CallObjectMethod(connObj, midGetError);
        if (jenv->ExceptionCheck()) {
            jenv->ExceptionClear();
            jenv->CallVoidMethod(connObj, midDisconnect);
            throw std::runtime_error("HTTPClient: HTTP " + std::to_string(responseCode) +
                                     " and no error stream from: " + url);
        }
    }

    jclass clsIS    = jenv->FindClass("java/io/InputStream");
    jmethodID midRead  = jenv->GetMethodID(clsIS, "read", "([B)I");
    jmethodID midClose = jenv->GetMethodID(clsIS, "close", "()V");

    std::vector<char> responseData;
    jbyteArray buf = jenv->NewByteArray(4096);
    while (true) {
        jint n = jenv->CallIntMethod(inputStream, midRead, buf);
        if (jenv->ExceptionCheck()) { jenv->ExceptionClear(); break; }
        if (n < 0) break;
        jbyte tmp[4096];
        jenv->GetByteArrayRegion(buf, 0, n, tmp);
        responseData.insert(responseData.end(),
                            reinterpret_cast<const char*>(tmp),
                            reinterpret_cast<const char*>(tmp) + n);
    }
    jenv->CallVoidMethod(inputStream, midClose);
    if (jenv->ExceptionCheck()) jenv->ExceptionClear();

    jenv->CallVoidMethod(connObj, midDisconnect);

    std::string responseStr(responseData.begin(), responseData.end());

    if (responseCode < 200 || responseCode >= 300) {
        throw std::runtime_error("HTTPClient: HTTP " + std::to_string(responseCode) +
                                 " from: " + url + " — " + responseStr);
    }

    return responseStr;
}

} // namespace routing
