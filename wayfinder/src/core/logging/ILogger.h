#pragma once
#include "ILogOutput.h"
#include "LogTypes.h"
#include <string>
#include <string_view>
#include <utility>

#include <memory>
#include <vector>
#include <format>

namespace Wayfinder
{
    // Interface for loggers
    class ILogger
    {
    public:
        virtual ~ILogger() = default;

        virtual const std::string& GetName() const = 0;
        virtual LogVerbosity GetVerbosity() const = 0;
        virtual void SetVerbosity(LogVerbosity level) = 0;
        virtual void AddOutput(std::shared_ptr<ILogOutput> output) = 0;
        virtual void ClearOutputs() = 0;
        virtual const std::vector<std::shared_ptr<ILogOutput>>& GetOutputs() const = 0;

        virtual void Log(LogVerbosity level, const std::string_view message) = 0;

        // Public template interface - uses std::format concepts
        // Note: C++23 allows std::format_string<Args...> for compile-time checks,
        // but std::string_view is safer for the public template interface here.
        template <typename... Args>
        void LogFormat(LogVerbosity level, std::string_view format, Args&& ...args)
        {
            if (level <= GetVerbosity())
            {
                // Use std::make_format_args to capture arguments
                LogFormatted(level, format,
                             std::make_format_args(std::forward<Args>(args)...));
            }
        }

    protected:
        virtual void LogFormatted(LogVerbosity level, std::string_view format, std::format_args args) = 0;

    public:
        template <typename... Args>
        void Fatal(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::Fatal, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void Error(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::Error, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void Warning(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::Warning, format,
                      std::forward<Args>(args)...);
        }
        template <typename... Args>
        void Info(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::Info, format, std::forward<Args>(args)...);
        }
        template <typename... Args>
        void Verbose(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::Verbose, format,
                      std::forward<Args>(args)...);
        }
        template <typename... Args>
        void VeryVerbose(std::string_view format, Args&& ...args)
        {
            LogFormat(LogVerbosity::VeryVerbose, format,
                      std::forward<Args>(args)...);
        }
    };

    // Factory function for creating loggers
    std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info);

} // namespace Wayfinder
