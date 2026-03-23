#include "Log.h"
#include "core/logging/spdlog/SpdLogManager.h"
#include "core/logging/spdlog/SpdLogOutput.h"
#include "core/logging/spdlog/SpdLogger.h"

#include <cstdint>

namespace Wayfinder
{
    namespace
    {
        using LogCategoryMap = std::unordered_map<std::string, std::unique_ptr<LogCategory>>;

        LogCategoryMap& GetCategories()
        {
            static LogCategoryMap sCategories;
            return sCategories;
        }

        LogVerbosity& GetGlobalVerbosityStorage()
        {
            static LogVerbosity sGlobalVerbosity = LogVerbosity::Info;
            return sGlobalVerbosity;
        }

        LogConfig& GetConfigStorage()
        {
            static LogConfig sConfig;
            return sConfig;
        }

        std::uint64_t& GetCategoryGeneration()
        {
            static std::uint64_t sGeneration = 1;
            return sGeneration;
        }

    } // namespace

    const LogCategoryHandle LogEngine{"Engine"};
    const LogCategoryHandle LogRenderer{"Renderer"};
    const LogCategoryHandle LogInput{"Input"};
    const LogCategoryHandle LogAudio{"Audio"};
    const LogCategoryHandle LogAssets{"Assets"};
    const LogCategoryHandle LogPhysics{"Physics"};
    const LogCategoryHandle LogGame{"Game"};
    const LogCategoryHandle LogScene{"Scene"};

    LogCategory& LogCategoryHandle::Get() const
    {
        const std::uint64_t currentGeneration = GetCategoryGeneration();
        if (m_cached != nullptr && m_generation == currentGeneration)
        {
            return *m_cached;
        }

        m_cached = &Log::GetCategory(m_name);
        m_generation = currentGeneration;
        return *m_cached;
    }

    const std::string& LogCategoryHandle::GetName() const
    {
        return Get().GetName();
    }

    std::shared_ptr<ILogger> LogCategoryHandle::GetLogger() const
    {
        return Get().GetLogger();
    }

    LogVerbosity LogCategoryHandle::GetVerbosity() const
    {
        return Get().GetVerbosity();
    }

    LogCategoryHandle::operator LogCategory&() const
    {
        return Get();
    }

    LogCategory::LogCategory(const std::string& name, LogVerbosity defaultVerbosity) : m_name(name), m_verbosity(defaultVerbosity)
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
            auto output = CreateFileOutput(config.FileOutputConfig);
            output->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
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
        /// Initialise the spdlog manager.
        SpdLogManager::Initialise();

        /// Initialise all predefined categories.
        /// They're already created as static globals, but we might want to do additional setup.
        SetGlobalVerbosity(GetGlobalVerbosityStorage());
    }

    void Log::Shutdown()
    {
        GetCategories().clear();
        ++GetCategoryGeneration();
        SpdLogManager::Shutdown();
    }

    const LogConfig& Log::GetConfig()
    {
        return GetConfigStorage();
    }

    void Log::SetConfig(const LogConfig& config)
    {
        auto& storedConfig = GetConfigStorage();
        storedConfig = config;

        // Update all existing categories with new configuration
        for (auto& [name, category] : GetCategories())
        {
            category->UpdateOutputs();
        }
    }

    void Log::EnableOutput(LogOutputType output, bool enable)
    {
        auto& config = GetConfigStorage();
        config.EnableOutput(output, enable);

        // Update all existing categories with new configuration
        for (auto& [name, category] : GetCategories())
        {
            category->UpdateOutputs();
        }
    }

    bool Log::IsOutputEnabled(LogOutputType output)
    {
        return GetConfigStorage().IsOutputEnabled(output);
    }

    void Log::SetLogFilePath(const std::string& path)
    {
        auto& config = GetConfigStorage();
        config.FileOutputConfig.FilePath = path;

        // Update all existing categories with new configuration
        for (auto& [name, category] : GetCategories())
        {
            category->UpdateOutputs();
        }
    }

    void Log::SetLogFileRotationSize(size_t maxSize)
    {
        auto& config = GetConfigStorage();
        config.FileOutputConfig.MaxFileSize = maxSize;

        // Update all existing categories with new configuration
        for (auto& [name, category] : GetCategories())
        {
            category->UpdateOutputs();
        }
    }

    void Log::SetLogFileMaxFiles(size_t maxFiles)
    {
        auto& config = GetConfigStorage();
        config.FileOutputConfig.MaxFiles = maxFiles;

        // Update all existing categories with new configuration
        for (auto& [name, category] : GetCategories())
        {
            category->UpdateOutputs();
        }
    }

    LogCategory& Log::CreateCategory(const std::string& name, LogVerbosity defaultVerbosity)
    {
        auto& categories = GetCategories();
        auto it = categories.find(name);
        if (it != categories.end())
        {
            return *it->second;
        }

        auto category = std::make_unique<LogCategory>(name, defaultVerbosity);
        auto* categoryPtr = category.get();
        categories.emplace(name, std::move(category));
        return *categoryPtr;
    }

    LogCategory& Log::GetCategory(const std::string& name)
    {
        auto& categories = GetCategories();
        auto it = categories.find(name);
        if (it != categories.end())
        {
            return *it->second;
        }
        return CreateCategory(name);
    }

    void Log::SetGlobalVerbosity(LogVerbosity level)
    {
        GetGlobalVerbosityStorage() = level;
        for (auto& [name, category] : GetCategories())
        {
            category->SetVerbosity(level);
        }
    }

} // namespace Wayfinder
