#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <cstdio>

namespace routing {

    /**
     * Log levels used by the routing library.
     */
    enum class LogLevel {
        Debug = 0,
        Info  = 1,
        Warn  = 2,
        Error = 3,
        Fatal = 4
    };

    /**
     * Simple callback-based logger. No external dependencies.
     * Users can install a custom callback to intercept log messages.
     */
    class Log {
    public:
        using Callback = std::function<void(LogLevel level, const char* message)>;

        static void setCallback(Callback cb);
        static Callback getCallback();

        static void setMinLevel(LogLevel level);
        static LogLevel getMinLevel();

        static void fatal(const char* message);
        static void error(const char* message);
        static void warn(const char* message);
        static void info(const char* message);
        static void debug(const char* message);

        template <typename... Args>
        static void fatalf(const char* fmt, const Args&... args) { log(LogLevel::Fatal, fmt, args...); }
        template <typename... Args>
        static void errorf(const char* fmt, const Args&... args) { log(LogLevel::Error, fmt, args...); }
        template <typename... Args>
        static void warnf(const char* fmt, const Args&... args)  { log(LogLevel::Warn,  fmt, args...); }
        template <typename... Args>
        static void infof(const char* fmt, const Args&... args)  { log(LogLevel::Info,  fmt, args...); }
        template <typename... Args>
        static void debugf(const char* fmt, const Args&... args) { log(LogLevel::Debug, fmt, args...); }

    private:
        Log() = delete;

        static void emit(LogLevel level, const std::string& message);

        template <typename... Args>
        static void log(LogLevel level, const char* fmt, const Args&... args) {
            if (level < _minLevel) return;
            // snprintf-based formatting to avoid boost/tinyformat dependency
            char buf[4096];
#if defined(_MSC_VER)
            _snprintf_s(buf, sizeof(buf), _TRUNCATE, fmt, args...);
#else
            std::snprintf(buf, sizeof(buf), fmt, args...);
#endif
            emit(level, buf);
        }

        static Callback  _callback;
        static LogLevel  _minLevel;
        static std::mutex _mutex;
    };

} // namespace routing
