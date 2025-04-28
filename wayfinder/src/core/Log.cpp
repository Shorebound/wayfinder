#include "core/Log.h"
#include "core/logging/spdlog/SpdLogManager.h"
#include "core/logging/spdlog/SpdLogOutput.h"
#include "core/logging/spdlog/SpdLogger.h"

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
    LogCategory& LogScene = Log::CreateCategory("Scene");

    LogCategory::LogCategory(const std::string& name, LogVerbosity defaultVerbosity)
        : m_name(name), m_verbosity(defaultVerbosity)
    {
        // Create logger with empty output list
        m_logger = CreateLogger(name, defaultVerbosity);

        // Update outputs based on current configuration
        UpdateOutputs();
    }

    void LogCategory::UpdateOutputs()
    {
        // Clear existing outputs
        m_logger->ClearOutputs();

        const LogConfig& config = Log::GetConfig();

        // Add console output if enabled
        if (config.IsOutputEnabled(LogOutputType::Console))
        {
            auto output = CreateConsoleOutput();
            output->SetPattern("%^[%T] %n: %v%$");
            m_logger->AddOutput(output);
        }

        // Add file output if enabled
        if (config.IsOutputEnabled(LogOutputType::File))
        {
            auto output = CreateFileOutput(config.fileOutputConfig);
            output->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
            m_logger->AddOutput(output);
        }

        // Add raylib output if enabled
        if (config.IsOutputEnabled(LogOutputType::Raylib))
        {
            auto output = CreateRaylibOutput();
            output->SetPattern("[%n] %v");
            m_logger->AddOutput(output);
        }
    }

    void LogCategory::SetVerbosity(LogVerbosity level)
    {
        m_verbosity = level;
        m_logger->SetVerbosity(level);
    }

    void Log::Init()
    {
        // Initialize the spdlog manager
        SpdLogManager::Initialize();

        // Initialize all predefined categories
        // They're already created as static globals, but we might want to do additional setup
        SetGlobalVerbosity(LogVerbosity::Info);
    }

    void Log::Shutdown()
    {
        s_categories.clear();
        SpdLogManager::Shutdown();
    }

    void Log::SetConfig(const LogConfig& config)
    {
        s_config = config;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateOutputs();
        }
    }

    void Log::EnableOutput(LogOutputType output, bool enable)
    {
        s_config.EnableOutput(output, enable);

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateOutputs();
        }
    }

    bool Log::IsOutputEnabled(LogOutputType output)
    {
        return s_config.IsOutputEnabled(output);
    }

    void Log::SetLogFilePath(const std::string& path)
    {
        s_config.fileOutputConfig.filePath = path;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateOutputs();
        }
    }

    void Log::SetLogFileRotationSize(size_t maxSize)
    {
        s_config.fileOutputConfig.maxFileSize = maxSize;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateOutputs();
        }
    }

    void Log::SetLogFileMaxFiles(size_t maxFiles)
    {
        s_config.fileOutputConfig.maxFiles = maxFiles;

        // Update all existing categories with new configuration
        for (auto& [name, category] : s_categories)
        {
            category->UpdateOutputs();
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
