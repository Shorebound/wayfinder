#pragma once
#include "core/logging/ILogger.h"
#include "core/logging/LogTypes.h"
#include "wayfinder_exports.h"

#include <memory>
#include <string>
#include <unordered_map>

namespace Wayfinder
{

    class WAYFINDER_API LogCategory
    {
    public:
        LogCategory(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info);

        // Rebuild outputs based on current configuration
        void UpdateOutputs();

        const std::string& GetName() const
        {
            return m_name;
        }
        std::shared_ptr<ILogger> GetLogger()
        {
            return m_logger;
        }
        LogVerbosity GetVerbosity() const
        {
            return m_verbosity;
        }
        void SetVerbosity(LogVerbosity level);

    private:
        std::string m_name;
        std::shared_ptr<ILogger> m_logger;
        LogVerbosity m_verbosity;
    };

    class WAYFINDER_API Log
    {
    public:
        static void Init();
        static void Shutdown();

        static LogCategory& CreateCategory(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info);
        static LogCategory& GetCategory(const std::string& name);

        // Global verbosity control
        static void SetGlobalVerbosity(LogVerbosity level);

        // Configuration methods
        static const LogConfig& GetConfig()
        {
            return s_config;
        }
        static void SetConfig(const LogConfig& config);

        // Convenience methods for output configuration
        static void EnableOutput(LogOutputType output, bool enable = true);
        static bool IsOutputEnabled(LogOutputType output);

        // File output specific configuration
        static void SetLogFilePath(const std::string& path);
        static void SetLogFileRotationSize(size_t maxSize);
        static void SetLogFileMaxFiles(size_t maxFiles);

    private:
        static std::unordered_map<std::string, std::unique_ptr<LogCategory>> s_categories;
        static LogVerbosity s_globalVerbosity;
        static LogConfig s_config;
    };

    // Declare common categories
    extern WAYFINDER_API LogCategory& LogEngine;
    extern WAYFINDER_API LogCategory& LogRenderer;
    extern WAYFINDER_API LogCategory& LogInput;
    extern WAYFINDER_API LogCategory& LogAudio;
    extern WAYFINDER_API LogCategory& LogAssets;
    extern WAYFINDER_API LogCategory& LogPhysics;
    extern WAYFINDER_API LogCategory& LogGame;
    extern WAYFINDER_API LogCategory& LogScene;

} // namespace Wayfinder

// Logging macros
#define WAYFINDER_LOG(category, verbosity, ...)                                                                                                                                                        \
    if (verbosity <= category.GetVerbosity())                                                                                                                                                          \
    category.GetLogger()->LogFormat(verbosity, __VA_ARGS__)

#define WAYFINDER_VERBOSE(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Verbose, __VA_ARGS__)
#define WAYFINDER_INFO(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Info, __VA_ARGS__)
#define WAYFINDER_WARNING(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Warning, __VA_ARGS__)
#define WAYFINDER_ERROR(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Error, __VA_ARGS__)
#define WAYFINDER_FATAL(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Fatal, __VA_ARGS__)
