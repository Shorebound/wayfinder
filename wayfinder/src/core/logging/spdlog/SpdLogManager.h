#pragma once
#include "../ILogger.h"
#include "SpdLogger.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Wayfinder
{
    // Implementation of log manager using spdlog
    class SpdLogManager
    {
    public:
        static void Initialize()
        {
            // Set up default pattern
            spdlog::set_pattern("%^[%T] %n: %v%$");
        }

        static void Shutdown()
        {
            s_loggers.clear();
            spdlog::shutdown();
        }

        static std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info)
        {
            auto it = s_loggers.find(name);
            if (it != s_loggers.end())
            {
                return it->second;
            }

            auto logger = std::make_shared<SpdLogger>(name, defaultVerbosity);
            s_loggers[name] = logger;
            return logger;
        }

        static std::shared_ptr<ILogger> GetLogger(const std::string& name)
        {
            auto it = s_loggers.find(name);
            if (it != s_loggers.end())
            {
                return it->second;
            }
            return CreateLogger(name);
        }

        static void UpdateLoggerOutputs(const LogConfig& config)
        {
            for (auto& [name, logger] : s_loggers)
            {
                UpdateLoggerOutputs(logger, config);
            }
        }

        static void UpdateLoggerOutputs(std::shared_ptr<ILogger> logger, const LogConfig& config)
        {
            logger->ClearOutputs();

            // Add console output if enabled
            if (config.IsOutputEnabled(LogOutputType::Console))
            {
                auto output = CreateConsoleOutput();
                output->SetPattern("%^[%T] %n: %v%$");
                logger->AddOutput(output);
            }

            // Add file output if enabled
            if (config.IsOutputEnabled(LogOutputType::File))
            {
                auto output = CreateFileOutput(config.fileOutputConfig);
                output->SetPattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %n: %v");
                logger->AddOutput(output);
            }

            // Add raylib output if enabled
            if (config.IsOutputEnabled(LogOutputType::Raylib))
            {
                auto output = CreateRaylibOutput();
                output->SetPattern("[%n] %v");
                logger->AddOutput(output);
            }
        }

    private:
        static std::unordered_map<std::string, std::shared_ptr<ILogger>> s_loggers;
    };

    // Static member initialization
    std::unordered_map<std::string, std::shared_ptr<ILogger>> SpdLogManager::s_loggers;

} // namespace Wayfinder
