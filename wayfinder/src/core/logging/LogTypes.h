#pragma once
#include <string>

namespace Wayfinder
{
    // Log verbosity levels
    enum class LogVerbosity
    {
        Fatal,      // The program cannot continue. Logs and crashes.
        Error,      // The feature is disabled. Logs and continues.
        Warning,    // Something is wrong but the feature can still work.
        Info,       // Standard log messages
        Verbose,    // Detailed information for debugging
        VeryVerbose // Very detailed information for debugging specific issues
    };

    // Log output types (formerly "sinks")
    enum class LogOutputType
    {
        None = 0,
        Console = 1 << 0, // 0b0001
        File = 1 << 1,    // 0b0010
        Raylib = 1 << 2,  // 0b0100

        Count = 3 // Keep track of how many flags we have, excluding None
    };

    // Add operator overloads for enum class bitwise operations
    inline LogOutputType operator|(LogOutputType a, LogOutputType b)
    {
        return static_cast<LogOutputType>(
            static_cast<std::underlying_type_t<LogOutputType>>(a) |
            static_cast<std::underlying_type_t<LogOutputType>>(b));
    }

    inline LogOutputType operator&(LogOutputType a, LogOutputType b)
    {
        return static_cast<LogOutputType>(
            static_cast<std::underlying_type_t<LogOutputType>>(a) &
            static_cast<std::underlying_type_t<LogOutputType>>(b));
    }

    inline LogOutputType& operator|=(LogOutputType& a, LogOutputType b)
    {
        a = a | b;
        return a;
    }

    inline LogOutputType& operator&=(LogOutputType& a, LogOutputType b)
    {
        a = a & b;
        return a;
    }

    // Configuration for file output
    struct LogFileConfig
    {
        std::string filePath = "logs/wayfinder.log"; // Default log file path
        size_t maxFileSize = 5 * 1024 * 1024;        // 5 MB default
        size_t maxFiles = 3;                         // Keep 3 rotated files by default
    };

    // Global logger configuration
    struct LogConfig
    {
        // Enabled outputs using the enum directly
        LogOutputType enabledOutputs = LogOutputType::Console;

        // File output specific configuration
        LogFileConfig fileOutputConfig;

        // Helper methods to enable/disable specific outputs
        void EnableOutput(LogOutputType output, bool enable = true)
        {
            if (enable)
            {
                enabledOutputs = enabledOutputs | output;
            }
            else
            {
                // Clear the specific bit using AND with inverted bits
                enabledOutputs = enabledOutputs & static_cast<LogOutputType>(
                                                     ~static_cast<std::underlying_type_t<LogOutputType>>(output));
            }
        }

        bool IsOutputEnabled(LogOutputType output) const
        {
            return static_cast<bool>(enabledOutputs & output);
        }
    };
} // namespace Wayfinder
