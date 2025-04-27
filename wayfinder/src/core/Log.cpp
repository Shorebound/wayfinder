#include "core/Log.h"
#include <spdlog/sinks/stdout_color_sinks.h>
#include "spdlog/sinks/rotating_file_sink.h" 
namespace Wayfinder
{
    std::unordered_map<std::string, std::unique_ptr<LogCategory>> Log::s_categories;
    LogVerbosity Log::s_globalVerbosity = LogVerbosity::Info;

    // Define common categories
    LogCategory &LogEngine = Log::CreateCategory("Engine");
    LogCategory &LogRenderer = Log::CreateCategory("Renderer");
    LogCategory &LogInput = Log::CreateCategory("Input");
    LogCategory &LogAudio = Log::CreateCategory("Audio");
    LogCategory &LogPhysics = Log::CreateCategory("Physics");
    LogCategory &LogGame = Log::CreateCategory("Game");

    LogCategory::LogCategory(const std::string& name, LogVerbosity defaultVerbosity)
        : m_name(name)
        , m_verbosity(defaultVerbosity)
    {
        // Create sinks
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        auto raylib_sink = std::make_shared<raylib_sink_mt>();
        
        // Set patterns for each sink
        console_sink->set_pattern("%^[%T] %n: %v%$");
        raylib_sink->set_pattern("[%n] %v");
        
        // Create logger with multiple sinks
        m_logger = std::make_shared<spdlog::logger>(name, spdlog::sinks_init_list{
            console_sink, raylib_sink
        });
        
        // Set initial level
        SetVerbosity(defaultVerbosity);
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

    LogCategory& Log::CreateCategory(const std::string &name, LogVerbosity defaultVerbosity)
    {
        auto it = s_categories.find(name);
        if (it != s_categories.end())
        {
            return *it->second;
        }

        auto category = std::make_unique<LogCategory>(name, defaultVerbosity);
        auto &ref = *category;
        s_categories[name] = std::move(category);
        return ref;
    }

    LogCategory& Log::GetCategory(const std::string &name)
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
        for (auto &[name, category] : s_categories)
        {
            category->SetVerbosity(level);
        }
    }

} // namespace Wayfinder

