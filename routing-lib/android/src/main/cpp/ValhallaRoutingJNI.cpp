//
// ValhallaRoutingJNI.cpp
//
// JNI bridge between com.akylas.routing.ValhallaRoutingService (Kotlin)
// and routing::ValhallaRoutingService (C++).
//

#include <jni.h>

#include "../../../../native/routing/ValhallaRoutingService.h"
#include "../../../../native/datasource/MBTilesDataSource.h"

#include <string>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Global JavaVM pointer — stored in JNI_OnLoad and used by the built-in
// C++ HTTP client (HTTPClientAndroidImpl.cpp) when ROUTING_WITH_HTTP_CLIENT
// is defined.
// ---------------------------------------------------------------------------
#ifdef ROUTING_WITH_HTTP_CLIENT
JavaVM* g_routing_jvm = nullptr;
#  include "../../../../native/network/HTTPClient.h"
#endif

// ---------------------------------------------------------------------------
// Helper: convert jstring to std::string
// ---------------------------------------------------------------------------
static std::string jstringToStr(JNIEnv* env, jstring js) {
    if (!js) return {};
    const char* chars = env->GetStringUTFChars(js, nullptr);
    std::string s(chars);
    env->ReleaseStringUTFChars(js, chars);
    return s;
}

// ---------------------------------------------------------------------------
// Helper: convert std::string to jstring
// ---------------------------------------------------------------------------
static jstring strToJString(JNIEnv* env, const std::string& s) {
    return env->NewStringUTF(s.c_str());
}

// ---------------------------------------------------------------------------
// Helper: throw a Java RuntimeException
// ---------------------------------------------------------------------------
static void throwRuntimeException(JNIEnv* env, const char* msg) {
    jclass cls = env->FindClass("java/lang/RuntimeException");
    if (cls) env->ThrowNew(cls, msg);
}

// ---------------------------------------------------------------------------
// Pointer cast helpers
// ---------------------------------------------------------------------------
static routing::ValhallaRoutingService* toService(jlong ptr) {
    return reinterpret_cast<routing::ValhallaRoutingService*>(ptr);
}

extern "C" {

// ---------------------------------------------------------------------------
// JNI_OnLoad — store the JavaVM pointer for the built-in HTTP client.
// ---------------------------------------------------------------------------
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* /*reserved*/) {
#ifdef ROUTING_WITH_HTTP_CLIENT
    g_routing_jvm = vm;
#endif
    return JNI_VERSION_1_6;
}

// ---------------------------------------------------------------------------
// com.akylas.routing.ValhallaRoutingService native methods
// ---------------------------------------------------------------------------

JNIEXPORT jlong JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeCreate(JNIEnv*, jobject) {
    auto* svc = new routing::ValhallaRoutingService();
    return reinterpret_cast<jlong>(svc);
}

JNIEXPORT void JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeDestroy(JNIEnv*, jobject, jlong ptr) {
    delete toService(ptr);
}

JNIEXPORT void JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeAddMBTilesPath(
        JNIEnv* env, jobject, jlong ptr, jstring jpath) {
    try {
        std::string path = jstringToStr(env, jpath);
        auto src = std::make_shared<routing::MBTilesDataSource>(path);
        toService(ptr)->addSource(src);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
    }
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeGetProfile(
        JNIEnv* env, jobject, jlong ptr) {
    return strToJString(env, toService(ptr)->getProfile());
}

JNIEXPORT void JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeSetProfile(
        JNIEnv* env, jobject, jlong ptr, jstring jprofile) {
    toService(ptr)->setProfile(jstringToStr(env, jprofile));
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeGetConfigParam(
        JNIEnv* env, jobject, jlong ptr, jstring jkey) {
    routing::Variant val = toService(ptr)->getConfigurationParameter(jstringToStr(env, jkey));
    if (val.getType() == routing::VariantType::VARIANT_TYPE_NULL) return nullptr;
    return strToJString(env, val.toJSON());
}

JNIEXPORT void JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeSetConfigParam(
        JNIEnv* env, jobject, jlong ptr, jstring jkey, jstring jvalue) {
    try {
        routing::Variant val = routing::Variant::FromJSON(jstringToStr(env, jvalue));
        toService(ptr)->setConfigurationParameter(jstringToStr(env, jkey), val);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
    }
}

JNIEXPORT void JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeAddLocale(
        JNIEnv* env, jobject, jlong ptr, jstring jkey, jstring jjson) {
    toService(ptr)->addLocale(jstringToStr(env, jkey), jstringToStr(env, jjson));
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeCalculateRoute(
        JNIEnv* env, jobject, jlong ptr, jstring jrequestJSON) {
    try {
        // The Kotlin layer serializes the request to JSON. We use callRaw("route", ...) here
        // because the Kotlin RoutingRequest builds a valid Valhalla route request JSON directly.
        // The profile is embedded in the JSON by the Kotlin builder.
        std::string result = toService(ptr)->callRaw("route", jstringToStr(env, jrequestJSON));
        return strToJString(env, result);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeMatchRoute(
        JNIEnv* env, jobject, jlong ptr, jstring jrequestJSON) {
    try {
        std::string result = toService(ptr)->callRaw("trace_attributes", jstringToStr(env, jrequestJSON));
        return strToJString(env, result);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaRoutingService_nativeCallRaw(
        JNIEnv* env, jobject, jlong ptr, jstring jendpoint, jstring jbody) {
    try {
        std::string result = toService(ptr)->callRaw(
            jstringToStr(env, jendpoint),
            jstringToStr(env, jbody));
        return strToJString(env, result);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
        return nullptr;
    }
}

JNIEXPORT jstring JNICALL
Java_com_akylas_routing_ValhallaOnlineRoutingService_nativeHttpPost(
        JNIEnv* env, jobject /*thiz*/, jstring jurl, jstring jbody) {
#ifdef ROUTING_WITH_HTTP_CLIENT
    try {
        routing::HTTPClient client;
        std::string result = client.post(jstringToStr(env, jurl), jstringToStr(env, jbody));
        return strToJString(env, result);
    } catch (const std::exception& ex) {
        throwRuntimeException(env, ex.what());
        return nullptr;
    }
#else
    (void)jurl; (void)jbody;
    throwRuntimeException(env, "Built without ROUTING_WITH_HTTP_CLIENT: supply an HttpPostHandler");
    return nullptr;
#endif
}

} // extern "C"
