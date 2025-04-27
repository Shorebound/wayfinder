#include "core/Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include "spdlog/sinks/rotating_file_sink.h"
#include <filesystem>

namespace Wayfinder
{
    std::unordered_map<std::string, std::unique_ptr<LogCategory>> Log::s_categories;
    LogVerbosity Log::s_globalVerbosity = LogVerbosity::Info;
    LogConfig Log::s_config;

    // Define common categories
    LogCategory& LogEngine = Log::CreateCategory("Engine");
    LogCategory& LogRenderer = Log::CreateCategory("Renderer");
    LogCategory& LogInput = Log::CreateCategory("Input");
    LogCategory& LogAudio = Log::CreateCategory("Audio");
    LogCategory& LogPhysics = Log::CreateCategory("Physics");
    LogCategory& LogGame = Log::CreateCategory("Game");

    LogCategory::LogCategory(const std::string& name, LogVerbosity defaultVerbosity)
        : m_name(name), m_verbosity(defaultVerbosity)
    {
        // Create logger with empty sink list (will be populated in UpdateSinks)
        m_logger = std::make_shared<spdlog::logger>(name);

        // Update sinks based on current configuration
        UpdateSinks();

        // Set initial level
        SetVerbosity(defaultVerbosity);
    }

    void LogCategory::UpdateSinks()
    {
        // Clear existing sinks
        m_logger->sinks().clear();

        const LogConfig& config = Log::GetConfig();

        // Add console output if enabled
        if (config.IsOutputEnabled(LogOutputType::Console))
        {
            auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
            console_sink->set_pattern("%^[%T] %n: %v%$");
            m_logger->sinks().push_back(console_sink);
        }

        // Add file output if enabled
        if (config.IsOutputEnabled(LogOutputType::File))
        {
            // Ensure directory exists
            std::filesystem::path logPath(config.fileSinkConfig.filePath);
            std::filesystem::create_directories(logPath.parent_path());

            auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
                config.fileSinkConfig.filePath,
                config.fileSinkConfig.maxFileSize,
                config.fileSinkConfig.maxFiles);
            file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
            m_logger->sinks().push_back(file_sink);
        }

        // Add raylib output if enabled
        if (config.IsOutputEnabled(LogOutputType::Raylib))
        {
            auto raylib_sink = std::make_shared<raylib_sink_mt>();
            raylib_sink->set_pattern("[%n] %v");
            m_logger->sinks().push_back(raylib_sink);
        }
    }

    void LogCategory::SetVerbosity(LogVerbosity level)
    {
        m_verbosity = level;

        // Map our verbosity levels to spdlog levels
        spdlog::level::level_enum spdlogLevel;
        switch (level)
        {
        case LogVerbosity::Fatal:
            spdlogLevel = spdlog::level::critical;
            break;
        case LogVerbosity::Error:
            spdlogLevel = spdlog::level::err;
            break;
        case LogVerbosity::Warning:
            spdlogLevel = spdlog::level::warn;
            break;
        case LogVerbosity::Info:
            spdlogLevel = spdlog::level::info;
            break;
        case LogVerbosity::Verbose:
            spdlogLevel = spdlog::level::debug;
            break;
        case LogVerbosity::VeryVerbose:
            spdlogLevel = spdlog::level::trace;
            break;
        }
        m_logger->set_level(spdlogLevel);
    }

    void Log::Init()
    {
        // Set up default pattern
        spdlog::set_pattern("%^[%T] %n: %v%$");

        // Initialize all predefined categories
        // They're already created as static globals, but we might want to do additional setup
        SetGlobalVerbosity(LogVerbosity::Info);
    }

    void Log::Shutdown()
    {
        s_categories.clear();
        spdlog::shutdown();
    }

    void Log::SetConfig(const LogConfig& config)
    {
        s_config = config;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateSinks();
        }
    }

    void Log::EnableOutput(LogOutputType output, bool enable)
    {
        s_config.EnableOutput(output, enable);

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateSinks();
        }
    }

    bool Log::IsOutputEnabled(LogOutputType output)
    {
        return s_config.IsOutputEnabled(output);
    }

    void Log::SetLogFilePath(const std::string& path)
    {
        s_config.fileSinkConfig.filePath = path;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateSinks();
        }
    }

    void Log::SetLogFileRotationSize(size_t maxSize)
    {
        s_config.fileSinkConfig.maxFileSize = maxSize;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateSinks();
        }
    }

    void Log::SetLogFileMaxFiles(size_t maxFiles)
    {
        s_config.fileSinkConfig.maxFiles = maxFiles;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateSinks();
        }
    }

    LogCategory& Log::CreateCategory(const std::string& name, LogVerbosity defaultVerbosity)
    {
        auto it = s_categories.find(name);
        if (it != s_categories.end())
        {
            return *it->second;
        }

        auto category = std::make_unique<LogCategory>(name, defaultVerbosity);
        auto& ref = *category;
        s_categories[name] = std::move(category);
        return ref;
    }

    LogCategory& Log::GetCategory(const std::string& name)
    {
        auto it = s_categories.find(name);
        if (it != s_categories.end())
        {
            return *it->second;
        }
        return CreateCategory(name);
    }

    void Log::SetGlobalVerbosity(LogVerbosity level)
    {
        s_globalVerbosity = level;
        for (auto& [name, category] : s_categories)
        {
            category->SetVerbosity(level);
        }
    }

} // namespace Wayfinder
