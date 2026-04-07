#include "Log.h"

#include <cstdio>
#include <string>

#ifdef __ANDROID__
#  include <android/log.h>
#endif

namespace routing {

    static void defaultOutput(LogLevel level, const char* message) {
#ifdef __ANDROID__
        static const android_LogPriority prio[] = {
            ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN,
            ANDROID_LOG_ERROR, ANDROID_LOG_FATAL
        };
        __android_log_print(prio[static_cast<int>(level)], "routing-lib", "%s", message);
#else
        static const char* prefix[] = { "[DEBUG]", "[INFO]", "[WARN]", "[ERROR]", "[FATAL]" };
        std::fprintf(level >= LogLevel::Error ? stderr : stdout,
                     "%s %s\n", prefix[static_cast<int>(level)], message);
#endif
    }

    void Log::setCallback(Callback cb) {
        std::lock_guard<std::mutex> lk(_mutex);
        _callback = std::move(cb);
    }

    Log::Callback Log::getCallback() {
        std::lock_guard<std::mutex> lk(_mutex);
        return _callback;
    }

    void Log::setMinLevel(LogLevel level) {
        std::lock_guard<std::mutex> lk(_mutex);
        _minLevel = level;
    }

    LogLevel Log::getMinLevel() {
        std::lock_guard<std::mutex> lk(_mutex);
        return _minLevel;
    }

    void Log::fatal(const char* message) { emit(LogLevel::Fatal, message); }
    void Log::error(const char* message) { emit(LogLevel::Error, message); }
    void Log::warn(const char* message)  { emit(LogLevel::Warn,  message); }
    void Log::info(const char* message)  { emit(LogLevel::Info,  message); }
    void Log::debug(const char* message) { emit(LogLevel::Debug, message); }

    void Log::emit(LogLevel level, const std::string& message) {
        if (level < _minLevel) return;
        Callback cb;
        {
            std::lock_guard<std::mutex> lk(_mutex);
            cb = _callback;
        }
        if (cb) {
            cb(level, message.c_str());
        } else {
            defaultOutput(level, message.c_str());
        }
    }

    Log::Callback Log::_callback;
    LogLevel      Log::_minLevel = LogLevel::Info;
    std::mutex    Log::_mutex;

} // namespace routing
