#pragma once
#include "Core.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/base_sink.h>
#include <memory>
#include <unordered_map>
#include <string>

namespace Wayfinder {

// Custom sink that forwards to Raylib's TraceLog
template<typename Mutex>
class raylib_sink : public spdlog::sinks::base_sink<Mutex>
{
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        // Format the message
        spdlog::memory_buf_t formatted;
        spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);
        
        // Create a null-terminated string from the memory buffer
        std::string str(formatted.data(), formatted.size());
        
        // Convert spdlog levels to Raylib trace levels
        int trace_level;
        switch (msg.level) {
            case spdlog::level::trace:
                trace_level = LOG_TRACE;
                break;
            case spdlog::level::debug:
                trace_level = LOG_DEBUG;
                break;
            case spdlog::level::info:
                trace_level = LOG_INFO;
                break;
            case spdlog::level::warn:
                trace_level = LOG_WARNING;
                break;
            case spdlog::level::err:
                trace_level = LOG_ERROR;
                break;
            case spdlog::level::critical:
                trace_level = LOG_ERROR;
                break;
            default:
                trace_level = LOG_INFO;
        }
        
        // Forward to Raylib's TraceLog using the null-terminated string
        TraceLog(trace_level, "%s", str.c_str());
    }

    void flush_() override {}
};

// Type aliases for convenience
using raylib_sink_mt = raylib_sink<std::mutex>;
using raylib_sink_st = raylib_sink<spdlog::details::null_mutex>;

    enum class LogVerbosity
    {
        Fatal,      // The program cannot continue. Logs and crashes.
        Error,      // The feature is disabled. Logs and continues.
        Warning,    // Something is wrong but the feature can still work.
        Info,        // Standard log messages
        Verbose,    // Detailed information for debugging
        VeryVerbose // Very detailed information for debugging specific issues
    };

    class WAYFINDER_API LogCategory
    {
    public:
        LogCategory(const std::string &name, LogVerbosity defaultVerbosity = LogVerbosity::Info);

        const std::string &GetName() const { return m_name; }
        std::shared_ptr<spdlog::logger>& GetLogger() { return m_logger; }
        LogVerbosity GetVerbosity() const { return m_verbosity; }
        void SetVerbosity(LogVerbosity level);

    private:
        std::string m_name;
        std::shared_ptr<spdlog::logger> m_logger;
        LogVerbosity m_verbosity;
    };

    class WAYFINDER_API Log
    {
    public:
        static void Init();
        static void Shutdown();

        static LogCategory& CreateCategory(const std::string &name, LogVerbosity defaultVerbosity = LogVerbosity::Info);
        static LogCategory& GetCategory(const std::string &name);

        // Global verbosity control
        static void SetGlobalVerbosity(LogVerbosity level);

    private:
        static std::unordered_map<std::string, std::unique_ptr<LogCategory>> s_categories;
        static LogVerbosity s_globalVerbosity;
    };

    // Declare common categories
    extern WAYFINDER_API LogCategory& LogEngine;
    extern WAYFINDER_API LogCategory& LogRenderer;
    extern WAYFINDER_API LogCategory& LogInput;
    extern WAYFINDER_API LogCategory& LogAudio;
    extern WAYFINDER_API LogCategory& LogPhysics;
    extern WAYFINDER_API LogCategory& LogGame;

} // namespace Wayfinder

// Logging macros
#define WAYFINDER_LOG(category, verbosity, ...)      \
    if (verbosity <= category.GetVerbosity()) \
    category.GetLogger()->log(spdlog::level::info, __VA_ARGS__)

#define WAYFINDER_VERBOSE(category, ...) WAYFINDER_LOG(category, LogVerbosity::Verbose, __VA_ARGS__)
#define WAYFINDER_INFO(category, ...) WAYFINDER_LOG(category, LogVerbosity::Info, __VA_ARGS__)
#define WAYFINDER_WARNING(category, ...) WAYFINDER_LOG(category, LogVerbosity::Warning, __VA_ARGS__)
#define WAYFINDER_ERROR(category, ...) WAYFINDER_LOG(category, LogVerbosity::Error, __VA_ARGS__)
#define WAYFINDER_FATAL(category, ...) WAYFINDER_LOG(category, LogVerbosity::Fatal, __VA_ARGS__)




