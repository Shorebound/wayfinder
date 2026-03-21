#pragma once
#include "core/logging/ILogger.h"
#include "SpdLogOutput.h"

namespace Wayfinder
{
    // Implementation of ILogMessage using spdlog
    class SpdLogMessage : public ILogMessage
    {
    public:
        SpdLogMessage(const spdlog::details::log_msg& msg)
            : m_level(ConvertLevel(msg.level)),
              m_loggerName(msg.logger_name.data(), msg.logger_name.size()),
              m_payload(msg.payload.data(), msg.payload.size()),
              m_timestamp(msg.time.time_since_epoch().count() / 1000000000.0) // Convert to seconds
        {
        }

        // ILogMessage implementation
        LogVerbosity GetVerbosity() const override { return m_level; }
        const std::string& GetLoggerName() const override { return m_loggerName; }
        const std::string& GetPayload() const override { return m_payload; }
        double GetTimestamp() const override { return m_timestamp; }

        // Convert spdlog level to Wayfinder level
        static LogVerbosity ConvertLevel(const spdlog::level::level_enum level)
        {
            switch (level)
            {
            case spdlog::level::trace:
                return LogVerbosity::VeryVerbose;
            case spdlog::level::debug:
                return LogVerbosity::Verbose;
            case spdlog::level::info:
                return LogVerbosity::Info;
            case spdlog::level::warn:
                return LogVerbosity::Warning;
            case spdlog::level::err:
                return LogVerbosity::Error;
            case spdlog::level::critical:
                return LogVerbosity::Fatal;
            default:
                return LogVerbosity::Info;
            }
        }

        // Convert Wayfinder level to spdlog level
        static spdlog::level::level_enum ConvertLevel(const LogVerbosity level)
        {
            switch (level)
            {
            case LogVerbosity::Fatal:
                return spdlog::level::critical;
            case LogVerbosity::Error:
                return spdlog::level::err;
            case LogVerbosity::Warning:
                return spdlog::level::warn;
            case LogVerbosity::Info:
                return spdlog::level::info;
            case LogVerbosity::Verbose:
                return spdlog::level::debug;
            case LogVerbosity::VeryVerbose:
                return spdlog::level::trace;
            default:
                return spdlog::level::info;
            }
        }

    private:
        LogVerbosity m_level;
        std::string m_loggerName;
        std::string m_payload;
        double m_timestamp;
    };

    // Implementation of ILogger using spdlog
    class SpdLogger : public ILogger
    {
    public:
        SpdLogger(const std::string& name, const LogVerbosity defaultVerbosity = LogVerbosity::Info)
            : m_name(name), m_verbosity(defaultVerbosity)
        {
            m_logger = std::make_shared<spdlog::logger>(name);
            m_logger->set_level(SpdLogMessage::ConvertLevel(defaultVerbosity));
            // m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        }

        virtual ~SpdLogger() override = default;

        const std::string& GetName() const override { return m_name; }
        LogVerbosity GetVerbosity() const override { return m_verbosity; }

        void SetVerbosity(const LogVerbosity level) override
        {
            m_verbosity = level;
            m_logger->set_level(SpdLogMessage::ConvertLevel(level));
        }

        void AddOutput(const std::shared_ptr<ILogOutput> output) override
        {
            m_outputs.push_back(output);

            // If it's an spdlog output, add its sink to the logger
            auto spdOutput = std::dynamic_pointer_cast<SpdLogOutput>(output);
            if (spdOutput)
            {
                m_logger->sinks().push_back(spdOutput->GetSink());
                // Maybe update the logger's level based on sinks?
                // m_logger->flush_on(m_logger->level()); // Example
            }
        }

        void ClearOutputs() override
        {
            m_outputs.clear();
            m_logger->sinks().clear();
        }

        const std::vector<std::shared_ptr<ILogOutput>>& GetOutputs() const override
        {
            return m_outputs;
        }

        void Log(const LogVerbosity level, const std::string_view message) override
        {
            m_logger->log(SpdLogMessage::ConvertLevel(level), message);
        }

    protected:
        void LogFormatted(const LogVerbosity level, const std::string_view format, const std::format_args args) override
        {
            const spdlog::level::level_enum spdlog_level = SpdLogMessage::ConvertLevel(level);
            if (m_logger->should_log(spdlog_level))
            {
                const std::string formatted_message = std::vformat(format, args);
                m_logger->log(spdlog_level, formatted_message);
            }
        }

    public:
        std::shared_ptr<spdlog::logger> GetSpdLogger() const { return m_logger; }

    private:
        std::string m_name;
        LogVerbosity m_verbosity;
        std::shared_ptr<spdlog::logger> m_logger;
        std::vector<std::shared_ptr<ILogOutput>> m_outputs;
    };

} // namespace Wayfinder
