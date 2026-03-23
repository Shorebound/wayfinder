#pragma once
#include "SpdLogger.h"
#include "core/logging/ILogger.h"

#include <ranges>
#include <unordered_map>

namespace Wayfinder
{
    // Implementation of log manager using spdlog
    class SpdLogManager
    {
    public:
        static void Initialise()
        {
            // Set up default pattern
            spdlog::set_pattern("%^[%T] %n: %v%$");
        }

        static void Shutdown()
        {
            GetLoggers().clear();
            spdlog::shutdown();
        }

        static std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info)
        {
            auto& loggers = GetLoggers();
            if (const auto it = loggers.find(name); it != loggers.end())
            {
                return it->second;
            }

            auto logger = std::make_shared<SpdLogger>(name, defaultVerbosity);
            loggers[name] = logger;
            return logger;
        }

        static std::shared_ptr<ILogger> GetLogger(const std::string& name)
        {
            auto& loggers = GetLoggers();
            if (const auto it = loggers.find(name); it != loggers.end())
            {
                return it->second;
            }
            return CreateLogger(name);
        }

        static void UpdateLoggerOutputs(const LogConfig& config)
        {
            for (const auto& logger : GetLoggers() | std::views::values)
            {
                UpdateLoggerOutputs(logger, config);
            }
        }

        static void UpdateLoggerOutputs(const std::shared_ptr<ILogger>& logger, const LogConfig& config)
        {
            logger->ClearOutputs();

            // Add console output if enabled
            if (config.IsOutputEnabled(LogOutputType::Console))
            {
                const auto output = CreateConsoleOutput();
                output->SetPattern("%^[%T] %n: %v%$");
                logger->AddOutput(output);
            }

            // Add file output if enabled
            if (config.IsOutputEnabled(LogOutputType::File))
            {
                const auto output = CreateFileOutput(config.FileOutputConfig);
                output->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
                logger->AddOutput(output);
            }
        }

    private:
        static std::unordered_map<std::string, std::shared_ptr<ILogger>>& GetLoggers()
        {
            static std::unordered_map<std::string, std::shared_ptr<ILogger>> sLoggers;
            return sLoggers;
        }
    };

} // namespace Wayfinder
