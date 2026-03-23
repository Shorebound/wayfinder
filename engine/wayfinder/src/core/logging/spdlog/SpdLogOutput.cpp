#include "SpdLogOutput.h"
#include "SpdLogger.h"
#include <filesystem>

namespace Wayfinder
{
    // Factory function implementations
    std::shared_ptr<ILogOutput> CreateConsoleOutput()
    {
        return std::make_shared<SpdConsoleOutput>();
    }

    std::shared_ptr<ILogOutput> CreateFileOutput(const LogFileConfig& config)
    {
        return std::make_shared<SpdFileOutput>(config);
    }

    std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity)
    {
        return std::make_shared<SpdLogger>(name, defaultVerbosity);
    }
    void SpdLogOutput::ProcessMessage([[maybe_unused]] const ILogMessage& message)
    {
        // This is a no-op as spdlog handles messages directly through its logger
    }

    void SpdLogOutput::Flush()
    {
        if (m_sink)
        {
            m_sink->flush();
        }
    }

    void SpdLogOutput::SetPattern(const std::string& pattern)
    {
        if (m_sink)
        {
            m_sink->set_pattern(pattern);
        }
    }

    SpdConsoleOutput::SpdConsoleOutput() : SpdLogOutput(std::make_shared<spdlog::sinks::stdout_color_sink_mt>()) {}

    SpdFileOutput::SpdFileOutput(const LogFileConfig& config) : SpdLogOutput(nullptr)
    {
        // Ensure directory exists
        const std::filesystem::path logPath(config.FilePath);
        std::filesystem::create_directories(logPath.parent_path());

        m_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(config.FilePath, config.MaxFileSize, config.MaxFiles);
    }

} // namespace Wayfinder
