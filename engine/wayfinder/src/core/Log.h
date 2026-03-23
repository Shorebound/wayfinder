#pragma once
#include "core/logging/ILogger.h"
#include "core/logging/LogTypes.h"
#include "wayfinder_exports.h"

#include <cstdint>
#include <memory>
#include <string>

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

    class WAYFINDER_API LogCategoryHandle
    {
    public:
        constexpr explicit LogCategoryHandle(const char* name) noexcept : m_name(name) {}

        LogCategory& Get() const;
        const std::string& GetName() const;
        std::shared_ptr<ILogger> GetLogger() const;
        LogVerbosity GetVerbosity() const;
        operator LogCategory&() const;

    private:
        const char* m_name;
        mutable LogCategory* m_cached = nullptr;
        mutable std::uint64_t m_generation = 0;
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
        static const LogConfig& GetConfig();
        static void SetConfig(const LogConfig& config);

        // Convenience methods for output configuration
        static void EnableOutput(LogOutputType output, bool enable = true);
        static bool IsOutputEnabled(LogOutputType output);

        // File output specific configuration
        static void SetLogFilePath(const std::string& path);
        static void SetLogFileRotationSize(size_t maxSize);
        static void SetLogFileMaxFiles(size_t maxFiles);
    };

    // Declare common categories
    extern WAYFINDER_API const LogCategoryHandle LogEngine;
    extern WAYFINDER_API const LogCategoryHandle LogRenderer;
    extern WAYFINDER_API const LogCategoryHandle LogInput;
    extern WAYFINDER_API const LogCategoryHandle LogAudio;
    extern WAYFINDER_API const LogCategoryHandle LogAssets;
    extern WAYFINDER_API const LogCategoryHandle LogPhysics;
    extern WAYFINDER_API const LogCategoryHandle LogGame;
    extern WAYFINDER_API const LogCategoryHandle LogScene;

    inline LogCategory& ResolveLogCategory(LogCategory& category)
    {
        return category;
    }

    inline LogCategory& ResolveLogCategory(const LogCategoryHandle& category)
    {
        return category.Get();
    }

} // namespace Wayfinder

// Logging macros
#define WAYFINDER_LOG(category, verbosity, ...)                                                                                                                                                                            \
    do                                                                                                                                                                                                                     \
    {                                                                                                                                                                                                                      \
        auto& wayfinderLogCategory = Wayfinder::ResolveLogCategory(category);                                                                                                                                              \
        if (verbosity <= wayfinderLogCategory.GetVerbosity())                                                                                                                                                              \
        {                                                                                                                                                                                                                  \
            wayfinderLogCategory.GetLogger()->LogFormat(verbosity, __VA_ARGS__);                                                                                                                                           \
        }                                                                                                                                                                                                                  \
    } while (false)

#define WAYFINDER_VERBOSE(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Verbose, __VA_ARGS__)
#define WAYFINDER_INFO(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Info, __VA_ARGS__)
#define WAYFINDER_WARNING(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Warning, __VA_ARGS__)
#define WAYFINDER_ERROR(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Error, __VA_ARGS__)
#define WAYFINDER_FATAL(category, ...) WAYFINDER_LOG(category, Wayfinder::LogVerbosity::Fatal, __VA_ARGS__)
