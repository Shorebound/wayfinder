#pragma once
#include "../ILogOutput.h"
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <mutex>
#include <raylib.h>

namespace Wayfinder
{
    // Forward declaration
    class SpdLogMessage;

    // Base class for spdlog-based outputs
    class SpdLogOutput : public ILogOutput
    {
    public:
        SpdLogOutput(std::shared_ptr<spdlog::sinks::sink> sink)
            : m_sink(sink) {}

        virtual ~SpdLogOutput() = default;

        // ILogOutput implementation
        void ProcessMessage(const ILogMessage& message) override;
        void Flush() override;
        void SetPattern(const std::string& pattern) override;

        // Get the underlying spdlog sink
        std::shared_ptr<spdlog::sinks::sink> GetSink() const { return m_sink; }

    protected:
        std::shared_ptr<spdlog::sinks::sink> m_sink;
    };

    // Console output implementation
    class SpdConsoleOutput : public SpdLogOutput
    {
    public:
        SpdConsoleOutput();
    };

    // File output implementation
    class SpdFileOutput : public SpdLogOutput
    {
    public:
        SpdFileOutput(const LogFileConfig& config);
    };

    // Raylib output implementation
    template <typename Mutex>
    class raylib_sink : public spdlog::sinks::base_sink<Mutex>
    {
    protected:
        void sink_it_(const spdlog::details::log_msg& msg) override
        {
            // Format the message
            spdlog::memory_buf_t formatted;
            spdlog::sinks::base_sink<Mutex>::formatter_->format(msg, formatted);

            // Create a null-terminated string from the memory buffer
            std::string str(formatted.data(), formatted.size());

            // Convert spdlog levels to Raylib trace levels
            int trace_level;
            switch (msg.level)
            {
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

    class SpdRaylibOutput : public SpdLogOutput
    {
    public:
        SpdRaylibOutput();
    };

    // Factory function declarations - implementations are in SpdLogOutput.cpp

} // namespace Wayfinder
