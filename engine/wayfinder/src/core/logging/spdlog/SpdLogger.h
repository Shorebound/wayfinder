#pragma once
#include "SpdLogOutput.h"
#include "core/logging/ILogger.h"

namespace Wayfinder
{

    /** @brief ILogger implementation backed by spdlog. */
    class SpdLogger : public ILogger
    {
    public:
        /// Convert Wayfinder verbosity to the equivalent spdlog level.
        static spdlog::level::level_enum ToSpdlog(LogVerbosity level)
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

        SpdLogger(const std::string& name, const LogVerbosity defaultVerbosity = LogVerbosity::Info) : m_name(name), m_verbosity(defaultVerbosity)
        {
            m_logger = std::make_shared<spdlog::logger>(name);
            m_logger->set_level(ToSpdlog(defaultVerbosity));
        }

        ~SpdLogger() override = default;

        const std::string& GetName() const override
        {
            return m_name;
        }

        LogVerbosity GetVerbosity() const override
        {
            return m_verbosity;
        }

        void SetVerbosity(const LogVerbosity level) override
        {
            m_verbosity = level;
            m_logger->set_level(ToSpdlog(level));
        }

        void AddOutput(const std::shared_ptr<ILogOutput> output) override
        {
            m_outputs.push_back(output);

            if (auto spdOutput = std::dynamic_pointer_cast<SpdLogOutput>(output))
            {
                m_logger->sinks().push_back(spdOutput->GetSink());
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
            m_logger->log(ToSpdlog(level), message);
        }

        void LogFormatted(const LogVerbosity level, const std::string_view format, const std::format_args args) override
        {
            const auto spdLevel = ToSpdlog(level);
            if (m_logger->should_log(spdLevel))
            {
                m_logger->log(spdLevel, std::vformat(format, args));
            }
        }

    private:
        std::string m_name;
        LogVerbosity m_verbosity;
        std::shared_ptr<spdlog::logger> m_logger;
        std::vector<std::shared_ptr<ILogOutput>> m_outputs;
    };

} // namespace Wayfinder
