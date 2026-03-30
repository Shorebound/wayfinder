#include "Log.h"
#include "core/TransparentStringHash.h"
#include "core/logging/spdlog/SpdLogManager.h"
#include "core/logging/spdlog/SpdLogOutput.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Wayfinder
{
    namespace
    {
        using LoggerMap = std::unordered_map<std::string, std::shared_ptr<ILogger>, TransparentStringHash, std::equal_to<>>;

        LoggerMap& GetLoggers()
        {
            static LoggerMap sLoggers;
            return sLoggers;
        }

        LogVerbosity& GetGlobalVerbosity()
        {
            static LogVerbosity sVerbosity = LogVerbosity::Info;
            return sVerbosity;
        }

        LogConfig& GetConfigStorage()
        {
            static LogConfig sConfig;
            return sConfig;
        }

        std::uint64_t& GetGeneration()
        {
            static std::uint64_t sGeneration = 1;
            return sGeneration;
        }

        /// Rebuild a logger's outputs to match the current config.
        void RebuildOutputs(ILogger& logger, const LogConfig& config)
        {
            logger.ClearOutputs();

            if (config.IsOutputEnabled(LogOutputType::Console))
            {
                auto output = CreateConsoleOutput();
                output->SetPattern("%^[%T] %n: %v%$");
                logger.AddOutput(std::move(output));
            }

            if (config.IsOutputEnabled(LogOutputType::File))
            {
                auto output = CreateFileOutput(config.FileOutputConfig);
                output->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
                logger.AddOutput(std::move(output));
            }
        }
    } // namespace

    using std::literals::string_view_literals::operator""sv;

    const LogCategoryHandle LogEngine{"Engine"sv};
    const LogCategoryHandle LogRenderer{"Renderer"sv};
    const LogCategoryHandle LogInput{"Input"sv};
    const LogCategoryHandle LogAudio{"Audio"sv};
    const LogCategoryHandle LogAssets{"Assets"sv};
    const LogCategoryHandle LogPhysics{"Physics"sv};
    const LogCategoryHandle LogGame{"Game"sv};
    const LogCategoryHandle LogScene{"Scene"sv};

    // ── LogCategoryHandle ───────────────────────────────────────────────

    ILogger& LogCategoryHandle::Get() const
    {
        const auto currentGeneration = GetGeneration();
        if (m_cached != nullptr && m_generation == currentGeneration)
        {
            return *m_cached;
        }

        m_cached = &Log::GetOrCreateLogger(m_name);
        m_generation = currentGeneration;
        return *m_cached;
    }

    // ── Log namespace ───────────────────────────────────────────────────

    void Log::Initialise()
    {
        SpdLogManager::Initialise();
        /// Re-apply stored verbosity to loggers created before Initialise() ran.
        SetGlobalVerbosity(GetGlobalVerbosity());
    }

    void Log::Shutdown()
    {
        GetLoggers().clear();
        ++GetGeneration();
        SpdLogManager::Shutdown();
    }

    ILogger& Log::GetOrCreateLogger(std::string_view name, LogVerbosity defaultVerbosity)
    {
        auto& loggers = GetLoggers();

        if (const auto it = loggers.find(name); it != loggers.end())
        {
            return *it->second;
        }

        const LogVerbosity verbosity = GetGlobalVerbosity() != LogVerbosity::Info ? GetGlobalVerbosity() : defaultVerbosity;
        std::string key{name};
        auto logger = CreateLogger(key, verbosity);
        RebuildOutputs(*logger, GetConfigStorage());
        auto* raw = logger.get();
        loggers.emplace(std::move(key), std::move(logger));
        return *raw;
    }

    void Log::SetGlobalVerbosity(LogVerbosity level)
    {
        GetGlobalVerbosity() = level;
        for (auto& [name, logger] : GetLoggers())
        {
            logger->SetVerbosity(level);
        }
    }

    const LogConfig& Log::GetConfig()
    {
        return GetConfigStorage();
    }

    void Log::SetConfig(const LogConfig& config)
    {
        GetConfigStorage() = config;
        for (auto& [name, logger] : GetLoggers())
        {
            RebuildOutputs(*logger, config);
        }
    }

} // namespace Wayfinder
