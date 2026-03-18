#pragma once
#include "LogTypes.h"


namespace Wayfinder
{
    // Forward declarations
    class ILogMessage;

    // Interface for log outputs (formerly "sinks")
    class ILogOutput
    {
    public:
        virtual ~ILogOutput() = default;

        // Process a log message
        virtual void ProcessMessage(const ILogMessage& message) = 0;

        // Flush any buffered messages
        virtual void Flush() = 0;

        // Set the pattern for formatting messages
        virtual void SetPattern(const std::string& pattern) = 0;
    };

    // Interface for log messages
    class ILogMessage
    {
    public:
        virtual ~ILogMessage() = default;

        // Get the log level
        virtual LogVerbosity GetVerbosity() const = 0;

        // Get the logger name
        virtual const std::string& GetLoggerName() const = 0;

        // Get the message payload
        virtual const std::string& GetPayload() const = 0;

        // Get the timestamp
        virtual double GetTimestamp() const = 0;
    };

    // Factory functions for creating outputs
    std::shared_ptr<ILogOutput> CreateConsoleOutput();
    std::shared_ptr<ILogOutput> CreateFileOutput(const LogFileConfig& config);

} // namespace Wayfinder
