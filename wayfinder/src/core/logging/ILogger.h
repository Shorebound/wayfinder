#pragma once
#include "ILogOutput.h"
#include "LogTypes.h"
#include <string>
#include <memory>
#include <vector>


namespace Wayfinder
{
    // Interface for loggers
    class ILogger
    {
    public:
        virtual ~ILogger() = default;

        // Get the logger name
        virtual const std::string& GetName() const = 0;

        // Get the current verbosity level
        virtual LogVerbosity GetVerbosity() const = 0;

        // Set the verbosity level
        virtual void SetVerbosity(LogVerbosity level) = 0;

        // Add an output to this logger
        virtual void AddOutput(std::shared_ptr<ILogOutput> output) = 0;

        // Clear all outputs
        virtual void ClearOutputs() = 0;

        // Get all outputs
        virtual const std::vector<std::shared_ptr<ILogOutput>>& GetOutputs() const = 0;

        // Log a message with the specified verbosity
        virtual void Log(LogVerbosity level, const std::string& message) = 0;

        // Log a formatted message with the specified verbosity
        // This is a pure virtual method that must be implemented by derived classes
        virtual void LogFormat(LogVerbosity level, const std::string& format) = 0;

        // Variadic template version for formatting with arguments
        template<typename... Args>
        void LogFormat(LogVerbosity level, const std::string& format, Args&&... args)
        {
            // Implementations should override this to use their own formatting
            // This default implementation just forwards to the non-template version
            LogFormat(level, format);
        }

        // Convenience methods for different verbosity levels
        template<typename... Args>
        void Fatal(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::Fatal, format, std::forward<Args>(args)...); }

        template<typename... Args>
        void Error(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::Error, format, std::forward<Args>(args)...); }

        template<typename... Args>
        void Warning(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::Warning, format, std::forward<Args>(args)...); }

        template<typename... Args>
        void Info(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::Info, format, std::forward<Args>(args)...); }

        template<typename... Args>
        void Verbose(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::Verbose, format, std::forward<Args>(args)...); }

        template<typename... Args>
        void VeryVerbose(const std::string& format, Args&&... args) { LogFormat(LogVerbosity::VeryVerbose, format, std::forward<Args>(args)...); }
    };

    // Factory function for creating loggers
    std::shared_ptr<ILogger> CreateLogger(const std::string& name, LogVerbosity defaultVerbosity = LogVerbosity::Info);

} // namespace Wayfinder
